#include "base.h"
#include "lang.h"
#include "testing.h"

typedef struct {
	u8 memory[16 * 1024];
	Arena arena;
	Parser parser;
} Parser_Fixture;

static Parser_Result parse_test_expression(String source, Parser_Fixture* fixture){
	fixture->arena = arena_from_buffer(fixture->memory, sizeof(fixture->memory));
	fixture->parser = parser_make(source, &fixture->arena);
	return parse_expression(&fixture->parser);
}

static bool is_binary(Node const* node, Token_Type op){
	return node != NULL && node->type == Node_Binary && node->value.binary.op == op;
}

static bool is_unary(Node const* node, Token_Type op){
	return node != NULL && node->type == Node_Unary && node->value.unary.op == op;
}

static bool is_integer(Node const* node, i64 value){
	return node != NULL && node->type == Node_Integer && node->value.integer == value;
}

void parser_tests(Test* t){
	t->name = "parser";

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("1 + 2 * 3"), &fixture);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, is_binary(result.node, Tk_Plus));
		if(is_binary(result.node, Tk_Plus)){
			Node* left = result.node->value.binary.left;
			Node* right = result.node->value.binary.right;
			t_pred(t, is_integer(left, 1));
			t_pred(t, is_binary(right, Tk_Star));
			if(is_binary(right, Tk_Star)){
				t_pred(t, is_integer(right->value.binary.left, 2));
				t_pred(t, is_integer(right->value.binary.right, 3));
				t_pred(t, right->parent == result.node);
			}
		}
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("10 - 3 - 2"), &fixture);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, is_binary(result.node, Tk_Minus));
		if(is_binary(result.node, Tk_Minus)){
			t_pred(t, is_binary(result.node->value.binary.left, Tk_Minus));
			t_pred(t, is_integer(result.node->value.binary.right, 2));
		}
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("a = b"), &fixture);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, result.node != NULL && result.node->type == Node_Identifier);
		t_pred(t, scan_peek_token(&fixture.parser.scanner).token.type == Tk_Assign);
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("1 + 2 << 3 & 4"), &fixture);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, is_binary(result.node, Tk_Plus));
		if(is_binary(result.node, Tk_Plus)){
			Node* right = result.node->value.binary.right;
			t_pred(t, is_binary(right, Tk_And));
			if(is_binary(right, Tk_And)){
				t_pred(t, is_binary(right->value.binary.left, Tk_ShiftLeft));
			}
		}
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("-value.field * (2 + 3)"), &fixture);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, is_binary(result.node, Tk_Star));
		if(is_binary(result.node, Tk_Star)){
			Node* left = result.node->value.binary.left;
			Node* right = result.node->value.binary.right;
			t_pred(t, is_unary(left, Tk_Minus));
			if(is_unary(left, Tk_Minus)){
				t_pred(t, is_binary(left->value.unary.operand, Tk_Dot));
			}
			t_pred(t, is_binary(right, Tk_Plus));
		}
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("not false or true"), &fixture);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, is_binary(result.node, Tk_LogicOr));
		if(is_binary(result.node, Tk_LogicOr)){
			Node* left = result.node->value.binary.left;
			Node* right = result.node->value.binary.right;
			t_pred(t, is_unary(left, Tk_LogicNot));
			t_pred(t, right->type == Node_Boolean && right->value.boolean);
			if(is_unary(left, Tk_LogicNot)){
				Node* boolean = left->value.unary.operand;
				t_pred(t, boolean->type == Node_Boolean && !boolean->value.boolean);
			}
		}
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("\"line\\nnext\""), &fixture);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, result.node != NULL && result.node->type == Node_String);
		if(result.node != NULL && result.node->type == Node_String){
			t_pred(t, str_equal(result.node->value.str, strlit("line\nnext")));
		}
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("(1 + 2"), &fixture);
		t_pred(t, result.node == NULL);
		t_pred(t, result.error.typ == Err_UnexpectedToken);
		t_pred(t, result.error.expected.token_type == Tk_ParenClose);
		t_pred(t, result.error.got.token_type == Tk_EndOfFile);
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("1 +"), &fixture);
		t_pred(t, result.node == NULL);
		t_pred(t, result.error.typ == Err_UnexpectedToken);
		t_pred(t, result.error.got.token_type == Tk_EndOfFile);
	}
}
