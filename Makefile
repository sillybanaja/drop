# See LICENSE file for license details.
PREFIX=/usr/local

all:
	gcc -Wall -lX11 drop.c -O0 -o drop

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f drop ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/drop

uninstall:
	rm  -f ${DESTDIR}${PREFIX}/bin/drop

clean:
	rm -f drop

.PHONY: all clean install
