all: agent

agent:
	@ GOWORK=off CGO_ENABLED=0 GOOS=darwin GOARCH=arm64 go build -modfile=go.mod -trimpath -ldflags="-s -w" -o agent && rm agent
