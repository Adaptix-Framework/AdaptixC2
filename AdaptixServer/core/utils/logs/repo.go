package logs

import (
	"fmt"
	"os"
)

type RepoLogs struct {
	Path         string
	DataPath     string
	DbPath       string
	ListenerPath string
	DownloadPath string
}

var RepoLogsInstance *RepoLogs

func NewRepoLogs() (*RepoLogs, error) {

	path, err := os.Getwd()
	repologs := &RepoLogs{
		Path:         path,
		DataPath:     path + "/data",
		DbPath:       path + "/data/adaptixserver.db",
		ListenerPath: path + "/data/listener",
		DownloadPath: path + "/data/download",
	}

	_, err = os.Stat(repologs.DataPath)
	if os.IsNotExist(err) {
		err = os.Mkdir(repologs.DataPath, os.ModePerm)
		if err != nil {
			return nil, fmt.Errorf("failed to create %s folder: %s", repologs.DataPath, err.Error())
		}
	}

	_, err = os.Stat(repologs.ListenerPath)
	if os.IsNotExist(err) {
		err = os.Mkdir(repologs.ListenerPath, os.ModePerm)
		if err != nil {
			return nil, fmt.Errorf("failed to create %s folder: %s", repologs.ListenerPath, err.Error())
		}
	}

	_, err = os.Stat(repologs.DownloadPath)
	if os.IsNotExist(err) {
		err = os.Mkdir(repologs.DownloadPath, os.ModePerm)
		if err != nil {
			return nil, fmt.Errorf("failed to create %s folder: %s", repologs.DownloadPath, err.Error())
		}
	}

	return repologs, nil
}
