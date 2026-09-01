#include "base.h"
#include "lang.h"
#include "testing.h"

typedef struct {
	u8 memory[16 * 1024];
	Arena arena;
	Parser parser;
} Parser_Fixture;

static void parser_fixture_init(String source, Parser_Fixture* fixture){
	fixture->arena = arena_from_buffer(fixture->memory, sizeof(fixture->memory));
	fixture->parser = parser_make(source, &fixture->arena);
}

static Parser_Result parse_test_expression(String source, Parser_Fixture* fixture){
	parser_fixture_init(source, fixture);
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

static bool is_identifier(Node const* node, String ident){
	return node != NULL
		&& node->type == Node_Identifier
		&& str_equal(node->value.ident, ident);
}

static bool is_named_parser_type(Parser_Type const* type, String name){
	return type != NULL
		&& type->kind == ParserType_Named
		&& str_equal(type->value.name, name);
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

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("[32][]^Item"), &fixture);
		Parser_Type_Result result = parse_type(&fixture.parser);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, result.type != NULL && result.type->kind == ParserType_Array);
		if(result.type != NULL && result.type->kind == ParserType_Array){
			t_pred(t, result.type->value.length == 32);
			Parser_Type* slice = result.type->value.element;
			t_pred(t, slice != NULL && slice->kind == ParserType_Slice);
			if(slice != NULL && slice->kind == ParserType_Slice){
				Parser_Type* pointer = slice->value.element;
				t_pred(t, pointer != NULL && pointer->kind == ParserType_Pointer);
				if(pointer != NULL && pointer->kind == ParserType_Pointer){
					t_pred(t, is_named_parser_type(pointer->value.element, strlit("Item")));
				}
			}
		}
		t_pred(t, scan_peek_token(&fixture.parser.scanner).token.type == Tk_EndOfFile);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("[]"), &fixture);
		Parser_Type_Result result = parse_type(&fixture.parser);
		t_pred(t, result.type == NULL);
		t_pred(t, result.error.typ == Err_UnexpectedToken);
		t_pred(t, result.error.got.token_type == Tk_EndOfFile);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("first, second, third:"), &fixture);
		Parser_Result result = parse_identifier_list(&fixture.parser);
		Node_List list = {.first = result.node, .last = result.last_node};
		t_pred(t, !has_error(result));
		t_pred(t, node_list_cardinality(list) == 3);
		t_pred(t, is_identifier(list.first, strlit("first")));
		t_pred(t, is_identifier(list.last, strlit("third")));
		t_pred(t, scan_peek_token(&fixture.parser.scanner).token.type == Tk_Colon);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("1 + 2, value, false"), &fixture);
		Parser_Result result = parse_expression_list(&fixture.parser, Tk_EndOfFile);
		Node_List list = {.first = result.node, .last = result.last_node};
		t_pred(t, !has_error(result));
		t_pred(t, node_list_cardinality(list) == 3);
		t_pred(t, is_binary(list.first, Tk_Plus));
		t_pred(t, is_identifier(list.first->next, strlit("value")));
		t_pred(t, list.last != NULL
			&& list.last->type == Node_Boolean
			&& !list.last->value.boolean);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("1 2"), &fixture);
		Parser_Result result = parse_expression_list(&fixture.parser, Tk_EndOfFile);
		t_pred(t, has_error(result));
		t_pred(t, result.error.typ == Err_UnexpectedToken);
		t_pred(t, result.error.expected.token_type == Tk_Comma);
		t_pred(t, result.error.got.token_type == Tk_Integer);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("var first, second: [4]^Int = 1 + 2, other;"), &fixture);
		Parser_Result result = parse_var_declaration(&fixture.parser);
		t_pred(t, result.error.typ == Err_None);
		t_pred(t, result.node != NULL && result.node->type == Node_VarDefinition);
		t_pred(t, fixture.parser.ast.root == result.node);
		if(result.node != NULL && result.node->type == Node_VarDefinition){
			Var_Definition* definition = &result.node->value.var_definition;
			t_pred(t, node_list_cardinality(definition->idents) == 2);
			t_pred(t, is_identifier(definition->idents.first, strlit("first")));
			t_pred(t, is_identifier(definition->idents.last, strlit("second")));
			t_pred(t, definition->type != NULL
				&& definition->type->kind == ParserType_Array
				&& definition->type->value.length == 4);
			if(definition->type != NULL && definition->type->kind == ParserType_Array){
				Parser_Type* pointer = definition->type->value.element;
				t_pred(t, pointer != NULL && pointer->kind == ParserType_Pointer);
				if(pointer != NULL && pointer->kind == ParserType_Pointer){
					t_pred(t, is_named_parser_type(pointer->value.element, strlit("Int")));
				}
			}
			t_pred(t, node_list_cardinality(definition->values) == 2);
			t_pred(t, is_binary(definition->values.first, Tk_Plus));
			t_pred(t, is_identifier(definition->values.last, strlit("other")));
			t_pred(t, definition->idents.first->parent == result.node);
			t_pred(t, definition->values.first->parent == result.node);
		}
		t_pred(t, scan_peek_token(&fixture.parser.scanner).token.type == Tk_Semicolon);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("var first, second: Int = 1"), &fixture);
		Parser_Result result = parse_var_declaration(&fixture.parser);
		t_pred(t, result.node == NULL);
		t_pred(t, result.error.typ == Err_MismatchedListCardinality);
		t_pred(t, result.error.expected.cardinality == 2);
		t_pred(t, result.error.got.cardinality == 1);
	}
}
