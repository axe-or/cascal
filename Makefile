CC := clang
WFLAGS := -Wall -Wextra -Werror=uninitialized -Werror=return-type
CFLAGS := -std=c17 -fwrapv -fno-strict-aliasing -O0 -g
#----------------

EXE := cascal.exe

.PHONY: all clean
all: $(EXE)

$(EXE): $(wildcard *.c *.h) Makefile
	$(CC) $(CFLAGS) -o $(EXE) main.c

clean:
	rm -f *.o $(EXE) || del *.o *.ilk *.pdb *.exp $(EXE)
