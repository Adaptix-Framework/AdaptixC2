package server

import (
	"AdaptixServer/core/eventing"
	"AdaptixServer/core/utils/krypt"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/tformat"
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"time"

	"github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsScreenshotList() (string, error) {
	dbScreens := ts.DBMS.DbScreenshotAll()
	screens := make([]adaptix.ScreenData, 0, len(dbScreens))
	for _, s := range dbScreens {
		s.LocalPath = "******"
		s.Content = nil
		screens = append(screens, s)
	}

	jsonScreenshot, err := json.Marshal(screens)
	if err != nil {
		return "", err
	}
	return string(jsonScreenshot), nil
}

func (ts *Teamserver) TsScreenshotGetImage(screenId string) ([]byte, error) {
	screenData, err := ts.DBMS.DbScreenshotById(screenId)
	if err != nil {
		return []byte(""), errors.New("Screen not found: " + screenId)
	}
	content, err := os.ReadFile(screenData.LocalPath)
	if err != nil {
		return []byte(""), errors.New("Failed to read screenshot file: " + err.Error())
	}
	return content, nil
}

func (ts *Teamserver) TsScreenshotAdd(agentId string, Note string, Content []byte) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataScreenshotAdd{
		AgentId: agentId,
		Note:    Note,
		Content: Content,
	}
	if !ts.EventManager.Emit(eventing.EventScreenshotAdd, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	screenData := adaptix.ScreenData{
		Note:    Note,
		Date:    time.Now().Unix(),
		Content: Content,
	}

	format, err := tformat.DetectImageFormat(Content)
	if err != nil {
		return err
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
	screenData.User = agentData.Username
	screenData.Computer = agentData.Computer
	screenData.ScreenId, _ = krypt.GenerateUID(8)

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

	packet := CreateSpScreenshotCreate(screenData)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryScreenshotRealtime)

	screenData.Content = nil
	_ = ts.DBMS.DbScreenshotInsert(screenData)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataScreenshotAdd{
		AgentId: agentId,
		Note:    Note,
		Content: Content,
	}
	ts.EventManager.EmitAsync(eventing.EventScreenshotAdd, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsScreenshotNote(screenId string, note string) error {
	_, err := ts.DBMS.DbScreenshotById(screenId)
	if err != nil {
		return errors.New("Screen not found: " + screenId)
	}

	_ = ts.DBMS.DbScreenshotUpdate(screenId, note)
	packet := CreateSpScreenshotUpdate(screenId, note)
	ts.TsSyncStateWithCategory(packet, "screenshot:"+screenId, SyncCategoryScreenshotRealtime)

	return nil
}

func (ts *Teamserver) TsScreenshotDelete(screenId string) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventDataScreenshotRemove{ScreenId: screenId}
	if !ts.EventManager.Emit(eventing.EventScreenshotRemove, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	// ----------------

	screenData, err := ts.DBMS.DbScreenshotById(screenId)
	if err != nil {
		return errors.New("Screen not found: " + screenId)
	}

	_ = os.Remove(screenData.LocalPath)

	_ = ts.DBMS.DbScreenshotDelete(screenId)
	packet := CreateSpScreenshotDelete(screenId)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryScreenshotRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventDataScreenshotRemove{ScreenId: screenId}
	ts.EventManager.EmitAsync(eventing.EventScreenshotRemove, postEvent)
	// -----------------

	return nil
}
