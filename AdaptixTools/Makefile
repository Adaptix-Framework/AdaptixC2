GOFLAGS ?= -ldflags="-s -w"
BINARY  := axtool
DIST    := dist

.PHONY: all clean

all: clean
	@ echo "  * Building axtool"
	@ mkdir -p $(DIST)
	@ go build $(GOFLAGS) -o $(DIST)/$(BINARY) .
	@ echo "    done -> $(DIST)/$(BINARY)"

clean:
	@ rm -rf $(DIST)
