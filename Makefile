VERSION_MAJOR = 1
VERSION_MINOR = 0

#CFLAGS = -Wall -Wextra -ansi -pedantic -Werror -O2 -fomit-frame-pointer -frename-registers -funroll-loops -fPIC
CFLAGS = -Wall -Wextra -ansi -pedantic -Werror -g -fPIC -I. -D_DEBUG
#LDFLAGS = -shared -Wl,-soname,libsmacker.so.$(VERSION_MAJOR)
LDFLAGS = -L.

all:	libsmacker.so driver

libsmacker.so:	smacker.o smk_bitstream.o smk_hufftree.o
	gcc $(LDFLAGS) -shared -o libsmacker.so smacker.o smk_bitstream.o smk_hufftree.o

smk_bitstream.o:	smk_bitstream.c
	gcc $(CFLAGS) -c smk_bitstream.c -D_VERSION="$(VERSION_MAJOR).$(VERSION_MINOR)"
smk_hufftree.o:	smk_hufftree.c
	gcc $(CFLAGS) -c smk_hufftree.c -D_VERSION="$(VERSION_MAJOR).$(VERSION_MINOR)"
smacker.o:	smacker.c
	gcc $(CFLAGS) -c smacker.c -D_VERSION="$(VERSION_MAJOR).$(VERSION_MINOR)"


clean:
	rm -f libsmacker.* *.o

driver:	libsmacker.so
	gcc -L. -lsmacker driver.c -o driver
