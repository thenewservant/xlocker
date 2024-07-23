# Makefile for xlocker - Minimal X11 screen locker
#
# Copyright (C)1993,1994 Ian Jackson
# Copyright (C)2024 thenewservant
#
# This is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3, or (at your option)
# any later version.
#
# This is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

LDLIBS=-lX11 -lcrypt
CC=gcc
CFLAGS=-Wall -Wextra
INSTALL=install
OWNER=root
GROUP=shadow

xlocker: xlocker.o

xlocker.o: 
	$(CC) $(CFLAGS) -c xlocker.c patchlevel.h

install: xlocker
	sudo $(INSTALL) -o $(OWNER) -g $(GROUP) -s -m 4755 xlocker /usr/bin
	make clean

clean: 
	rm -f *.gch xlocker.o xlocker
