package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsDownloadAdd(agentId string, fileId string, fileName string, fileSize int64) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataDownloadStart{
		AgentId:  agentId,
		FileId:   fileId,
		FileName: fileName,
		FileSize: fileSize,
	}
	if !ts.EventManager.Emit(eventing.EventDownloadStart, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	downloadData := adaptix.DownloadData{
		AgentId:    agentId,
		FileId:     fileId,
		RemotePath: fileName,
		TotalSize:  fileSize,
		RecvSize:   0,
		Date:       time.Now().Unix(),
		State:      adaptix.DOWNLOAD_STATE_RUNNING,
	}

	value, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New("Agent not found: " + agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("Invalid agent type: " + agentId)
	}
	agentData := agent.GetData()
	downloadData.User = agentData.Username
	downloadData.Computer = agentData.Computer
	downloadData.AgentName = agentData.Name

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
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryDownloadsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataDownloadStart{
		AgentId:  agentId,
		FileId:   fileId,
		FileName: fileName,
		FileSize: fileSize,
	}
	ts.EventManager.EmitAsync(eventing.EventDownloadStart, postEvent)
	// -----------------

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
		downloadData.RecvSize += int64(len(data))
	}

	ts.downloads.Put(downloadData.FileId, downloadData)

	packet := CreateSpDownloadUpdate(downloadData)
	ts.TsSyncStateWithCategory(packet, "download:"+downloadData.FileId, SyncCategoryDownloadsRealtime)

	return nil
}

