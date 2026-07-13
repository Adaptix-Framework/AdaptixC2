package main

import (
	"AdaptixServer/core/connector"
	"AdaptixServer/core/server"
	"AdaptixServer/core/utils/token"
	"flag"
	"fmt"
	"os"

	"github.com/Adaptix-Framework/axc2/v2"
)

func main() {
	fmt.Printf("\n[===== Adaptix Framework %v =====]\n\n", connector.SMALL_VERSION)

	var (
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

	ts := server.NewTeamserver(*debug)
	if ts == nil {
		os.Exit(1)
	}

	if *profilePath != "" {
		if err := ts.SetProfile(*profilePath); err != nil {
			ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "%s", err.Error())
			os.Exit(1)
		}
	} else {
		flag.Usage()
		os.Exit(0)
	}

	if err := ts.Profile.IsValid(); err != nil {
		ts.TsLogAdd(adaptix.LogStatusError, 0, "server", "%s", err.Error())
		os.Exit(1)
	}

	token.InitJWT(ts.Profile.Server.ATokenLive, ts.Profile.Server.RTokenLive)

	ts.Start()
}
