#include "base.h"
#include "array.h"
#include "lang.h"
#include "testing.h"

#include "base.c"
#include "base_test.c"
#include "array_test.c"
#include "scanner.c"
#include "parser.c"
#include "types.c"
#include "scanner_test.c"
#include "parser_test.c"
#include "types_test.c"

int main(int argc, char** argv){
	// Preserve earlier test results when a regression aborts the process.
	setvbuf(stdout, NULL, _IONBF, 0);
	if(argc == 2){
		String name = {.v = argv[1], .len = 0};
		while(name.v[name.len]) name.len += 1;
		for(usize i = 0; i < sizeof(interner_cases) / sizeof(interner_cases[0]); i += 1){
			if(str_equal(name, interner_cases[i].name)){
				return test_run(interner_cases[i].run) ? 0 : 1;
			}
		}
		return 2;
	}
	bool ok = true;
	ok = test_run(base_tests) && ok;
	ok = test_run(array_tests) && ok;
	ok = test_run(scanner_tests) && ok;
	ok = test_run(parser_tests) && ok;
	ok = test_run(type_tests) && ok;
	for(usize i = 0; i < sizeof(interner_cases) / sizeof(interner_cases[0]); i += 1){
		ok = test_run(interner_cases[i].run) && ok;
	}
	return ok ? 0 : 1;
}
