package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
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
	baseName = filepath.Base(filepath.FromSlash(filepath.Clean(fileName)))
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

	if len(data) == 0 {
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
	} else if downloadData.State == DOWNLOAD_STATE_FINISHED {
		ts.downloads.Put(downloadData.FileId, downloadData)
	}

	packet := CreateSpDownloadUpdate(downloadData)
	ts.TsSyncAllClients(packet)
	ts.TsSyncSavePacket(packet.store, packet)

	return nil
}
