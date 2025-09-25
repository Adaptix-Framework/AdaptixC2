package server

import (
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/tformat"
	"encoding/json"
	"errors"
	"os"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsScreenshotList() (string, error) {
	var screens []adaptix.ScreenData
	ts.screenshots.ForEach(func(key string, value interface{}) bool {
		screens = append(screens, value.(adaptix.ScreenData))
		return true
	})

	jsonScreenshot, err := json.Marshal(screens)
	if err != nil {
		return "", err
	}
	return string(jsonScreenshot), nil
}

func (ts *Teamserver) TsScreenshotAdd(agentId string, Note string, Content []byte) error {
	screenData := adaptix.ScreenData{
		Note:    Note,
		Date:    time.Now().Unix(),
		Content: Content,
	}

	format, err := tformat.DetectImageFormat(Content)
	if err != nil {
		return err
	}

	value, ok := ts.agents.Get(agentId)
	if ok {
		screenData.User = value.(*Agent).Data.Username
		screenData.Computer = value.(*Agent).Data.Computer
		screenData.ScreenId, _ = krypt.GenerateUID(8)
	} else {
		return errors.New("Agent not found: " + agentId)
	}

	d := time.Now().Format("15:04:05 02.01.2006")
	saveName := krypt.MD5(append([]byte(d), screenData.Content...)) + "." + format

	dirPath := logs.RepoLogsInstance.ScreenshotPath
	_, err = os.Stat(dirPath)
	if os.IsNotExist(err) {
		err = os.MkdirAll(dirPath, os.ModePerm)
		if err != nil {
			return errors.New("Failed to create screenshots path: " + err.Error())
		}
	}

	screenData.LocalPath = dirPath + "/" + saveName
	err = os.WriteFile(screenData.LocalPath, Content, os.ModePerm)
	if err != nil {
		return errors.New("Failed to create file: " + err.Error())
	}

	ts.screenshots.Put(screenData.ScreenId, screenData)

	_ = ts.DBMS.DbScreenshotInsert(screenData)

	packet := CreateSpScreenshotCreate(screenData)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsScreenshotNote(screenId string, note string) error {
	value, ok := ts.screenshots.Get(screenId)
	if !ok {
		return errors.New("Screen not found: " + screenId)
	}
	screenData := value.(adaptix.ScreenData)
	screenData.Note = note

	ts.screenshots.Put(screenId, screenData)

	_ = ts.DBMS.DbScreenshotUpdate(screenId, note)
	packet := CreateSpScreenshotUpdate(screenId, note)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsScreenshotDelete(screenId string) error {
	value, ok := ts.screenshots.Get(screenId)
	if !ok {
		return errors.New("Screen not found: " + screenId)
	}
	screenData := value.(adaptix.ScreenData)

	_ = os.Remove(screenData.LocalPath)

	_ = ts.DBMS.DbScreenshotDelete(screenId)
	packet := CreateSpScreenshotDelete(screenId)
	ts.TsSyncAllClients(packet)

	ts.screenshots.Delete(screenId)
	return nil
}
