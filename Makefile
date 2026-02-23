# Copyright (C) 2026 sillybanaja
# See LICENSE file for license details.

PREFIX=/usr/local

all:
	gcc -Wall -Os -o drop drop.c -lX11 -lXi

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f drop ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/drop

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/drop

clean:
	rm -f drop

.PHONY: all clean install
