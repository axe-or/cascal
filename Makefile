CC := clang
WFLAGS := -Wall -Wextra -Werror=uninitialized -Werror=return-type
CFLAGS := -std=c17 -fwrapv -fno-strict-aliasing -O0 -g
#----------------

EXE := cascal.exe
TEST_EXE := cascal_test.exe

.PHONY: all clean test
all: $(EXE)

$(EXE): $(wildcard *.c *.h) Makefile
	$(CC) $(CFLAGS) -o $(EXE) main.c

$(TEST_EXE): $(wildcard *.c *.h) Makefile
	$(CC) $(CFLAGS) -o $(TEST_EXE) test_main.c

test: $(TEST_EXE)
	./$(TEST_EXE)

clean:
	rm -f *.o $(EXE) $(TEST_EXE) || del *.o *.ilk *.pdb *.exp $(EXE) $(TEST_EXE)
