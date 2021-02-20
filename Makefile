#!/usr/bin/make -f

all: avr-control
LIBS    =
CFLAGS  = -Wall -g -O2 -D_GNU_SOURCE -export-dynamic
CFLAGS += $(shell pkg-config --cflags gtk+-3.0)
CFLAGS += $(shell pkg-config --cflags json-glib-1.0)
LIBS   += $(shell pkg-config --libs gtk+-3.0)
LIBS   += $(shell pkg-config --libs json-glib-1.0)
LDFLAGS = -Wl,--as-needed

avr-control: avr-control.c
	gcc ${CFLAGS} -o $@ $< ${LDFLAGS} ${LIBS}

install:
	cp avr /usr/local/bin
ifeq ($(wildcard avr-control),)
	# Compiled avr-control application not found - skipping installation
else
	cp avr-control /usr/local/bin
	mkdir -p /usr/local/share/avr-control
	cp avr-control.xml /usr/local/share/avr-control
	mkdir -p /usr/share/applications
	cp avr-control.desktop /usr/share/applications
endif
