package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/krypt"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsFileGenID() int64 {
	return ts.IdGen.Next("file")
}

func (ts *Teamserver) TsDownloadAdd(agentId int64, fileId int64, fileName string, fileSize int64) error {
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

	downloadData := adaptix.TransferData{
		AgentId:    agentId,
		FileId:     fileId,
		RemotePath: fileName,
		TotalSize:  fileSize,
		Progress:   0,
		Date:       time.Now().Unix(),
		State:      adaptix.TRANSFER_STATE_RUNNING,
	}

	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New(fmt.Sprintf("Agent not found: %d", agentId))
	}
	agentData := agent.GetData()
	downloadData.User = agentData.Username
	downloadData.Computer = agentData.Computer
	downloadData.AgentName = agentData.Name

	dirPath := ts.Paths.DownloadPath
	baseName := filepath.Base(filepath.Clean(strings.ReplaceAll(fileName, `\`, `/`)))
	hashSeed := fmt.Sprintf("%d_%d", fileId, downloadData.Date)
	saveName := krypt.MD5([]byte(hashSeed)) + "_" + baseName

	_, err := os.Stat(dirPath)
	if os.IsNotExist(err) {
		err = os.MkdirAll(dirPath, os.ModePerm)
		if err != nil {
			return errors.New("Failed to create download path: " + err.Error())
		}
	}

	downloadData.LocalPath = dirPath + "/" + saveName
	f, err := os.Create(downloadData.LocalPath)
	if err != nil {
		return errors.New("Failed to create file: " + err.Error())
	}
	f.Close()

	ts.downloads.Put(downloadData.FileId, downloadData)

	if err := ts.DBMS.DbDownloadInsert(downloadData); err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server:transfer_manager", "%s", err.Error())
	}

	packet := CreateSpTransferCreate(downloadData, TRANSFER_DOWNLOAD)
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

func (ts *Teamserver) TsDownloadUpdate(fileId int64, state int, data []byte) error {
	d, ok := ts.downloads.Get(fileId)
	if !ok {
		return fmt.Errorf("File not found: %d", fileId)
	}

	var writeErr error
	if len(data) > 0 {
		f, err := os.OpenFile(d.LocalPath, os.O_APPEND|os.O_WRONLY, 0644)
		if err != nil {
			f, err = os.OpenFile(d.LocalPath, os.O_WRONLY|os.O_CREATE, 0644)
			if err != nil {
				writeErr = errors.New("Failed to open file: " + err.Error())
			}
		}
		if writeErr == nil {
			_, err = f.Write(data)
			_ = f.Close()
			if err != nil {
				writeErr = errors.New("Failed to write file '" + d.LocalPath + "': " + err.Error())
			}
		}
	}

	if writeErr != nil {
		return writeErr
	}

	var updated adaptix.TransferData
	ts.downloads.Update(fileId, func(d adaptix.TransferData) adaptix.TransferData {
		d.State = state
		if len(data) > 0 {
			d.Progress += int64(len(data))
		}
		updated = d
		return d
	})

	packet := CreateSpTransferUpdate(updated, TRANSFER_DOWNLOAD)
	ts.TsSyncStateWithCategory(packet, fmt.Sprintf("download:%d", updated.FileId), SyncCategoryDownloadsRealtime)

	return nil
}

func (ts *Teamserver) TsDownloadClose(fileId int64, reason int) error {
	downloadData, ok := ts.downloads.Get(fileId)
	if !ok {
		return fmt.Errorf("File not found: %d", fileId)
	}

	canceled := reason != adaptix.TRANSFER_STATE_FINISHED

	// --- PRE HOOK ---
	preEvent := &eventing.EventDataDownloadFinish{Download: downloadData, Canceled: canceled}
	if !ts.EventManager.Emit(eventing.EventDownloadFinish, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	ts.downloads.Delete(fileId)

	if reason == adaptix.TRANSFER_STATE_FINISHED {
		downloadData.State = adaptix.TRANSFER_STATE_FINISHED
	} else {
		downloadData.State = adaptix.TRANSFER_STATE_CANCELED
		_ = os.Remove(downloadData.LocalPath)
	}

	err := ts.DBMS.DbDownloadUpdateState(downloadData.FileId, downloadData.State, downloadData.Progress)
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server:transfer_manager", "%s", err.Error())
	}

	packet := CreateSpTransferUpdate(downloadData, TRANSFER_DOWNLOAD)
	ts.TsSyncStateWithCategory(packet, fmt.Sprintf("download:%d", downloadData.FileId), SyncCategoryDownloadsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataDownloadFinish{Download: downloadData, Canceled: canceled}
	ts.EventManager.EmitAsync(eventing.EventDownloadFinish, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsDownloadSave(agentId int64, fileId int64, filename string, content []byte) error {

	downloadData := adaptix.TransferData{
		AgentId:    agentId,
		FileId:     fileId,
		RemotePath: filename,
		TotalSize:  int64(len(content)),
		Progress:   int64(len(content)),
		Date:       time.Now().Unix(),
		State:      adaptix.TRANSFER_STATE_FINISHED,
	}

	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New(fmt.Sprintf("Agent not found: %d", agentId))
	}
	agentData := agent.GetData()
	downloadData.User = agentData.Username
	downloadData.Computer = agentData.Computer
	downloadData.AgentName = agentData.Name

	dirPath := ts.Paths.DownloadPath
	baseName := filepath.Base(filepath.Clean(strings.ReplaceAll(filename, `\`, `/`)))
	hashSeed := fmt.Sprintf("%d_%d", fileId, downloadData.Date)
	saveName := krypt.MD5([]byte(hashSeed)) + "_" + baseName

	_, err := os.Stat(dirPath)
	if os.IsNotExist(err) {
		err = os.MkdirAll(dirPath, os.ModePerm)
		if err != nil {
			return errors.New("Failed to create download path: " + err.Error())
		}
	}

	downloadData.LocalPath = dirPath + "/" + saveName
	err = os.WriteFile(downloadData.LocalPath, content, 0644)
	if err != nil {
		return errors.New("Failed to write file: " + err.Error())
	}

	packetRes1 := CreateSpTransferCreate(downloadData, TRANSFER_DOWNLOAD)
	ts.TsSyncAllClientsWithCategory(packetRes1, SyncCategoryDownloadsRealtime)

	packetRes2 := CreateSpTransferUpdate(downloadData, TRANSFER_DOWNLOAD)
	ts.TsSyncStateWithCategory(packetRes2, fmt.Sprintf("download:%d", downloadData.FileId), SyncCategoryDownloadsRealtime)

	err = ts.DBMS.DbDownloadInsert(downloadData)
	if err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server:transfer_manager", "%s", err.Error())
	}

	return nil
}

// /

type DownloadsPage struct {
	Items  []adaptix.TransferData `json:"items"`
	Total  int                    `json:"total"`
	Offset int                    `json:"offset"`
	Limit  int                    `json:"limit"`
}

func (ts *Teamserver) TsDownloadsGetPage(agentId int64, offset, limit int, filterExpr, sortCol, sortOrder string) ([]byte, error) {
	items, total, err := ts.DBMS.DbDownloadsGetPage(agentId, offset, limit, filterExpr, sortCol, sortOrder)
	if err != nil {
		return nil, err
	}
	if items == nil {
		items = make([]adaptix.TransferData, 0)
	}

	for i := range items {
		if live, ok := ts.downloads.Get(items[i].FileId); ok {
			items[i].Progress = live.Progress
			items[i].State = live.State
		}
	}

	return json.Marshal(DownloadsPage{
		Items:  items,
		Total:  total,
		Offset: offset,
		Limit:  limit,
	})
}

func (ts *Teamserver) TsDownloadList() (string, error) {
	var downloads []adaptix.TransferData

	ts.downloads.ForEachFast(func(key int64, data adaptix.TransferData) bool {
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

func (ts *Teamserver) TsDownloadGet(fileId int64) (adaptix.TransferData, error) {
	downloadData, ok := ts.downloads.Get(fileId)
	if ok {
		return downloadData, nil
	}

	var err error
	downloadData, err = ts.DBMS.DbDownloadGet(fileId)
	if err != nil {
		return adaptix.TransferData{}, fmt.Errorf("File not found: %d", fileId)
	}
	return downloadData, nil
}

func (ts *Teamserver) TsDownloadSync(fileId int64) (string, []byte, error) {
	downloadData, err := ts.TsDownloadGet(fileId)
	if err != nil {
		return "", nil, fmt.Errorf("File not found: %d", fileId)
	}

	if downloadData.State != adaptix.TRANSFER_STATE_FINISHED {
		return "", nil, errors.New("download not finished")
	}

	filename := filepath.Base(filepath.FromSlash(filepath.Clean(downloadData.LocalPath)))
	content, err := os.ReadFile(downloadData.LocalPath)
	return filename, content, err
}

func (ts *Teamserver) TsDownloadDelete(fileId []int64) error {
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

	var deleteFiles []int64
	var dbDeleteIds []int64
	var filesToRemove []string

	for _, id := range fileId {
		downloadData, ok := ts.downloads.Get(id)
		if ok {
			if downloadData.State == adaptix.TRANSFER_STATE_RUNNING || downloadData.State == adaptix.TRANSFER_STATE_STOPPED {
				_ = ts.TsDownloadClose(id, adaptix.TRANSFER_STATE_CANCELED)
				deleteFiles = append(deleteFiles, id)
				dbDeleteIds = append(dbDeleteIds, id)
				continue
			}

			filesToRemove = append(filesToRemove, downloadData.LocalPath)
			deleteFiles = append(deleteFiles, id)
			dbDeleteIds = append(dbDeleteIds, id)
			ts.downloads.Delete(id)
		} else {
			downloadData, err := ts.DBMS.DbDownloadGet(id)
			if err != nil {
				continue
			}

			if downloadData.State == adaptix.TRANSFER_STATE_FINISHED {
				filesToRemove = append(filesToRemove, downloadData.LocalPath)
			}
			deleteFiles = append(deleteFiles, id)
			dbDeleteIds = append(dbDeleteIds, id)
		}
	}

	go func(paths []string, ids []int64) {
		for _, path := range paths {
			_ = os.Remove(path)
		}
		_ = ts.DBMS.DbDownloadDeleteBatch(ids)
	}(filesToRemove, dbDeleteIds)

	packet := CreateSpTransferDelete(fileId, TRANSFER_DOWNLOAD)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryDownloadsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataDownloadRemove{FileIds: deleteFiles}
	ts.EventManager.EmitAsync(eventing.EventDownloadRemove, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsDownloadSetTag(fileIds []int64, tag string) error {
	if len(fileIds) == 0 {
		return nil
	}

	for _, id := range fileIds {
		if d, ok := ts.downloads.Get(id); ok {
			d.Tag = tag
			ts.downloads.Put(id, d)
		}
	}

	go func(ids []int64, t string) {
		_ = ts.DBMS.DbDownloadSetTagBatch(ids, t)
	}(fileIds, tag)

	packet := CreateSpTransferSetTag(fileIds, tag, TRANSFER_DOWNLOAD)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryDownloadsRealtime)

	return nil
}

///

func (ts *Teamserver) TsDownloadGetFilepath(fileId int64) (string, error) {
	downloadData, err := ts.TsDownloadGet(fileId)
	if err != nil {
		return "", fmt.Errorf("File not found: %d", fileId)
	}

	if downloadData.State != adaptix.TRANSFER_STATE_FINISHED {
		return "", errors.New("Download not finished")
	}

	return downloadData.LocalPath, nil
}

func (ts *Teamserver) TsUploadGetFilepath(fileId int64) (string, error) {
	filename, ok := ts.tmp_uploads.Get(fileId)
	if !ok {
		return "", fmt.Errorf("File not found: %d", fileId)
	}

	path := ts.Paths.UploadPath + "/" + filename

	return path, nil
}

func (ts *Teamserver) TsUploadGetFileContent(fileId int64) ([]byte, error) {
	filename, ok := ts.tmp_uploads.GetDelete(fileId)
	if !ok {
		return nil, fmt.Errorf("File not found: %d", fileId)
	}

	path := ts.Paths.UploadPath + "/" + filename

	data, err := os.ReadFile(path)
	if err != nil {
		return nil, fmt.Errorf("Failed to read file: %d", fileId)
	}
	_ = os.Remove(path)

	return data, nil
}
