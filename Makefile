# 通常ビルドと Sanitizer ビルドのオブジェクトを混在させない。
CC ?= cc
PYTHON ?= python3
CPPFLAGS ?= -D_POSIX_C_SOURCE=200809L -I.
CFLAGS ?=
LDFLAGS ?=
LDLIBS ?= -lm
PROFILE ?= release

COMMON_CFLAGS = -Wall -Wextra -std=c99
ifeq ($(PROFILE),asan)
OBJDIR = obj-asan
TARGET = basic-asan
PROFILE_CFLAGS = -g -O1 -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=undefined
PROFILE_LDFLAGS = -fsanitize=address,undefined
else ifeq ($(PROFILE),release)
OBJDIR = obj
TARGET = basic
PROFILE_CFLAGS = -g -O2
PROFILE_LDFLAGS =
else
$(error Unknown PROFILE=$(PROFILE); use release or asan)
endif

SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c,$(OBJDIR)/%.o,$(SOURCES))

.PHONY: all clean test asan test-asan install
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) $(PROFILE_LDFLAGS) $(OBJECTS) -o $@ $(LDLIBS)

$(OBJDIR)/%.o: %.c basic.h | $(OBJDIR)
	$(CC) $(CPPFLAGS) $(COMMON_CFLAGS) $(PROFILE_CFLAGS) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $@

asan:
	$(MAKE) PROFILE=asan all

test-asan:
	$(MAKE) PROFILE=asan test

test: $(TARGET)
	@echo "Testing BASIC interpreter ($(PROFILE))..."
	@printf '%s\n' 'PRINT "Hello, World!"' 'QUIT' | ./$(TARGET)
	$(PYTHON) tests/test_file_commands.py ./$(TARGET)
	$(PYTHON) tests/test_core_regressions.py ./$(TARGET)

clean:
	rm -rf obj obj-asan basic basic-asan

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/basic
