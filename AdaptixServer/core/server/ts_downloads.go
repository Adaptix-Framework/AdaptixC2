package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"
)

const (
	DOWNLOAD_STATE_RUNNING  = 0x1
	DOWNLOAD_STATE_STOPPED  = 0x2
	DOWNLOAD_STATE_FINISHED = 0x3
	DOWNLOAD_STATE_CANCELED = 0x4
)

func (ts *Teamserver) TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int) error {
	var (
		downloadData DownloadData
		baseName     string
		saveName     string
		err          error
	)

	downloadData = DownloadData{
		AgentId:    agentId,
		FileId:     fileId,
		RemotePath: fileName,
		TotalSize:  fileSize,
		RecvSize:   0,
		Date:       time.Now().Unix(),
		State:      DOWNLOAD_STATE_RUNNING,
	}

	value, ok := ts.agents.Get(agentId)
	if ok {
		downloadData.Computer = value.(*Agent).Data.Computer
		downloadData.AgentName = value.(*Agent).Data.Name
	} else {
		return errors.New("Agent not found: " + agentId)
	}

	dirPath := logs.RepoLogsInstance.DownloadPath
	baseName = filepath.Base(filepath.Clean(strings.ReplaceAll(fileName, `\`, `/`)))
	saveName = krypt.MD5([]byte(strconv.FormatInt(downloadData.Date, 10))) + "_" + baseName

	if _, err = os.Stat(dirPath); os.IsNotExist(err) {
		if err = os.MkdirAll(dirPath, os.ModePerm); err != nil {
			return errors.New("Failed to create download path: " + err.Error())
		}
	}

	downloadData.LocalPath = dirPath + "/" + saveName
	downloadData.File, err = os.Create(downloadData.LocalPath)
	if err != nil {
		return errors.New("Failed to create file: " + err.Error())
	}

	ts.downloads.Put(downloadData.FileId, downloadData)

	packet := CreateSpDownloadCreate(downloadData)
	ts.TsSyncAllClients(packet)
	ts.TsSyncSavePacket(packet.store, packet)

	return nil
}

func (ts *Teamserver) TsDownloadUpdate(fileId string, state int, data []byte) error {
	var downloadData DownloadData
	value, ok := ts.downloads.Get(fileId)
	if ok {
		downloadData = value.(DownloadData)
	} else {
		return errors.New("File not found: " + fileId)
	}

	downloadData.State = state

	if len(data) > 0 {
		_, err := downloadData.File.Write(data)
		if err != nil {
			downloadData.File, err = os.Create(downloadData.LocalPath)
			if err != nil {
				return errors.New("Failed to create file: " + err.Error())
			}

			_, err = downloadData.File.Write(data)
			if err != nil {
				return errors.New("Failed to write file '" + downloadData.LocalPath + "': " + err.Error())
			}
		}
		downloadData.RecvSize += len(data)
	}

	ts.downloads.Put(downloadData.FileId, downloadData)

	packet := CreateSpDownloadUpdate(downloadData)
	ts.TsSyncAllClients(packet)
	ts.TsSyncSavePacket(packet.store, packet)

	return nil
}

func (ts *Teamserver) TsDownloadFinish(fileId string, state int) error {
	var (
		downloadData DownloadData
		err          error
	)
	value, ok := ts.downloads.Get(fileId)
	if ok {
		downloadData = value.(DownloadData)
	} else {
		return errors.New("File not found: " + fileId)
	}

	downloadData.State = state
	err = downloadData.File.Close()
	if err != nil {
		fmt.Println(fmt.Sprintf("Failed to finish download [%x] file: %v", downloadData.FileId, err))
	}

	if downloadData.State == DOWNLOAD_STATE_CANCELED {
		os.Remove(downloadData.LocalPath)
		ts.downloads.Delete(fileId)
	} else {
		downloadData.State = DOWNLOAD_STATE_FINISHED
		ts.downloads.Put(downloadData.FileId, downloadData)
	}

	packet := CreateSpDownloadUpdate(downloadData)
	ts.TsSyncAllClients(packet)
	ts.TsSyncSavePacket(packet.store, packet)

	return nil
}

func (ts *Teamserver) TsDownloadSync(fileId string) (string, []byte, error) {
	var (
		downloadData DownloadData
		filename     string
		content      []byte
		err          error
	)

	value, ok := ts.downloads.Get(fileId)
	if ok {
		downloadData = value.(DownloadData)
	} else {
		return "", nil, errors.New("File not found: " + fileId)
	}

	if downloadData.State != DOWNLOAD_STATE_FINISHED {
		return "", nil, errors.New("Download not finished")
	}

	filename = filepath.Base(filepath.FromSlash(filepath.Clean(downloadData.LocalPath)))
	content, err = os.ReadFile(downloadData.LocalPath)
	return filename, content, err
}

func (ts *Teamserver) TsDownloadDelete(fileId string) error {
	var (
		downloadData DownloadData
		err          error
	)

	value, ok := ts.downloads.Get(fileId)
	if ok {
		downloadData = value.(DownloadData)
	} else {
		return errors.New("File not found: " + fileId)
	}

	if downloadData.State != DOWNLOAD_STATE_FINISHED {
		return errors.New("Download not finished")
	}

	err = os.Remove(downloadData.LocalPath)
	if err != nil {
		return err
	}

	packet := CreateSpDownloadDelete(downloadData.FileId)
	ts.TsSyncAllClients(packet)
	ts.TsSyncSavePacket(packet.store, packet)

	ts.downloads.Delete(fileId)

	return nil
}

func (ts *Teamserver) TsDownloadChangeState(fileId string, command string) error {
	var (
		downloadData DownloadData
		agentData    AgentData
		agentObject  bytes.Buffer
		newState     int
	)

	value, ok := ts.downloads.Get(fileId)
	if ok {
		downloadData = value.(DownloadData)
	} else {
		return errors.New("File not found: " + fileId)
	}

	if command == "cancel" {
		newState = DOWNLOAD_STATE_CANCELED
	} else if command == "stop" {
		newState = DOWNLOAD_STATE_STOPPED
	} else {
		newState = DOWNLOAD_STATE_RUNNING
	}

	value, ok = ts.agents.Get(downloadData.AgentId)
	if ok {
		agentData = value.(*Agent).Data
	} else {
		return errors.New("Agent not found: " + downloadData.AgentId)
	}
	_ = json.NewEncoder(&agentObject).Encode(agentData)

	return ts.Extender.ExAgentDownloadChangeState(agentData.Name, agentObject.Bytes(), newState, fileId)
}
