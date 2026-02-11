package server

import (
	"AdaptixServer/core/eventing"
	"encoding/json"
	"fmt"
	"math/rand/v2"
	"time"

	adaptix "github.com/Adaptix-Framework/axc2"
)

func (ts *Teamserver) TsCredentilsList() (string, error) {
	var creds []adaptix.CredsData
	for value := range ts.credentials.Iterator() {
		c := *value.Item.(*adaptix.CredsData)
		creds = append(creds, c)
	}

	jsonCreds, err := json.Marshal(creds)
	if err != nil {
		return "", err
	}
	return string(jsonCreds), nil
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
		if v, ok := value["agent_id"].(string); ok {
			cred.AgentId = v
		}
		if v, ok := value["host"].(string); ok {
			cred.Host = v
		}

		found := false
		for c_value := range ts.credentials.Iterator() {
			c := c_value.Item.(*adaptix.CredsData)
			if c.Username == cred.Username && c.Realm == cred.Realm && c.Password == cred.Password {
				found = true
				break
			}
		}
		if found {
			continue
		}

		cred.CredId = fmt.Sprintf("%08x", rand.Uint32())
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

		go ts.TsNotifyCallbackCreds(cbCredsData)
	}

	return nil
}

func (ts *Teamserver) TsCredentilsEdit(credId string, username string, password string, realm string, credType string, tag string, storage string, host string) error {

	var cred *adaptix.CredsData
	var oldCred adaptix.CredsData
	found := false
	for value := range ts.credentials.Iterator() {
		cred = value.Item.(*adaptix.CredsData)
		if cred.CredId == credId {

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
	ts.TsSyncStateWithCategory(packet, "cred:"+credId, SyncCategoryCredentialsRealtime)

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

func (ts *Teamserver) TsCredentilsDelete(credsId []string) error {
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

	go func(ids []string) {
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

func (ts *Teamserver) TsCredentialsSetTag(credsId []string, tag string) error {
	updateSet := make(map[string]struct{}, len(credsId))
	for _, id := range credsId {
		updateSet[id] = struct{}{}
	}

	var updatedCreds []adaptix.CredsData
	for valueCred := range ts.credentials.Iterator() {
		cred := valueCred.Item.(*adaptix.CredsData)
		if _, exists := updateSet[cred.CredId]; exists {
			cred.Tag = tag
			updatedCreds = append(updatedCreds, *cred)
		}
	}

	go func(creds []adaptix.CredsData) {
		_ = ts.DBMS.DbCredentialsUpdateBatch(creds)
	}(updatedCreds)

	var ids []string
	for _, c := range updatedCreds {
		ids = append(ids, c.CredId)
	}

	packet := CreateSpCredentialsSetTag(ids, tag)
	ts.TsSyncAllClientsWithCategory(packet, SyncCategoryCredentialsRealtime)

	return nil
}
