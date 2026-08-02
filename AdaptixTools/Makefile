.PHONY: all clean

all:
	@mkdir -p dist
	@GO111MODULE=on GOWORK=off go build -buildvcs=false -ldflags="-s -w" -o dist/axtool .
	@echo "done -> dist/axtool"

clean:
	@rm -rf dist
