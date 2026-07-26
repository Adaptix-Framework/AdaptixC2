package server

import (
	"AdaptixServer/core/eventing"
	"bufio"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/rand/v2"
	"os/exec"
	"strings"
	"sync"

	"github.com/Adaptix-Framework/axc2/v2"
	"github.com/gorilla/websocket"
)

func (ts *Teamserver) TsAgentBuildSyncOnce(agentName string, config string, listenersName []string) ([]byte, string, error) {

	conf := adaptix.BuildProfile{
		BuilderId:   "",
		AgentConfig: config,
	}

	for _, listener := range listenersName {
		listenerWM, listenerProfile, err := ts.TsListenerGetProfile(listener)
		if err != nil {
			return nil, "", err
		}

		transport := adaptix.TransportProfile{
			Watermark: listenerWM,
			Profile:   listenerProfile,
		}

		conf.ListenerProfiles = append(conf.ListenerProfiles, transport)
	}

	return ts.Extender.ExAgentGenerate(agentName, conf)
}

type BuildChannelData struct {
	AgentName     string   `json:"agent_name"`
	ListenersName []string `json:"listeners_name"`
}

func (ts *Teamserver) TsAgentBuildCreateChannel(buildData string, wsconn adaptix.WebSocketConn) error {
	var bd BuildChannelData
	if err := json.Unmarshal([]byte(buildData), &bd); err != nil {
		if wsconn != nil {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data"))
			wsconn.Close()
		}
		return errors.New("invalid build data")
	}

	if bd.AgentName == "" || len(bd.ListenersName) == 0 {
		if wsconn != nil {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data"))
			wsconn.Close()
		}
		return errors.New("invalid build data")
	}

	listenersName := bd.ListenersName

	_, wsMsg, err := wsconn.ReadMessage()
	if err != nil {
		_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid agent config"))
		wsconn.Close()
		return errors.New("invalid agent config")
	}

	builder := &AgentBuilder{
		Id:            fmt.Sprintf("%08x", rand.Uint32()),
		Name:          bd.AgentName,
		ListenersName: listenersName,
		Config:        string(wsMsg),
		wsconn:        wsconn,
		mu:            sync.Mutex{},
		closed:        false,
	}
	ts.builders.Put(builder.Id, builder)

	var (
		fileContent []byte
		fileName    string
		conf        adaptix.BuildProfile
	)

	_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_INFO, "Building agent...")

	var postEvent *eventing.EventDataAgentGenerate
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataAgentGenerate{
		BuilderId:     builder.Id,
		AgentName:     builder.Name,
		ListenersName: builder.ListenersName,
		Config:        builder.Config,
	}
	if !ts.EventManager.Emit(eventing.EventAgentGenerate, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_ERROR, "Error: "+preEvent.Error.Error())
		} else {
			_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_ERROR, "Error: operation cancelled by hook")
		}
		err = fmt.Errorf("operation cancelled by hook")
		goto RET
	}
	// ----------------

	conf = adaptix.BuildProfile{
		BuilderId:   builder.Id,
		AgentConfig: builder.Config,
	}

	for _, listener := range listenersName {
		listenerWM, listenerProfile, err := ts.TsListenerGetProfile(listener)
		if err != nil {
			_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_ERROR, fmt.Sprintf("Error: invalid '%s' listener profile", listener))
			goto RET
		}

		_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_INFO, fmt.Sprintf("Listener '%s' profile created", listener))

		transport := adaptix.TransportProfile{
			Watermark: listenerWM,
			Profile:   listenerProfile,
		}
		conf.ListenerProfiles = append(conf.ListenerProfiles, transport)
	}

	fileContent, fileName, err = ts.Extender.ExAgentGenerate(builder.Name, conf)
	if err != nil {
		_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_ERROR, "Error: agent builder failed")
		goto RET
	}
	_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_SUCCESS, "Agent built successfully")

	// --- POST HOOK ---
	postEvent = &eventing.EventDataAgentGenerate{
		BuilderId:     builder.Id,
		AgentName:     builder.Name,
		ListenersName: builder.ListenersName,
		Config:        builder.Config,
		FileName:      fileName,
		FileContent:   fileContent,
	}
	if !ts.EventManager.Emit(eventing.EventAgentGenerate, eventing.HookPost, postEvent) {
		if postEvent.Error != nil {
			_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_ERROR, "Error: "+postEvent.Error.Error())
		} else {
			_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_ERROR, "Error: operation cancelled by hook")
		}
		err = fmt.Errorf("operation cancelled by hook")
		goto RET
	}
	fileName = postEvent.FileName
	fileContent = postEvent.FileContent
	// -----------------

	_ = ts.TsAgentBuildSendFile(builder.Id, fileName, fileContent)

