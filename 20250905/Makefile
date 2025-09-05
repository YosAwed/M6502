CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O2
INCLUDES = -I.
LIBS = -lm

SRCDIR = .
OBJDIR = obj
SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)
TARGET = basic

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LIBS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

test: $(TARGET)
	@echo "Testing basic BASIC interpreter..."
	@echo 'PRINT "Hello, World!"' | ./$(TARGET)
	@echo '10 PRINT "Line 10"' | ./$(TARGET)
	@echo '20 PRINT "Line 20"' | ./$(TARGET)
	@echo 'LIST' | ./$(TARGET)

install: $(TARGET)
	cp $(TARGET) /usr/local/bin/

.SUFFIXES:
.SUFFIXES: .c .o
