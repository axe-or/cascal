#include "base.h"
#include "lang.h"
#include "testing.h"

#include "base.c"
#include "base_test.c"
#include "scanner.c"
#include "parser.c"
#include "scanner_test.c"
#include "parser_test.c"

int main(void){
	bool ok = true;
	ok = test_run(base_tests) && ok;
	ok = test_run(scanner_tests) && ok;
	ok = test_run(parser_tests) && ok;
	return ok ? 0 : 1;
}
