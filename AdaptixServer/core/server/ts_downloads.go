package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"errors"
	"fmt"
	"github.com/Adaptix-Framework/axc2"
	"io/ioutil"
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
	downloadData := adaptix.DownloadData{
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
		downloadData.User = value.(*Agent).Data.Username
		downloadData.Computer = value.(*Agent).Data.Computer
		downloadData.AgentName = value.(*Agent).Data.Name
	} else {
		return errors.New("Agent not found: " + agentId)
	}

	dirPath := logs.RepoLogsInstance.DownloadPath
	baseName := filepath.Base(filepath.Clean(strings.ReplaceAll(fileName, `\`, `/`)))
	saveName := krypt.MD5([]byte(strconv.FormatInt(downloadData.Date, 10))) + "_" + baseName

	_, err := os.Stat(dirPath)
	if os.IsNotExist(err) {
		err = os.MkdirAll(dirPath, os.ModePerm)
		if err != nil {
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

	return nil
}

func (ts *Teamserver) TsDownloadUpdate(fileId string, state int, data []byte) error {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)
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

	return nil
}

func (ts *Teamserver) TsDownloadClose(fileId string, reason int) error {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)

	err := downloadData.File.Close()
	if err != nil {
		fmt.Println(fmt.Sprintf("Failed to finish download [%x] file: %v", downloadData.FileId, err))
	}

	if reason == DOWNLOAD_STATE_FINISHED {
		downloadData.State = DOWNLOAD_STATE_FINISHED
		ts.downloads.Put(downloadData.FileId, downloadData)
		err = ts.DBMS.DbDownloadInsert(downloadData)
		if err != nil {
			logs.Error("", err.Error())
		}
	} else {
		downloadData.State = DOWNLOAD_STATE_CANCELED
		_ = os.Remove(downloadData.LocalPath)
		ts.downloads.Delete(fileId)
	}

	packet := CreateSpDownloadUpdate(downloadData)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsDownloadGet(fileId string) (adaptix.DownloadData, error) {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return adaptix.DownloadData{}, errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)

	return downloadData, nil
}

func (ts *Teamserver) TsDownloadSync(fileId string) (string, []byte, error) {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return "", nil, errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)

	if downloadData.State != DOWNLOAD_STATE_FINISHED {
		return "", nil, errors.New("download not finished")
	}

	filename := filepath.Base(filepath.FromSlash(filepath.Clean(downloadData.LocalPath)))
	content, err := os.ReadFile(downloadData.LocalPath)
	return filename, content, err
}

func (ts *Teamserver) TsDownloadDelete(fileId string) error {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)

	if downloadData.State != DOWNLOAD_STATE_FINISHED && downloadData.State != DOWNLOAD_STATE_CANCELED {
		return errors.New("download not finished")
	}

	if downloadData.State == DOWNLOAD_STATE_CANCELED {
		_ = downloadData.File.Close()
	}

	_ = os.Remove(downloadData.LocalPath)

	if downloadData.State == DOWNLOAD_STATE_FINISHED {
		_ = ts.DBMS.DbDownloadDelete(fileId)
	}

	packet := CreateSpDownloadDelete(downloadData.FileId)
	ts.TsSyncAllClients(packet)

	ts.downloads.Delete(fileId)

	return nil
}

///

func (ts *Teamserver) TsDownloadTaskStart(agentId string, path string, clientName string) error {
	value, ok := ts.agents.Get(agentId)
	if !ok {
		return fmt.Errorf("agent '%v' does not exist", agentId)
	}

	agent, _ := value.(*Agent)
	if agent.Active == false {
		return fmt.Errorf("agent '%v' not active", agentId)
	}

	taskData, err := ts.Extender.ExAgentDownloadTaskStart(agent.Data, path)
	if err != nil {
		return err
	}

	ts.TsTaskCreate(agentId, "", clientName, taskData)
	return nil
}

func (ts *Teamserver) TsDownloadTaskCancel(fileId string, clientName string) error {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)

	value, ok = ts.agents.Get(downloadData.AgentId)
	if !ok {
		return errors.New("Agent not found: " + downloadData.AgentId)
	}
	agent := value.(*Agent)

	taskData, err := ts.Extender.ExAgentDownloadTaskCancel(agent.Data, fileId)
	if err != nil {
		return err
	}

	ts.TsTaskCreate(agent.Data.Id, "", clientName, taskData)
	return nil
}

func (ts *Teamserver) TsDownloadTaskResume(fileId string, clientName string) error {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)

	value, ok = ts.agents.Get(downloadData.AgentId)
	if !ok {
		return errors.New("Agent not found: " + downloadData.AgentId)
	}
	agent := value.(*Agent)

	taskData, err := ts.Extender.ExAgentDownloadTaskResume(agent.Data, fileId)
	if err != nil {
		return err
	}

	ts.TsTaskCreate(agent.Data.Id, "", clientName, taskData)
	return nil
}

func (ts *Teamserver) TsDownloadTaskPause(fileId string, clientName string) error {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)

	value, ok = ts.agents.Get(downloadData.AgentId)
	if !ok {
		return errors.New("Agent not found: " + downloadData.AgentId)
	}
	agent := value.(*Agent)

	taskData, err := ts.Extender.ExAgentDownloadTaskPause(agent.Data, fileId)
	if err != nil {
		return err
	}

	ts.TsTaskCreate(agent.Data.Id, "", clientName, taskData)
	return nil
}

///

func (ts *Teamserver) TsDownloadGetFilepath(fileId string) (string, error) {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return "", errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)

	if downloadData.State != DOWNLOAD_STATE_FINISHED {
		return "", errors.New("Download not finished")
	}

	return downloadData.LocalPath, nil
}

func (ts *Teamserver) TsUploadGetFilepath(fileId string) (string, error) {
	value, ok := ts.tmp_uploads.Get(fileId)
	if !ok {
		return "", errors.New("File not found: " + fileId)
	}
	filename := value.(string)

	path := logs.RepoLogsInstance.UploadPath + "/" + filename

	return path, nil
}

func (ts *Teamserver) TsUploadGetFileContent(fileId string) ([]byte, error) {
	value, ok := ts.tmp_uploads.GetDelete(fileId)
	if !ok {
		return nil, errors.New("File not found: " + fileId)
	}
	filename := value.(string)

	path := logs.RepoLogsInstance.UploadPath + "/" + filename

	data, err := ioutil.ReadFile(path)
	if err != nil {
		return nil, errors.New("Failed to read file: " + fileId)
	}
	_ = os.Remove(path)

	return data, nil
}
