# Makefile for HamDex (C rewrite)
CC      ?= gcc
PKGS    := gtk4 sqlite3 libcurl json-glib-1.0

PREFIX  ?= /usr/local
DESTDIR ?=

CFLAGS  += -Wall -Wextra -O2 -std=gnu11 $(shell pkg-config --cflags $(PKGS))
CFLAGS  += -Ithird_party
CFLAGS  += -MMD -MP

LDFLAGS +=
LDLIBS  += $(shell pkg-config --libs $(PKGS)) -lm -lpthread

SRCS := \
    main.c \
    db.c \
    download.c \
    fcc_db.c \
    fcc_parse.c \
    fcc_urls.c \
    ui_build.c \
    ui_core.c \
    ui_fcc_tab.c \
    ui_lookup.c \
    zip_util.c \
    third_party/miniz.c

OBJS := $(SRCS:.c=.o)
DEPS := $(OBJS:.o=.d)
TARGET := hamdex

.PHONY: all clean run install uninstall

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEPS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
