package main

import (
	"AdaptixServer/core/server"
	"AdaptixServer/core/utils/logs"
	"AdaptixServer/core/utils/token"
	"flag"
	"fmt"
	"os"
)

const VERSION = 0.1

func main() {
	fmt.Printf("\n[===== Adaptix Framework v%v by HackerRalf =====]\n\n", VERSION)

	var (
		port        = flag.Int("p", 0, "Teamserver handler port")
		endpoint    = flag.String("e", "", "Teamserver URI endpoint")
		password    = flag.String("pw", "", "Teamserver password")
		certPath    = flag.String("sc", "", "Path to the SSL certificate")
		keyPath     = flag.String("sk", "", "Path to the SSL key")
		debug       = flag.Bool("debug", false, "Enable debug mode")
		profilePath = flag.String("profile", "", "Path to JSON profile file")
	)

	flag.Usage = func() {
		fmt.Printf("Usage: AdaptixServer [options]\n")
		fmt.Printf("Options:\n")
		flag.PrintDefaults()
		fmt.Printf("\nEither provide options individually or use a JSON config file with -config flag.\n\n")
		fmt.Printf("Example:\n")
		fmt.Printf("   AdaptixServer -p port -pw password -e endpoint -sc SslCert -sk SslKey [-debug]\n")
		fmt.Printf("   AdaptixServer -profile profile.json [-debug] \n")
	}

	flag.Parse()

	logs.NewPrintLogger(*debug)

	token.InitJWT()

	ts := server.NewTeamserver()

	if *profilePath != "" {
		err := ts.SetProfile(*profilePath)
		if err != nil {
			logs.Error(err.Error())
			os.Exit(1)
		}
	} else if *port > 1 && *port < 65535 && *endpoint != "" && *password != "" {
		ts.SetSettings(*port, *endpoint, *password, *certPath, *keyPath)
	} else {
		flag.Usage()
		os.Exit(0)
	}

	err := ts.Profile.IsVaid()
	if err != nil {
		logs.Error(err.Error())
		os.Exit(0)
	}

	ts.Start()
}