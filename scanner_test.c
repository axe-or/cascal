#include "base.h"
#include "lang.h"
#include "testing.h"

void scanner_tests(Test* t){
	t->name = "scanner";

	Scanner tokens = {
		.source = strlit(" \t/* outer /* inner */ */ 12_3 + value // line\r\n true"),
	};
	Token_Type expected[] = {
		Tk_Integer,
		Tk_Plus,
		Tk_Identifier,
		Tk_True,
		Tk_EndOfFile,
	};

	for(usize i = 0; i < sizeof(expected) / sizeof(expected[0]); i += 1){
		Scanner_Result result = scan_next_token(&tokens);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, result.token.type == expected[i]);
		if(i == 0){
			t_pred(t, result.token.value_int == 123);
		}
	}

	Scanner peek = {.source = strlit("  /* trivia */ return")};
	Scanner_Result peeked = scan_peek_token(&peek);
	t_pred(t, peeked.token.type == Tk_Return);
	t_pred(t, peek.current == 0);

	Scanner string = {.source = strlit("\"line\\nnext\"")};
	Scanner_Result string_result = scan_next_token(&string);
	t_pred(t, string_result.token.type == Tk_String);
	t_pred(t, string_result.error.typ == Err_None);

	Scanner invalid_string = {.source = strlit("\"line\nnext\"")};
	t_pred(t, scan_next_token(&invalid_string).error.typ == Err_InvalidStringChar);

	Scanner unclosed_comment = {.source = strlit("/* outer /* inner */")};
	t_pred(t, scan_next_token(&unclosed_comment).error.typ == Err_UnclosedComment);
}