func (ts *Teamserver) TsDownloadClose(fileId string, reason int) error {
	value, ok := ts.downloads.Get(fileId)
	if !ok {
		return errors.New("File not found: " + fileId)
	}
	downloadData := value.(adaptix.DownloadData)

	canceled := reason != adaptix.DOWNLOAD_STATE_FINISHED

	// --- PRE HOOK ---
	preEvent := &eventing.EventDataDownloadFinish{Download: downloadData, Canceled: canceled}
	if !ts.EventManager.Emit(eventing.EventDownloadFinish, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	err := downloadData.File.Close()
	if err != nil {
		logs.Debug("", fmt.Sprintf("Failed to finish download [%x] file: %v", downloadData.FileId, err))
	}

	if reason == adaptix.DOWNLOAD_STATE_FINISHED {
		downloadData.State = adaptix.DOWNLOAD_STATE_FINISHED
		err = ts.DBMS.DbDownloadInsert(downloadData)
		if err != nil {
			logs.Error("", err.Error())
		}
		ts.downloads.Delete(fileId)
	} else {
		downloadData.State = adaptix.DOWNLOAD_STATE_CANCELED
		_ = os.Remove(downloadData.LocalPath)
		ts.downloads.Delete(fileId)
	}

	packet := CreateSpDownloadUpdate(downloadData)
	ts.TsSyncStateWithCategory(packet, "download:"+downloadData.FileId, SyncCategoryDownloadsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataDownloadFinish{Download: downloadData, Canceled: canceled}
	ts.EventManager.EmitAsync(eventing.EventDownloadFinish, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsDownloadSave(agentId string, fileId string, filename string, content []byte) error {

	downloadData := adaptix.DownloadData{
		AgentId:    agentId,
		FileId:     fileId,
		RemotePath: filename,
		TotalSize:  int64(len(content)),
		RecvSize:   int64(len(content)),
		Date:       time.Now().Unix(),
		State:      adaptix.DOWNLOAD_STATE_FINISHED,
	}

	value, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New("Agent not found: " + agentId)
	}
	agent, ok := value.(*Agent)
	if !ok {
		return errors.New("Invalid agent type: " + agentId)
	}
	agentData := agent.GetData()
	downloadData.User = agentData.Username
	downloadData.Computer = agentData.Computer
	downloadData.AgentName = agentData.Name

	dirPath := logs.RepoLogsInstance.DownloadPath
	baseName := filepath.Base(filepath.Clean(strings.ReplaceAll(filename, `\`, `/`)))
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
	_, err = downloadData.File.Write(content)
	if err != nil {
		_ = downloadData.File.Close()
		return errors.New("Failed to write file: " + err.Error())
	}
	_ = downloadData.File.Close()

	packetRes1 := CreateSpDownloadCreate(downloadData)
	ts.TsSyncAllClientsWithCategory(packetRes1, SyncCategoryDownloadsRealtime)

	packetRes2 := CreateSpDownloadUpdate(downloadData)
	ts.TsSyncStateWithCategory(packetRes2, "download:"+downloadData.FileId, SyncCategoryDownloadsRealtime)

	err = ts.DBMS.DbDownloadInsert(downloadData)
	if err != nil {
		logs.Error("", err.Error())
	}

	return nil
}

///

func (ts *Teamserver) TsDownloadList() (string, error) {
	var downloads []adaptix.DownloadData

	ts.downloads.ForEach(func(key string, value interface{}) bool {
		data := value.(adaptix.DownloadData)
		data.LocalPath = "******"
		downloads = append(downloads, data)
		return true
	})

	dbDownloads := ts.DBMS.DbDownloadAll()
	for _, data := range dbDownloads {
		data.LocalPath = "******"
		downloads = append(downloads, data)
	}

	jsonDownloads, err := json.Marshal(downloads)
	if err != nil {
		return "", err
	}
	return string(jsonDownloads), nil
}

func (ts *Teamserver) TsDownloadGet(fileId string) (adaptix.DownloadData, error) {
	value, ok := ts.downloads.Get(fileId)
	if ok {
		return value.(adaptix.DownloadData), nil
	}

	downloadData, err := ts.DBMS.DbDownloadGet(fileId)
	if err != nil {
		return adaptix.DownloadData{}, errors.New("File not found: " + fileId)
	}
	return downloadData, nil
}

func (ts *Teamserver) TsDownloadSync(fileId string) (string, []byte, error) {
	downloadData, err := ts.TsDownloadGet(fileId)
	if err != nil {
		return "", nil, errors.New("File not found: " + fileId)
	}

	if downloadData.State != adaptix.DOWNLOAD_STATE_FINISHED {
		return "", nil, errors.New("download not finished")
	}

	filename := filepath.Base(filepath.FromSlash(filepath.Clean(downloadData.LocalPath)))
	content, err := os.ReadFile(downloadData.LocalPath)
	return filename, content, err
}

func (ts *Teamserver) TsDownloadDelete(fileId []string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataDownloadRemove{FileIds: fileId}
	if !ts.EventManager.Emit(eventing.EventDownloadRemove, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	fileId = preEvent.FileIds
	// ----------------

	var deleteFiles []string
	var dbDeleteIds []string
	var filesToRemove []string

	for _, id := range fileId {
		value, ok := ts.downloads.Get(id)
		if ok {
			downloadData := value.(adaptix.DownloadData)

			if downloadData.State != adaptix.DOWNLOAD_STATE_FINISHED && downloadData.State != adaptix.DOWNLOAD_STATE_CANCELED {
				continue
			}

			if downloadData.State == adaptix.DOWNLOAD_STATE_CANCELED {
				_ = downloadData.File.Close()
			}
			filesToRemove = append(filesToRemove, downloadData.LocalPath)
			deleteFiles = append(deleteFiles, id)
			ts.downloads.Delete(id)
		} else {
			downloadData, err := ts.DBMS.DbDownloadGet(id)
			if err != nil {
				continue
			}

			if downloadData.State != adaptix.DOWNLOAD_STATE_FINISHED {
				continue
			}

			filesToRemove = append(filesToRemove, downloadData.LocalPath)
			deleteFiles = append(deleteFiles, id)
			dbDeleteIds = append(dbDeleteIds, id)
		}
	}

	go func(paths []string, ids []string) {
		for _, path := range paths {
			_ = os.Remove(path)
		}
		_ = ts.DBMS.DbDownloadDeleteBatch(ids)
	}(filesToRemove, dbDeleteIds)

	packet := CreateSpDownloadDelete(fileId)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryDownloadsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataDownloadRemove{FileIds: deleteFiles}
	ts.EventManager.EmitAsync(eventing.EventDownloadRemove, postEvent)
	// -----------------

	return nil
}

///

func (ts *Teamserver) TsDownloadGetFilepath(fileId string) (string, error) {
	downloadData, err := ts.TsDownloadGet(fileId)
	if err != nil {
		return "", errors.New("File not found: " + fileId)
	}

	if downloadData.State != adaptix.DOWNLOAD_STATE_FINISHED {
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

	data, err := os.ReadFile(path)
	if err != nil {
		return nil, errors.New("Failed to read file: " + fileId)
	}
	_ = os.Remove(path)

	return data, nil
}
