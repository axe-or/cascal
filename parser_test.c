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

static bool is_named_parser_type(Node const* type, String name){
	return type != NULL
		&& type->type == Node_ParserType
		&& type->value.parser_type.kind == ParserType_Named
		&& str_equal(type->value.parser_type.value.name, name);
}

static void expect_node_format(Test* t, Node* node, String expected){
	u8 memory[4096];
	Arena arena = arena_from_buffer(memory, sizeof(memory));
	String_Builder builder = sb_make(64, &arena);
	isize written = node_format(sb_writer(&builder), node);
	String actual = sb_get(&builder);

	t_pred(t, written == expected.len);
	t_pred(t, str_equal(actual, expected));
}

void parser_tests(Test* t){
	t->name = "parser";

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("1 + 2 * 3"), &fixture);
		t_pred(t, result.error.type == Err_None);
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
		t_pred(t, result.error.type == Err_None);
		t_pred(t, is_binary(result.node, Tk_Minus));
		if(is_binary(result.node, Tk_Minus)){
			t_pred(t, is_binary(result.node->value.binary.left, Tk_Minus));
			t_pred(t, is_integer(result.node->value.binary.right, 2));
		}
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("a = b"), &fixture);
		t_pred(t, result.error.type == Err_None);
		t_pred(t, result.node != NULL && result.node->type == Node_Identifier);
		t_pred(t, scan_peek_token(&fixture.parser.scanner).token.type == Tk_Assign);
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("1 + 2 << 3 & 4"), &fixture);
		t_pred(t, result.error.type == Err_None);
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
		t_pred(t, result.error.type == Err_None);
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
		t_pred(t, result.error.type == Err_None);
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
		t_pred(t, result.error.type == Err_None);
		t_pred(t, result.node != NULL && result.node->type == Node_String);
		if(result.node != NULL && result.node->type == Node_String){
			t_pred(t, str_equal(result.node->value.str, strlit("line\nnext")));
		}
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("(1 + 2"), &fixture);
		t_pred(t, result.node == NULL);
		t_pred(t, result.error.type == Err_UnexpectedToken);
		t_pred(t, result.error.expected.token_type == Tk_ParenClose);
		t_pred(t, result.error.got.token_type == Tk_EndOfFile);
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("1 +"), &fixture);
		t_pred(t, result.node == NULL);
		t_pred(t, result.error.type == Err_UnexpectedToken);
		t_pred(t, result.error.got.token_type == Tk_EndOfFile);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("[32][]^Item"), &fixture);
		Parser_Result result = parse_type(&fixture.parser);
		t_pred(t, result.error.type == Err_None);
		t_pred(t, result.node != NULL
			&& result.node->type == Node_ParserType
			&& result.node->value.parser_type.kind == ParserType_Array);
		if(result.node != NULL
			&& result.node->type == Node_ParserType
			&& result.node->value.parser_type.kind == ParserType_Array){
			t_pred(t, result.node->value.parser_type.value.length == 32);
			Node* slice = result.node->value.parser_type.value.element;
			t_pred(t, slice != NULL
				&& slice->type == Node_ParserType
				&& slice->value.parser_type.kind == ParserType_Slice);
			t_pred(t, slice != NULL && slice->parent == result.node);
			if(slice != NULL
				&& slice->type == Node_ParserType
				&& slice->value.parser_type.kind == ParserType_Slice){
				Node* pointer = slice->value.parser_type.value.element;
				t_pred(t, pointer != NULL
					&& pointer->type == Node_ParserType
					&& pointer->value.parser_type.kind == ParserType_Pointer);
				t_pred(t, pointer != NULL && pointer->parent == slice);
				if(pointer != NULL
					&& pointer->type == Node_ParserType
					&& pointer->value.parser_type.kind == ParserType_Pointer){
					t_pred(t, is_named_parser_type(
						pointer->value.parser_type.value.element,
						strlit("Item")
					));
				}
			}
		}
		t_pred(t, scan_peek_token(&fixture.parser.scanner).token.type == Tk_EndOfFile);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("[]"), &fixture);
		Parser_Result result = parse_type(&fixture.parser);
		t_pred(t, result.node == NULL);
		t_pred(t, result.error.type == Err_UnexpectedToken);
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
		t_pred(t, result.error.type == Err_UnexpectedToken);
		t_pred(t, result.error.expected.token_type == Tk_Comma);
		t_pred(t, result.error.got.token_type == Tk_Integer);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("var first, second: [4]^Int = 1 + 2, other;"), &fixture);
		Parser_Result result = parse_var_declaration(&fixture.parser);
		t_pred(t, result.error.type == Err_None);
		t_pred(t, result.node != NULL && result.node->type == Node_VarDefinition);
		t_pred(t, fixture.parser.ast.root == result.node);
		if(result.node != NULL && result.node->type == Node_VarDefinition){
			Var_Definition* definition = &result.node->value.var_definition;
			t_pred(t, node_list_cardinality(definition->idents) == 2);
			t_pred(t, is_identifier(definition->idents.first, strlit("first")));
			t_pred(t, is_identifier(definition->idents.last, strlit("second")));
			t_pred(t, definition->type != NULL
				&& definition->type->type == Node_ParserType
				&& definition->type->value.parser_type.kind == ParserType_Array
				&& definition->type->value.parser_type.value.length == 4);
			if(definition->type != NULL
				&& definition->type->type == Node_ParserType
				&& definition->type->value.parser_type.kind == ParserType_Array){
				Node* pointer = definition->type->value.parser_type.value.element;
				t_pred(t, pointer != NULL
					&& pointer->type == Node_ParserType
					&& pointer->value.parser_type.kind == ParserType_Pointer);
				if(pointer != NULL
					&& pointer->type == Node_ParserType
					&& pointer->value.parser_type.kind == ParserType_Pointer){
					t_pred(t, is_named_parser_type(
						pointer->value.parser_type.value.element,
						strlit("Int")
					));
				}
			}
			t_pred(t, node_list_cardinality(definition->values) == 2);
			t_pred(t, is_binary(definition->values.first, Tk_Plus));
			t_pred(t, is_identifier(definition->values.last, strlit("other")));
			t_pred(t, definition->idents.first->parent == result.node);
			t_pred(t, definition->type->parent == result.node);
			t_pred(t, definition->values.first->parent == result.node);
		}
		t_pred(t, scan_peek_token(&fixture.parser.scanner).token.type == Tk_Semicolon);
	}

	{
		Parser_Fixture fixture;
		parser_fixture_init(strlit("var first, second: Int = 1"), &fixture);
		Parser_Result result = parse_var_declaration(&fixture.parser);
		t_pred(t, result.node == NULL);
		t_pred(t, result.error.type == Err_MismatchedListCardinality);
		t_pred(t, result.error.expected.cardinality == 2);
		t_pred(t, result.error.got.cardinality == 1);
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("1 + 2 * -value"), &fixture);
		t_pred(t, !has_error(result));
		expect_node_format(t, result.node, strlit("(+ 1 (* 2 (- value)))"));
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(
			strlit("\"line\\n\\\"quoted\\\"\\\\\""),
			&fixture
		);
		t_pred(t, !has_error(result));
		expect_node_format(t, result.node, strlit("\"line\\n\\\"quoted\\\"\\\\\""));
	}

	{
		Node object = {
			.type = Node_Identifier,
			.value.ident = strlit("items"),
		};
		Node idx = {
			.type = Node_Integer,
			.value.integer = 2,
		};
		Node index = {
			.type = Node_Index,
			.value.index = {
				.object = &object,
				.idx = &idx,
			},
		};
		Node first_arg = {
			.next = NULL,
			.type = Node_Real,
			.value.real = 3.5,
		};
		Node second_arg = {
			.type = Node_Boolean,
			.value.boolean = false,
		};
		first_arg.next = &second_arg;
		Node call = {
			.type = Node_Call,
			.value.call = {
				.callable = &index,
				.args = {.first = &first_arg, .last = &second_arg},
			},
		};

		expect_node_format(t, &call, strlit("(call ([] items 2) 3.5 false)"));
	}

	{
		Parser_Fixture fixture;
		Parser_Result result = parse_test_expression(strlit("items(1, value,)[2]"), &fixture);
		t_pred(t, !has_error(result));
		t_pred(t, result.node != NULL && result.node->type == Node_Index);
		if(result.node != NULL && result.node->type == Node_Index){
			Index* index = &result.node->value.index;
			t_pred(t, is_integer(index->idx, 2));
			t_pred(t, index->object != NULL && index->object->type == Node_Call);
			if(index->object != NULL && index->object->type == Node_Call){
				Call* call = &index->object->value.call;
				t_pred(t, is_identifier(call->callable, strlit("items")));
				t_pred(t, node_list_cardinality(call->args) == 2);
				t_pred(t, is_integer(call->args.first, 1));
				t_pred(t, is_identifier(call->args.last, strlit("value")));
			}
		}
	}

	{
		u8 memory[64 * 1024];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		AST ast = {0};
		Error error = parse(
			strlit(
				"proc foo(a, b, c: int, d: bool) -> (int, bool) {"
				"  var x, y: int = 1, 2;"
				"  x, y = y, x;"
				"  while x {"
				"    if y { break done; } else { continue; }"
				"  }"
				"  return a, d;"
				"}"
				"proc bar(,) -> ^int {}"
			),
			&ast,
			&arena
		);
		t_pred(t, error.type == Err_None);
		t_pred(t, ast.root != NULL && ast.root->type == Node_ProcDefinition);
		if(ast.root != NULL && ast.root->type == Node_ProcDefinition){
			Node* foo_node = ast.root;
			Proc_Definition* foo = &foo_node->value.proc_definition;
			t_pred(t, str_equal(foo->name, strlit("foo")));
			t_pred(t, node_list_cardinality(foo->args) == 4);

			Node* a = foo->args.first;
			Node* b = a == NULL ? NULL : a->next;
			Node* c = b == NULL ? NULL : b->next;
			Node* d = c == NULL ? NULL : c->next;
			t_pred(t, a != NULL && a->type == Node_Field);
			t_pred(t, b != NULL && b->type == Node_Field);
			t_pred(t, c != NULL && c->type == Node_Field);
			t_pred(t, d != NULL && d->type == Node_Field);
			if(a != NULL && b != NULL && c != NULL && d != NULL){
				t_pred(t, str_equal(a->value.field.identifier, strlit("a")));
				t_pred(t, str_equal(b->value.field.identifier, strlit("b")));
				t_pred(t, str_equal(c->value.field.identifier, strlit("c")));
				t_pred(t, str_equal(d->value.field.identifier, strlit("d")));
				t_pred(t, a->value.field.type == b->value.field.type);
				t_pred(t, b->value.field.type == c->value.field.type);
				t_pred(t, is_named_parser_type(c->value.field.type, strlit("int")));
				t_pred(t, is_named_parser_type(d->value.field.type, strlit("bool")));
				t_pred(t, a->parent == foo_node && d->parent == foo_node);
			}

			t_pred(t, node_list_cardinality(foo->returns) == 2);
			t_pred(t, foo->returns.first != NULL
				&& foo->returns.first->type == Node_ParserType
				&& is_named_parser_type(foo->returns.first, strlit("int")));
			t_pred(t, foo->returns.last != NULL
				&& foo->returns.last->type == Node_ParserType
				&& is_named_parser_type(foo->returns.last, strlit("bool")));

			t_pred(t, foo->body != NULL && foo->body->type == Node_Block);
			if(foo->body != NULL && foo->body->type == Node_Block){
				Node_List statements = foo->body->value.block.statements;
				t_pred(t, node_list_cardinality(statements) == 4);
				t_pred(t, statements.first != NULL
					&& statements.first->type == Node_VarDefinition);
				t_pred(t, statements.first != NULL
					&& statements.first->next != NULL
					&& statements.first->next->type == Node_Assignment);
				t_pred(t, statements.last != NULL && statements.last->type == Node_Return);
			}

			Node* bar_node = foo_node->next;
			t_pred(t, bar_node != NULL && bar_node->type == Node_ProcDefinition);
			if(bar_node != NULL && bar_node->type == Node_ProcDefinition){
				Proc_Definition* bar = &bar_node->value.proc_definition;
				t_pred(t, str_equal(bar->name, strlit("bar")));
				t_pred(t, bar->args.first == NULL && bar->args.last == NULL);
				t_pred(t, node_list_cardinality(bar->returns) == 1);
				t_pred(t, bar->returns.first != NULL
					&& bar->returns.first->type == Node_ParserType
					&& bar->returns.first->value.parser_type.kind == ParserType_Pointer
					&& is_named_parser_type(
						bar->returns.first->value.parser_type.value.element,
						strlit("int")
					));
				t_pred(t, bar_node->next == NULL);
				expect_node_format(t, bar_node, strlit("(proc bar () (^int) (block))"));
			}
		}
	}

	{
		u8 memory[4096];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		AST ast = {0};
		Error error = parse(strlit("var value: int = 1;"), &ast, &arena);
		t_pred(t, error.type == Err_UnexpectedToken);
		t_pred(t, error.expected.token_type == Tk_Proc);
		t_pred(t, error.got.token_type == Tk_Var);
		t_pred(t, ast.root == NULL);
	}
}
