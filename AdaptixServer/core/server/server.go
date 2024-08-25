package server

import (
	"AdaptixServer/core/httphandler"
	"AdaptixServer/core/profile"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/safe"
	"encoding/json"
	"os"
)

func NewTeamserver() *Teamserver {
	ts := new(Teamserver)
	ts.Profile = profile.NewProfile()
	ts.clients = safe.NewMap()
	return ts
}

func (ts *Teamserver) SetSettings(port int, endpoint string, password string, cert string, key string) {
	ts.Profile.Server = &profile.TsProfile{
		Port:     port,
		Endpoint: endpoint,
		Password: password,
		Cert:     cert,
		Key:      key,
	}
}

func (ts *Teamserver) SetProfile(path string) error {
	var (
		err        error
		fileConten []byte
	)

	fileConten, err = os.ReadFile(path)
	if err != nil {
		return err
	}

	err = json.Unmarshal(fileConten, &ts.Profile)
	if err != nil {
		return err
	}

	return nil
}

func (ts *Teamserver) Start() {
	var (
		stoped chan bool
		err    error
	)

	ts.AdaptixServer, err = httphandler.NewTsHttpHandler(ts, *ts.Profile.Server)
	if err != nil {
		logs.Error("Failed to init HTTP handler: " + err.Error())
		return
	}

	go ts.AdaptixServer.Start(&stoped)

	logs.Success("Starting server -> https://%s:%v%s", "0.0.0.0", ts.Profile.Server.Port, ts.Profile.Server.Endpoint)

	<-stoped

	logs.Warn("Teamserver finished")
	os.Exit(0)
}
