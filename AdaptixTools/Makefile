.PHONY: all clean

GO_LDFLAGS ?= -s -w

all:
	@mkdir -p dist
	@GO111MODULE=on GOWORK=off go build -buildvcs=false -ldflags="$(GO_LDFLAGS)" -o dist/axtool .
	@echo "done -> dist/axtool"

clean:
	@rm -rf dist
