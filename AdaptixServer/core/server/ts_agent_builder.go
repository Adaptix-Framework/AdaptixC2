package server

import (
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

func (ts *Teamserver) TsAgentBuildSyncOnce(agentName string, config string, listenerWM string, listenerProfile []byte) ([]byte, string, error) {
	return ts.Extender.ExAgentGenerate(agentName, config, listenerWM, listenerProfile, "")
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
	if len(d) != 3 {
		if wsconn != nil {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data"))
			wsconn.Close()
		}
		return errors.New("invalid build data")
	}

	decListenerName, err := base64.StdEncoding.DecodeString(d[0])
	if err != nil {
		if wsconn != nil {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data: listenerName"))
			wsconn.Close()
		}
		return errors.New("invalid build data: listenerName")
	}

	decListenerType, err := base64.StdEncoding.DecodeString(d[1])
	if err != nil {
		if wsconn != nil {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data: listenerType"))
			wsconn.Close()
		}
		return errors.New("invalid build data: listenerType")
	}

	decAgentName, err := base64.StdEncoding.DecodeString(d[2])
	if err != nil {
		if wsconn != nil {
			_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid build data: agentName"))
			wsconn.Close()
		}
		return errors.New("invalid build data: agentName")
	}

	_, wsMsg, err := wsconn.ReadMessage()
	if err != nil {
		_ = wsconn.WriteMessage(websocket.TextMessage, []byte("invalid agent config"))
		wsconn.Close()
		return errors.New("invalid agent config")
	}

	builder := &AgentBuilder{
		Id:           fmt.Sprintf("%08x", rand.Uint32()),
		Name:         string(decAgentName),
		ListenerName: string(decListenerName),
		ListenerType: string(decListenerType),
		Config:       string(wsMsg),
		wsconn:       wsconn,
		mu:           sync.Mutex{},
		closed:       false,
	}
	ts.builders.Put(builder.Id, builder)

	var (
		listenerWM      string
		listenerProfile []byte
		fileContent     []byte
		fileName        string
	)

	_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_INFO, "Building agent...")

	listenerWM, listenerProfile, err = ts.TsListenerGetProfile(builder.ListenerName, builder.ListenerType)
	if err != nil {
		_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_ERROR, "Error: invalid listener profile")
		goto RET
	}
	_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_INFO, "Listener profile created")

	fileContent, fileName, err = ts.Extender.ExAgentGenerate(builder.Name, builder.Config, listenerWM, listenerProfile, builder.Id)
	if err != nil {
		_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_ERROR, "Error: agent builder failed")
		goto RET
	}
	_ = ts.TsAgentBuildLog(builder.Id, adaptix.BUILD_LOG_SUCCESS, "Agent built successfully")

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
