CC := clang
LUA := lua
WFLAGS := -Wall -Wextra -Werror=uninitialized -Werror=return-type
CFLAGS := -I. -std=c17 -fwrapv -fno-strict-aliasing -O0 -g
#----------------

EXE := cascal.exe
TEST_EXE := cascal_test.exe

.PHONY: all clean test run
all: $(EXE)

run: $(EXE)
	./$(EXE)

gen/type_id_by_hash.c: hash_table.tmpl generate.lua templ.lua
	$(LUA) generate.lua

$(EXE): $(wildcard *.c *.h) gen/type_id_by_hash.c Makefile
	$(CC) $(CFLAGS) -o $(EXE) main.c

$(TEST_EXE): $(wildcard *.c *.h) gen/type_id_by_hash.c Makefile
	$(CC) $(CFLAGS) -o $(TEST_EXE) test_main.c

test: $(TEST_EXE)
	./$(TEST_EXE)

clean:
	rm -f *.o $(EXE) $(TEST_EXE) || del *.o *.ilk *.pdb *.exp $(EXE) $(TEST_EXE)
