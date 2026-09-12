# Shared Go linker flags. Included from the root Makefile, AdaptixTools,
# and extender Makefiles (path is relative to each Makefile via MAKEFILE_LIST).
#
# Go's linux/arm64 external linker historically injects -fuse-ld=gold
# (https://go.dev/issue/22040). Gold is deprecated and often missing on
# aarch64; gcc then fails with "cannot find 'ld'". GNU ld 2.36+ is fine.
# Force bfd when gold is not on PATH. Harmless on Go 1.26.3+, which no
# longer requires gold when GNU ld is new enough.

ifeq ($(origin GO_LDFLAGS),undefined)
  GO_LDFLAGS := -s -w
  ifeq ($(shell uname -s),Linux)
    ifneq ($(filter aarch64 arm64,$(shell uname -m)),)
      ifeq ($(shell command -v ld.gold 2>/dev/null),)
        GO_LDFLAGS += -extldflags=-fuse-ld=bfd
      endif
    endif
  endif
endif
export GO_LDFLAGS