RET:
	ts.TsAgentBuildClose(builder.Id)
	return err
}

func (ts *Teamserver) TsAgentBuildClose(builderId string) {
	if builderId == "" {
		return
	}

	builder, ok := ts.builders.GetDelete(builderId)
	if !ok {
		return
	}

	builder.mu.Lock()
	defer builder.mu.Unlock()

	if !builder.closed {
		builder.closed = true
		if builder.cancel != nil {
			builder.cancel()
		}
		if builder.wsconn != nil {
			_ = builder.wsconn.Close()
		}
	}
}

func (ts *Teamserver) TsAgentBuildExecute(builderId string, workingDir string, env []string, program string, args ...string) error {
	if builderId == "" {
		runner := exec.Command(program, args...)
		runner.Dir = workingDir
		runner.Env = env
		var stderr strings.Builder
		runner.Stderr = &stderr

		err := runner.Run()
		if err != nil {
			return fmt.Errorf("%v: %s", err, stderr.String())
		}
		return nil
	}

	builder, ok := ts.builders.Get(builderId)
	if !ok {
		return errors.New("builder not found")
	}

	builder.mu.Lock()
	if builder.closed {
		builder.mu.Unlock()
		return errors.New("channel closed")
	}
	builder.mu.Unlock()

	ctx, cancel := context.WithCancel(context.Background())
	builder.mu.Lock()
	builder.cancel = cancel
	builder.mu.Unlock()

	runner := exec.CommandContext(ctx, program, args...)
	runner.Dir = workingDir
	runner.Env = env

	var (
		stdoutPipe io.ReadCloser
		stderrPipe io.ReadCloser
		err        error
	)

	if stdoutPipe, err = runner.StdoutPipe(); err != nil {
		return err
	}
	if stderrPipe, err = runner.StderrPipe(); err != nil {
		return err
	}
	if err = runner.Start(); err != nil {
		return err
	}

	var stderrBuf strings.Builder
	var wg sync.WaitGroup
	wg.Add(2)

	pipeLog := func(r io.Reader, capture *strings.Builder) {
		defer wg.Done()
		sc := bufio.NewScanner(r)
		sc.Buffer(make([]byte, 0, 64*1024), 1024*1024)
		for sc.Scan() {
			line := sc.Text() + "\n"
			if capture != nil {
				capture.WriteString(line)
			}
			_ = ts.TsAgentBuildLog(builderId, adaptix.BUILD_LOG_NONE, line)
		}
	}

	go pipeLog(stdoutPipe, nil)
	go pipeLog(stderrPipe, &stderrBuf)

	wg.Wait()
	err = runner.Wait()
	if err != nil {
		return fmt.Errorf("%v: %s", err, stderrBuf.String())
	}

	return nil
}

type BuildLogMessage struct {
	Status  int    `json:"status"`
	Message string `json:"message"`
}

func (ts *Teamserver) TsAgentBuildLog(builderId string, status int, message string) error {
	if builderId == "" {
		return errors.New("builder ID is empty")
	}

	builder, ok := ts.builders.Get(builderId)
	if !ok {
		return errors.New("builder not found")
	}

	builder.mu.Lock()
	defer builder.mu.Unlock()

	if builder.closed {
		return errors.New("channel closed")
	}

	logMsg := BuildLogMessage{Status: status, Message: message}
	jsonData, err := json.Marshal(logMsg)
	if err != nil {
		return err
	}
	return builder.wsconn.WriteMessage(websocket.TextMessage, jsonData)
}

type BuiltPayload struct {
	Status   int    `json:"status"`
	Filename string `json:"filename"`
	Content  []byte `json:"content"`
}

func (ts *Teamserver) TsAgentBuildSendFile(builderId string, filename string, content []byte) error {
	if builderId == "" {
		return errors.New("builder ID is empty")
	}

	builder, ok := ts.builders.Get(builderId)
	if !ok {
		return errors.New("builder not found")
	}

	builder.mu.Lock()
	defer builder.mu.Unlock()

	if builder.closed {
		return errors.New("channel closed")
	}

	data := BuiltPayload{
		Status:   4,
		Filename: filename,
		Content:  content,
	}

	jsonData, err := json.Marshal(data)
	if err != nil {
		return err
	}

	return builder.wsconn.WriteMessage(websocket.TextMessage, jsonData)
}
