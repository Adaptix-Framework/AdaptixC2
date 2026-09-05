package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/krypt"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"os"
	"path/filepath"
	"strings"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

func (ts *Teamserver) TsUploadAdd(agentId int64, fileId int64, localPath string, remotePath string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataUploadStart{
		AgentId:    agentId,
		FileId:     fileId,
		FileName:   filepath.Base(localPath),
		RemotePath: remotePath,
	}
	if !ts.EventManager.Emit(eventing.EventUploadStart, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New(fmt.Sprintf("Agent not found: %d", agentId))
	}
	agentData := agent.GetData()

	fi, err := os.Stat(localPath)
	if err != nil {
		return errors.New("Failed to stat file: " + err.Error())
	}

	uploadData := adaptix.TransferData{
		FileId:     fileId,
		AgentId:    agentId,
		AgentName:  agentData.Name,
		User:       agentData.Username,
		Computer:   agentData.Computer,
		LocalPath:  localPath,
		RemotePath: remotePath,
		TotalSize:  fi.Size(),
		Progress:   0,
		ReadOffset: 0,
		Date:       time.Now().Unix(),
		State:      adaptix.TRANSFER_STATE_RUNNING,
	}

	preEvent.FileSize = fi.Size()

	uploadData.Cancellable = true
	uploadData.Kind = adaptix.TRANSFER_KIND_FILE

	ts.uploads.Put(uploadData.FileId, uploadData)
	if err := ts.DBMS.DbUploadInsert(uploadData); err != nil {
		ts.uploads.Delete(uploadData.FileId)
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "transfer_manager", "%s", err.Error())
		return err
	}

	packet := CreateSpTransferCreate(uploadData, TRANSFER_UPLOAD)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryDownloadsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataUploadStart{
		AgentId:    agentId,
		FileId:     fileId,
		FileName:   filepath.Base(localPath),
		RemotePath: remotePath,
		FileSize:   fi.Size(),
	}
	ts.EventManager.EmitAsync(eventing.EventUploadStart, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsUploadAddContent(agentId int64, fileId int64, remotePath string, content []byte, canceled bool, kind int, artname string, arttype string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataUploadStart{
		AgentId:    agentId,
		FileId:     fileId,
		FileName:   filepath.Base(remotePath),
		RemotePath: remotePath,
		FileSize:   int64(len(content)),
	}
	if !ts.EventManager.Emit(eventing.EventUploadStart, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	agent, ok := ts.Agents.Get(agentId)
	if !ok {
		return errors.New(fmt.Sprintf("Agent not found: %d", agentId))
	}
	agentData := agent.GetData()

	dirPath := ts.Paths.UploadPath
	_, err := os.Stat(dirPath)
	if os.IsNotExist(err) {
		err = os.MkdirAll(dirPath, os.ModePerm)
		if err != nil {
			return errors.New("Failed to create upload path: " + err.Error())
		}
	}

	baseName := filepath.Base(filepath.Clean(strings.ReplaceAll(remotePath, `\`, `/`)))
	hashSeed := fmt.Sprintf("%d_%d", fileId, time.Now().Unix())
	saveName := krypt.MD5([]byte(hashSeed)) + "_" + baseName
	savePath := dirPath + "/" + saveName

	err = os.WriteFile(savePath, content, 0644)
	if err != nil {
		return errors.New("Failed to write temp file: " + err.Error())
	}

	uploadData := adaptix.TransferData{
		FileId:       fileId,
		AgentId:      agentId,
		AgentName:    agentData.Name,
		User:         agentData.Username,
		Computer:     agentData.Computer,
		LocalPath:    savePath,
		RemotePath:   remotePath,
		TotalSize:    int64(len(content)),
		Progress:     0,
		ReadOffset:   0,
		Date:         time.Now().Unix(),
		State:        adaptix.TRANSFER_STATE_RUNNING,
		Cancellable:  canceled,
		Kind:         kind,
		ArtifactName: artname,
		ArtifactType: arttype,
	}

	ts.uploads.Put(uploadData.FileId, uploadData)
	err = ts.DBMS.DbUploadInsert(uploadData)
	if err != nil {
		ts.uploads.Delete(uploadData.FileId)
		_ = os.Remove(savePath)
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "transfer_manager", "%s", err.Error())
		return err
	}

	packet := CreateSpTransferCreate(uploadData, TRANSFER_UPLOAD)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryDownloadsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataUploadStart{
		AgentId:    agentId,
		FileId:     fileId,
		FileName:   baseName,
		RemotePath: remotePath,
		FileSize:   int64(len(content)),
	}
	ts.EventManager.EmitAsync(eventing.EventUploadStart, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsUploadGetChunk(fileId int64, chunkSize int, needApprove bool) ([]byte, error) {
	snapshot, ok := ts.uploads.Get(fileId)
	if !ok {
		return nil, fmt.Errorf("File not found: %d", fileId)
	}

	if snapshot.State != adaptix.TRANSFER_STATE_RUNNING {
		return nil, errors.New("Upload not running")
	}

	remaining := snapshot.TotalSize - snapshot.ReadOffset
	if remaining <= 0 {
		return nil, nil
	}

	readSize := int64(chunkSize)
	if readSize > remaining {
		readSize = remaining
	}

	f, err := os.Open(snapshot.LocalPath)
	if err != nil {
		return nil, fmt.Errorf("Failed to open file: %d", fileId)
	}
	defer f.Close()

	_, err = f.Seek(snapshot.ReadOffset, io.SeekStart)
	if err != nil {
		return nil, fmt.Errorf("Failed to seek file: %d", fileId)
	}

	buf := make([]byte, readSize)
	n, err := f.Read(buf)
	if err != nil || n == 0 {
		return nil, fmt.Errorf("Failed to read file: %d", fileId)
	}

	var updated adaptix.TransferData
	ts.uploads.Update(fileId, func(d adaptix.TransferData) adaptix.TransferData {
		d.ReadOffset += int64(n)
		if !needApprove {
			d.Progress += int64(n)
		}
		updated = d
		return d
	})

	if !needApprove {
		packet := CreateSpTransferUpdate(updated, TRANSFER_UPLOAD)
		ts.TsSyncStateWithCategory(packet, fmt.Sprintf("upload:%d", updated.FileId), SyncCategoryDownloadsRealtime)
	}

	return buf[:n], nil
}

func (ts *Teamserver) TsUploadApprove(fileId int64, approvedBytes int) error {
	var updated adaptix.TransferData

	ok := ts.uploads.Update(fileId, func(d adaptix.TransferData) adaptix.TransferData {
		d.Progress += int64(approvedBytes)
		updated = d
		return d
	})
	if !ok {
		return fmt.Errorf("File not found: %d", fileId)
	}

	packet := CreateSpTransferUpdate(updated, TRANSFER_UPLOAD)
	ts.TsSyncStateWithCategory(packet, fmt.Sprintf("upload:%d", updated.FileId), SyncCategoryDownloadsRealtime)

	return nil
}

func (ts *Teamserver) TsUploadClose(fileId int64, reason int) error {
	canceled := reason != adaptix.TRANSFER_STATE_FINISHED
	if canceled {
		if snapshot, ok := ts.uploads.Get(fileId); ok && !snapshot.Cancellable {
			return fmt.Errorf("upload %d is not cancellable", fileId)
		}
	}

	return ts.closeUploadForce(fileId, reason)
}

func (ts *Teamserver) closeUploadForce(fileId int64, reason int) error {
	uploadData, ok := ts.uploads.GetDelete(fileId)
	if !ok {
		return fmt.Errorf("File not found: %d", fileId)
	}

	canceled := reason != adaptix.TRANSFER_STATE_FINISHED

	// --- PRE HOOK ---
	preEvent := &eventing.EventDataUploadFinish{FileId: fileId, Canceled: canceled}
	if !ts.EventManager.Emit(eventing.EventUploadFinish, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	if reason == adaptix.TRANSFER_STATE_FINISHED {
		uploadData.State = adaptix.TRANSFER_STATE_FINISHED
		uploadData.Progress = uploadData.TotalSize
	} else {
		uploadData.State = adaptix.TRANSFER_STATE_CANCELED
		ts.TsFrameResetDownstream(uploadData.AgentId)
	}

	if err := ts.DBMS.DbUploadUpdateState(uploadData.FileId, uploadData.State, uploadData.Progress); err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "transfer_manager", "%s", err.Error())
	}

	packet := CreateSpTransferUpdate(uploadData, TRANSFER_UPLOAD)
	ts.TsSyncStateWithCategory(packet, fmt.Sprintf("upload:%d", uploadData.FileId), SyncCategoryDownloadsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataUploadFinish{FileId: fileId, Canceled: canceled}
	ts.EventManager.EmitAsync(eventing.EventUploadFinish, postEvent)
	// -----------------

	return nil
}

///

func (ts *Teamserver) TsUploadGet(fileId int64) (adaptix.TransferData, error) {
	if uploadData, ok := ts.uploads.Get(fileId); ok {
		return uploadData, nil
	}
	uploadData, err := ts.DBMS.DbUploadGet(fileId)
	if err != nil {
		return adaptix.TransferData{}, fmt.Errorf("File not found: %d", fileId)
	}
	return uploadData, nil
}

func (ts *Teamserver) TsUploadsGetPage(agentId int64, offset, limit int, filterExpr, sortCol, sortOrder string) ([]byte, error) {
	items, total, err := ts.DBMS.DbUploadsGetPage(agentId, offset, limit, filterExpr, sortCol, sortOrder)
	if err != nil {
		return nil, err
	}

	for i := range items {
		if live, ok := ts.uploads.Get(items[i].FileId); ok {
			items[i].Progress = live.Progress
			items[i].State = live.State
			items[i].Cancellable = live.Cancellable
		}
	}

	return json.Marshal(struct {
		Items  []adaptix.TransferData `json:"items"`
		Total  int                    `json:"total"`
		Offset int                    `json:"offset"`
		Limit  int                    `json:"limit"`
	}{Items: items, Total: total, Offset: offset, Limit: limit})
}

func (ts *Teamserver) TsUploadDelete(fileIds []int64) error {
	if len(fileIds) == 0 {
		return nil
	}
	for _, id := range fileIds {
		_ = ts.closeUploadForce(id, adaptix.TRANSFER_STATE_CANCELED)
	}
	if err := ts.DBMS.DbUploadDeleteBatch(fileIds); err != nil {
		return err
	}
	packet := CreateSpTransferDelete(fileIds, TRANSFER_UPLOAD)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryDownloadsRealtime)
	return nil
}
