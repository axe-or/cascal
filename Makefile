CC := clang
WFLAGS := -Wall -Wextra -Werror=uninitialized -Werror=return-type
CFLAGS := -std=c17 -fwrapv -fno-strict-aliasing -O0 -g
#----------------

.PHONY: all clean

all: cascal

base.o: Makefile base.h base.c
	$(CC) $(CFLAGS) -o base.o -c base.c

cascal: Makefile base.o $(wildcard ./*.c ./*.h)
	$(CC) $(CFLAGS) -o cascal base.o main.c 

clean:
	rm -f *.o cascal || del *.o cascal

# # Windows bullshit prevention
# HOME ?= $(USERPROFILE)
# # Config
# WASI_SYSROOT ?= /usr/share/wasi-sysroot
# WASI_CFLAGS := --target=wasm32-wasip1 --sysroot $(WASI_SYSROOT)

