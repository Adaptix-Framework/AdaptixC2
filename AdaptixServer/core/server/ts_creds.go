package server

import (
	"AdaptixServer/core/eventing"
	"encoding/json"
	"fmt"
	"strconv"
	"time"

	"github.com/Adaptix-Framework/axc2/v2"
)

func toInt64Any(v any) (int64, bool) {
	switch x := v.(type) {
	case int64:
		return x, true
	case int:
		return int64(x), true
	case float64:
		return int64(x), true
	case string:
		if x == "" {
			return 0, false
		}
		n, err := strconv.ParseInt(x, 10, 64)
		return n, err == nil
	case json.Number:
		n, err := x.Int64()
		return n, err == nil
	}
	return 0, false
}

func (ts *Teamserver) TsCredGenID() int64 {
	return ts.IdGen.Next("cred")
}

func (ts *Teamserver) TsCredentilsList() (string, error) {
	dbCreds := ts.DBMS.DbCredentialsAll()
	creds := make([]adaptix.CredsData, 0, len(dbCreds))
	for _, c := range dbCreds {
		creds = append(creds, *c)
	}

	jsonCreds, err := json.Marshal(creds)
	if err != nil {
		return "", err
	}
	return string(jsonCreds), nil
}

type CredsPage struct {
	Items  []adaptix.CredsData `json:"items"`
	Total  int                 `json:"total"`
	Offset int                 `json:"offset"`
	Limit  int                 `json:"limit"`
}

func (ts *Teamserver) TsCredentialsGetPage(agentId int64, offset, limit int, filterExpr, sortCol, sortOrder string) ([]byte, error) {
	items, total, err := ts.DBMS.DbCredentialsGetPage(agentId, offset, limit, filterExpr, sortCol, sortOrder)
	if err != nil {
		return nil, err
	}
	if items == nil {
		items = make([]adaptix.CredsData, 0)
	}
	return json.Marshal(CredsPage{
		Items:  items,
		Total:  total,
		Offset: offset,
		Limit:  limit,
	})
}

func (ts *Teamserver) TsCredentilsAdd(creds []map[string]interface{}) error {
	var newCreds []*adaptix.CredsData
	var cbCredsData []adaptix.CredsData
	var inputCreds []adaptix.CredsData

	for _, value := range creds {
		cred := &adaptix.CredsData{}
		if v, ok := value["username"].(string); ok {
			cred.Username = v
		}
		if v, ok := value["password"].(string); ok {
			cred.Password = v
		}
		if v, ok := value["realm"].(string); ok {
			cred.Realm = v
		}
		if v, ok := value["type"].(string); ok {
			cred.Type = v
		}
		if v, ok := value["tag"].(string); ok {
			cred.Tag = v
		}
		if v, ok := value["storage"].(string); ok {
			cred.Storage = v
		}
		if v, ok := toInt64Any(value["agent_id"]); ok {
			cred.AgentId = v
		}
		if v, ok := value["host"].(string); ok {
			cred.Host = v
		}

		if cred.Username == "" && cred.Password == "" {
			continue
		}

		if ts.DBMS.DbCredentialsFindDuplicate(cred.Username, cred.Realm, cred.Password) {
			continue
		}

		cred.CredId = ts.TsCredGenID()
		cred.Date = time.Now().Unix()

		inputCreds = append(inputCreds, *cred)
	}

	if len(inputCreds) == 0 {
		return nil
	}

	// --- PRE HOOK ---
	preEvent := &eventing.EventCredentialsAdd{Credentials: inputCreds}
	if !ts.EventManager.Emit(eventing.EventCredsAdd, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	inputCreds = preEvent.Credentials /// can be modified by hooks
	// ----------------

	for i := range inputCreds {
		cbCredsData = append(cbCredsData, inputCreds[i])
		newCreds = append(newCreds, &inputCreds[i])
	}

	if len(newCreds) > 0 {
		_ = ts.DBMS.DbCredentialsAdd(newCreds)

		packet := CreateSpCredentialsAdd(newCreds)
		ts.TsSyncAllClientsWithCategory(packet, SyncCategoryCredentialsRealtime)

		// --- POST HOOK ---
		postEvent := &eventing.EventCredentialsAdd{Credentials: cbCredsData}
		ts.EventManager.EmitAsync(eventing.EventCredsAdd, postEvent)
		// -----------------
	}
	return nil
}

func (ts *Teamserver) TsCredentilsEdit(credId int64, username string, password string, realm string, credType string, tag string, storage string, host string) error {

	cred, err := ts.DBMS.DbCredentialById(credId)
	if err != nil {
		return fmt.Errorf("creds %d not exists", credId)
	}

	if cred.Username == username && cred.Realm == realm && cred.Password == password && cred.Type == credType && cred.Tag == tag && cred.Storage == storage && cred.Host == host {
		return nil
	}

	oldCred := *cred

	cred.Username = username
	cred.Password = password
	cred.Realm = realm
	cred.Type = credType
	cred.Tag = tag
	cred.Storage = storage
	cred.Host = host

	// --- PRE HOOK ---
	preEvent := &eventing.EventCredentialsEdit{
		CredId:  credId,
		OldCred: oldCred,
		NewCred: *cred,
	}
	if !ts.EventManager.Emit(eventing.EventCredsEdit, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	*cred = preEvent.NewCred /// can be modified by hooks
	// ----------------

	_ = ts.DBMS.DbCredentialsUpdate(*cred)

	packet := CreateSpCredentialsUpdate(*cred)
	ts.TsSyncStateWithCategory(packet, fmt.Sprintf("cred:%d", credId), SyncCategoryCredentialsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventCredentialsEdit{
		CredId:  credId,
		OldCred: oldCred,
		NewCred: *cred,
	}
	ts.EventManager.EmitAsync(eventing.EventCredsEdit, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsCredentilsDelete(credsId []int64) error {
	// --- PRE HOOK ---
	preEvent := &eventing.EventCredentialsRemove{CredIds: credsId}
	if !ts.EventManager.Emit(eventing.EventCredsRemove, eventing.HookPre, preEvent) {
		if preEvent.Error != nil {
			return preEvent.Error
		}
		return fmt.Errorf("operation cancelled by hook")
	}
	credsId = preEvent.CredIds /// can be modified by hooks
	// ----------------

	go func(ids []int64) {
		_ = ts.DBMS.DbCredentialsDeleteBatch(ids)
	}(credsId)

	packet := CreateSpCredentialsDelete(credsId)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryCredentialsRealtime)

	// --- POST HOOK ---
	postEvent := &eventing.EventCredentialsRemove{CredIds: credsId}
	ts.EventManager.EmitAsync(eventing.EventCredsRemove, postEvent)
	// -----------------

	return nil
}

func (ts *Teamserver) TsCredentialsSetTag(credsId []int64, tag string) error {
	go func(ids []int64, t string) {
		_ = ts.DBMS.DbCredentialsSetTagBatch(ids, t)
	}(credsId, tag)

	packet := CreateSpCredentialsSetTag(credsId, tag)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryCredentialsRealtime)

	return nil
}
