# CC=g++
# INCLUDES=-I. -I..
# LIBS=-L. -lreadline -lncurses
# SOURCES=$(wildcard test/*.c)
# SOURCES+=format.c libparser.c shell.c

# FILES := $(subst .c,,$(SOURCES))

# # By default, make runs the first target in the file
# all: libvfs.so $(FILES) 

# libvfs.so: vfs.c
# 	$(CC) -g -Wall -Wvla -Werror $(INCLUDES) -fPIC -c $< -o vfs.o 
# 	$(CC) -shared -o $@ vfs.o 

# % :: %.c libvfs.so
# 	$(CC) -g -Wall -Wvla -Werror $(INCLUDES) $(LIBS) -Wl,-rpath=. $< -o $@ -lvfs -lm

# clean: 
# 	rm -f $(FILES) libvfs.so
CC=g++
INCLUDES=-I. -I..
LIBS=-L. -lreadline -lncurses
CFLAGS=-g -Wall -Wvla -Werror $(INCLUDES)

# All source files
PROGRAM_SOURCES := format.c shell.c $(wildcard test/*.c)
PROGRAMS := $(PROGRAM_SOURCES:.c=)

# Library object
LIBPARSER_OBJ := libparser.o

all: libvfs.so $(PROGRAMS)

libvfs.so: vfs.c
	$(CC) $(CFLAGS) -fPIC -c $< -o vfs.o 
	$(CC) -shared -o $@ vfs.o 

# Build libparser as object file only
$(LIBPARSER_OBJ): libparser.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build each program, linking with libparser.o if needed
format: format.c $(LIBPARSER_OBJ) libvfs.so
	$(CC) $(CFLAGS) $(LIBS) -Wl,-rpath=. $< libparser.o -o $@ -lvfs -lm

shell: shell.c $(LIBPARSER_OBJ) libvfs.so
	$(CC) $(CFLAGS) $(LIBS) -Wl,-rpath=. $< libparser.o -o $@ -lvfs -lm

test/%: test/%.c libvfs.so
	$(CC) $(CFLAGS) $(LIBS) -Wl,-rpath=. $< -o $@ -lvfs -lm

clean: 
	rm -f $(PROGRAMS) libvfs.so vfs.o libparser.o
