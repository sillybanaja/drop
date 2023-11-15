PREFIX=/usr/local

all:
	gcc -Wall -lX11 drop.c -Os -o drop

install: all
	mkdir -p ${PREFIX}/bin
	cp -f drop ${PREFIX}/bin
	chmod 755 ${PREFIX}/bin/drop

clean:
	rm -f drop

.PHONY: all clean install
