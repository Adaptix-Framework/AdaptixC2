package server

import (
	"fmt"
	adaptix "github.com/Adaptix-Framework/axc2"
	"math/rand"
	"time"
)

func (ts *Teamserver) TsCredentilsAdd(username string, password string, realm string, credType string, tag string, storage string, agentId string, host string) error {

	for value := range ts.credentials.Iterator() {
		cred := value.Item.(*adaptix.CredsData)
		if cred.Username == username && cred.Realm == realm && cred.Password == password {
			return nil
		}
	}

	credsData := &adaptix.CredsData{
		CredId:   fmt.Sprintf("%08x", rand.Uint32()),
		Username: username,
		Password: password,
		Realm:    realm,
		Type:     credType,
		Tag:      tag,
		Date:     time.Now().Unix(),
		Storage:  storage,
		AgentId:  agentId,
		Host:     host,
	}

	ts.credentials.Put(credsData)

	_ = ts.DBMS.DbCredentialsAdd(*credsData)

	packet := CreateSpCredentialsAdd(*credsData)
	ts.TsSyncAllClients(packet)

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

func (ts *Teamserver) TsCredentilsDelete(credId string) error {

	for i := uint(0); i < ts.credentials.Len(); i++ {
		valuePivot, ok := ts.credentials.Get(i)
		if ok {
			if valuePivot.(*adaptix.CredsData).CredId == credId {
				ts.credentials.Delete(i)
				break
			}
		}
	}
	
	_ = ts.DBMS.DbCredentialsDelete(credId)

	packet := CreateSpCredentialsDelete(credId)
	ts.TsSyncAllClients(packet)

	return nil
}
