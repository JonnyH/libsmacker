CFLAGS = -Wall -Wextra -ansi -pedantic -Werror -O2 -fomit-frame-pointer -frename-registers -funroll-loops -fPIC
LDFLAGS = 

all:	smacker.o
	gcc $(LDFLAGS) -shared -Wl,-soname,libsmacker.so.1 -o libsmacker.so.1.0.0 smacker.o

smacker.o:	smacker.c
	gcc $(CFLAGS) -c smacker.c

clean:
	rm -f libsmacker.* *.o

driver:	libsmacker.so
	gcc -lsmacker driver.c -o driver
