VERSION_MAJOR = 1
VERSION_MINOR = 0

CFLAGS = -pipe -Wall -Wextra -ansi -pedantic -Werror -O2 -fomit-frame-pointer -frename-registers -fPIC
#CFLAGS = -pipe -Wall -Wextra -ansi -pedantic -Werror -g -fPIC -I. -D_DEBUG
#LDFLAGS = -pipe -shared -Wl,-soname,libsmacker.so.$(VERSION_MAJOR)
LDFLAGS = -pipe -L.

all:	libsmacker.so driver smk2avi

smk_bitstream.o:	smk_bitstream.c smk_bitstream.h smk_malloc.h
	gcc $(CFLAGS) -c smk_bitstream.c -D_VERSION="$(VERSION_MAJOR).$(VERSION_MINOR)"
smk_hufftree.o:	smk_hufftree.c smk_hufftree.h smk_malloc.h
	gcc $(CFLAGS) -c smk_hufftree.c -D_VERSION="$(VERSION_MAJOR).$(VERSION_MINOR)"
smacker.o:	smacker.c smacker.h smk_malloc.h
	gcc $(CFLAGS) -c smacker.c -D_VERSION="$(VERSION_MAJOR).$(VERSION_MINOR)"

libsmacker.so: smk_bitstream.o smk_hufftree.o smacker.o
	gcc $(LDFLAGS) -shared -o libsmacker.so smk_bitstream.o smk_hufftree.o smacker.o

clean:
	rm -f libsmacker.* *.o

driver:	libsmacker.so driver.c
	gcc -O2 -L. -lsmacker driver.c -o driver

smk2avi: libsmacker.so smk2avi.c
	gcc -O2 -L. -lsmacker smk2avi.c -o smk2avi
