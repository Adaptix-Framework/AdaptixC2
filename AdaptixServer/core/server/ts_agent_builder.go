package server

import (
	"AdaptixServer/core/eventing"
	"encoding/base64"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"math/rand/v2"
	"os/exec"
	"strings"
	"sync"

	adaptix "github.com/Adaptix-Framework/axc2"
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

func (ts *Teamserver) TsAgentBuildCreateChannel(buildData string, wsconn *websocket.Conn) error {
	data, err := base64.StdEncoding.DecodeString(buildData)
	if err != nil {
		if wsconn != nil {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data"))
			wsconn.Close()
		}
		return errors.New("invalid build data")
	}

	d := strings.Split(string(data), "|")
	if len(d) < 2 {
		if wsconn != nil {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data"))
			wsconn.Close()
		}
		return errors.New("invalid build data")
	}

	decAgentName, err := base64.StdEncoding.DecodeString(d[0])
	if err != nil {
		if wsconn != nil {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data: agentName"))
			wsconn.Close()
		}
		return errors.New("invalid build data: agentName")
	}

	var listenersName []string

	for _, encListenerName := range d[1:] {
		decListenerName, err := base64.StdEncoding.DecodeString(encListenerName)
		if err != nil {
			if wsconn != nil {
				_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data: listenerName"))
				wsconn.Close()
			}
			return errors.New("invalid build data: listenerName")
		}
		listenersName = append(listenersName, string(decListenerName))
	}

	_, wsMsg, err := wsconn.ReadMessage()
	if err != nil {
		_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid agent config"))
		wsconn.Close()
		return errors.New("invalid agent config")
	}

	builder := &AgentBuilder{
		Id:            fmt.Sprintf("%08x", rand.Uint32()),
		Name:          string(decAgentName),
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
		AgentName:     builder.Name,
		ListenersName: builder.ListenersName,
		Config:        builder.Config,
		FileName:      fileName,
		FileContent:   fileContent,
	}
	ts.EventManager.EmitAsync(eventing.EventAgentGenerate, postEvent)
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

	value, ok := ts.builders.GetDelete(builderId)
	if !ok {
		return
	}
	builder, _ := value.(*AgentBuilder)

	builder.mu.Lock()
	defer builder.mu.Unlock()

	if !builder.closed {
		builder.closed = true
		if builder.wsconn != nil {
			_ = builder.wsconn.Close()
		}
	}
}

func (ts *Teamserver) TsAgentBuildExecute(builderId string, workingDir string, program string, args ...string) error {
	runner := exec.Command(program, args...)
	runner.Dir = workingDir

	if builderId == "" {
		var stderr strings.Builder
		runner.Stderr = &stderr

		err := runner.Run()
		if err != nil {
			return fmt.Errorf("%v: %s", err, stderr.String())
		}
		return nil
	}

	value, ok := ts.builders.Get(builderId)
	if !ok {
		return errors.New("builder not found")
	}
	builder, _ := value.(*AgentBuilder)

	builder.mu.Lock()
	if builder.closed {
		builder.mu.Unlock()
		return errors.New("channel closed")
	}
	builder.mu.Unlock()

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

	go func() {
		defer wg.Done()
		buf := make([]byte, 1024)
		for {
			n, err := stdoutPipe.Read(buf)
			if n > 0 {
				_ = ts.TsAgentBuildLog(builderId, adaptix.BUILD_LOG_NONE, string(buf[:n]))
			}
			if err != nil {
				break
			}
		}
	}()

	go func() {
		defer wg.Done()
		buf := make([]byte, 1024)
		for {
			n, err := stderrPipe.Read(buf)
			if n > 0 {
				stderrBuf.Write(buf[:n])
				_ = ts.TsAgentBuildLog(builderId, adaptix.BUILD_LOG_NONE, string(buf[:n]))
			}
			if err != nil {
				break
			}
		}
	}()

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

	value, ok := ts.builders.Get(builderId)
	if !ok {
		return errors.New("builder not found")
	}
	builder, _ := value.(*AgentBuilder)

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

	value, ok := ts.builders.Get(builderId)
	if !ok {
		return errors.New("builder not found")
	}
	builder, _ := value.(*AgentBuilder)

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
