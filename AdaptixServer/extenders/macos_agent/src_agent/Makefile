# macOS Agent (Native C) — Development Makefile
# For manual testing / validation. Production builds use pl_main.go.

CC       = aarch64-apple-darwin23.5-clang
CFLAGS   = -Os -fno-stack-protector -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
LDFLAGS  = -lSystem -framework CoreFoundation
SRCDIR   = agent
OBJDIR   = obj

SOURCES  = $(SRCDIR)/main.c \
           $(SRCDIR)/crt.c \
           $(SRCDIR)/msgpack.c \
           $(SRCDIR)/crypt.c \
           $(SRCDIR)/connector.c \
           $(SRCDIR)/agent_info.c \
           $(SRCDIR)/commander.c \
           $(SRCDIR)/tasks_fs.c \
           $(SRCDIR)/tasks_proc.c \
           $(SRCDIR)/tasks_macos.c \
           $(SRCDIR)/jobs.c \
           $(SRCDIR)/tasks_async.c \
           $(SRCDIR)/tasks_net.c \
           $(SRCDIR)/dyld_resolve.c \
           $(SRCDIR)/opsec.c

OBJECTS  = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET   = agent_macos

.PHONY: all clean

all: $(TARGET)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "Built: $@ ($$(ls -la $@ | awk '{print $$5}') bytes)"

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)
