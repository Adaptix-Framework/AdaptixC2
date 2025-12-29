package server

import (
	"encoding/json"
	"fmt"
	"math/rand"
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

		cbCredsData = append(cbCredsData, *cred)
		newCreds = append(newCreds, cred)
		ts.credentials.Put(cred)
	}

	if len(newCreds) > 0 {
		_ = ts.DBMS.DbCredentialsAdd(newCreds)

		packet := CreateSpCredentialsAdd(newCreds)
		ts.TsSyncAllClients(packet)

		go ts.TsEventCallbackCreds(cbCredsData)
	}

	return nil
}

func (ts *Teamserver) TsCredentilsEdit(credId string, username string, password string, realm string, credType string, tag string, storage string, host string) error {

	var cred *adaptix.CredsData
	found := false
	for value := range ts.credentials.Iterator() {
		cred = value.Item.(*adaptix.CredsData)
		if cred.CredId == credId {

			if cred.Username == username && cred.Realm == realm && cred.Password == password && cred.Type == credType && cred.Tag == tag && cred.Storage == storage && cred.Host == host {
				return nil
			}

			found = true

			cred.Username = username
			cred.Password = password
			cred.Realm = realm
			cred.Type = credType
			cred.Tag = tag
			cred.Storage = storage
			cred.Host = host
			break
		}
	}

	if !found {
		return fmt.Errorf("creds %s not exists", credId)
	}

	_ = ts.DBMS.DbCredentialsUpdate(*cred)

	packet := CreateSpCredentialsUpdate(*cred)
	ts.TsSyncAllClients(packet)

	return nil
}

func (ts *Teamserver) TsCredentilsDelete(credsId []string) error {
	for _, id := range credsId {
		for i := uint(0); i < ts.credentials.Len(); i++ {
			valuePivot, ok := ts.credentials.Get(i)
			if ok {
				if valuePivot.(*adaptix.CredsData).CredId == id {
					ts.credentials.Delete(i)
					break
				}
			}
		}

		_ = ts.DBMS.DbCredentialsDelete(id)
	}

	packet := CreateSpCredentialsDelete(credsId)
	ts.TsSyncAllClients(packet)

	return nil
}

/// Setters

func (ts *Teamserver) TsCredentialsSetTag(credsId []string, tag string) error {

	var ids []string
	for valueCred := range ts.credentials.Iterator() {
		cred := valueCred.Item.(*adaptix.CredsData)
		found := false

		for i := len(credsId) - 1; i >= 0; i-- {
			if cred.CredId == credsId[i] {
				cred.Tag = tag
				found = true
				_ = ts.DBMS.DbCredentialsUpdate(*cred)
				ids = append(ids, cred.CredId)
				credsId = append(credsId[:i], credsId[i+1:]...)
				break
			}
		}

		if found && len(credsId) == 0 {
			break
		}
	}

	packet := CreateSpCredentialsSetTag(ids, tag)
	ts.TsSyncAllClients(packet)

	return nil
}
