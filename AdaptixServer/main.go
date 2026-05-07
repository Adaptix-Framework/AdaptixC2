package main

import (
	"AdaptixServer/core/server"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"flag"
	"fmt"
	"os"
)

func main() {
	fmt.Printf("\n[===== Adaptix Framework %v =====]\n\n", server.SMALL_VERSION)

	var (
		err         error
		debug       = flag.Bool("debug", false, "Enable debug mode")
		profilePath = flag.String("profile", "", "Path to YAML profile file")
	)

	flag.Usage = func() {
		fmt.Printf("Usage: AdaptixServer [options]\n")
		fmt.Printf("Options:\n")
		flag.PrintDefaults()
		fmt.Printf("\nEither provide a YAML config file with -profile flag.\n\n")
		fmt.Printf("Example:\n")
		fmt.Printf("   AdaptixServer -profile profile.yaml [-debug]\n")
	}
	flag.Parse()

	logs.NewPrintLogger(*debug)
	logs.RepoLogsInstance, err = logs.NewRepoLogs()
	if err != nil {
		logs.Error("", err.Error())
		os.Exit(0)
	}

	ts := server.NewTeamserver()

	if *profilePath != "" {
		err := ts.SetProfile(*profilePath)
		if err != nil {
			logs.Error("", err.Error())
			os.Exit(1)
		}
	} else {
		flag.Usage()
		os.Exit(0)
	}

	err = ts.Profile.IsValid()
	if err != nil {
		logs.Error("", err.Error())
		os.Exit(0)
	}

	token.InitJWT(ts.Profile.Server.ATokenLive, ts.Profile.Server.RTokenLive)

	ts.Start()
}
