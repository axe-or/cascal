#include "base.h"
#include "lang.h"

static inline
Node* ast_make_node(AST* ast){
	Node* node = arena_make(ast->arena, Node, 1);
	ensure(node != NULL, "AST arena exhausted");
	return node;
}

static inline
Node* ast_make_unary(AST* ast, Token_Type op, Node* operand){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Unary,
		.value.unary = {
			.op = op,
			.operand = operand,
		},
	};

	operand->parent = node;
	return node;
}

static inline
Node* ast_make_binary(AST* ast, Token_Type op, Node* left, Node* right){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Binary,
		.value.binary = {
			.op = op,
			.left = left,
			.right = right,
		},
	};

    left->next = right;

	left->parent = node;
	right->parent = node;

	return node;
}

static inline
Node* ast_make_integer(AST* ast, i64 value){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Integer,
		.value.integer = value,
	};
	return node;
}

static inline
Node* ast_make_real(AST* ast, f64 value){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Real,
		.value.real = value,
	};
	return node;
}

static inline
Node* ast_make_boolean(AST* ast, bool value){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Boolean,
		.value.boolean = value,
	};
	return node;
}

static inline
Node* ast_make_identifier(AST* ast, String ident){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Identifier,
		.value.ident = ident,
	};
	return node;
}

static inline
String unescape_sequences_in_string(String s, Arena* arena){
	String_Builder sb = sb_make((i32)s.len + 1, arena);

	for(isize i = 0; i < s.len; i += 1){
		if(s.v[i] == '\\'){
			i += 1;
			ensure(i < s.len, "scanner accepted an incomplete escape sequence");
			rune escaped = escape_sequence(s.v[i]);
			ensure(escaped != RUNE_ERROR, "scanner accepted an invalid escape sequence");
			sb_write_rune(&sb, escaped);
			continue;
		}
		sb_write_byte(&sb, (u8)s.v[i]);
	}

	return sb_build(&sb);
}

static inline
Node* ast_make_string(AST* ast, String escaped){
	String value = unescape_sequences_in_string(escaped, ast->arena);
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_String,
		.value.str = value,
	};
	return node;
}

static inline
void node_list_push(Node_List* list, Node* node){
	ensure(node != NULL, "cannot add a null node to a node list");

	node->next = NULL;
	if(list->last != NULL){
		list->last->next = node;
	} else {
		list->first = node;
	}
	list->last = node;
}

static inline
Node* ast_make_var_definition(
	AST* ast,
	Node_List idents,
	Parser_Type* type,
	Node_List values
){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_VarDefinition,
		.value.var_definition = {
			.idents = idents,
			.type = type,
			.values = values,
		},
	};

	for(Node* ident = idents.first; ident != NULL; ident = ident->next){
		ident->parent = node;
	}
	for(Node* value = values.first; value != NULL; value = value->next){
		value->parent = node;
	}
	return node;
}

static inline
Parser_Type* parser_type_make(Parser* parser, Parser_Type_Kind kind){
	Parser_Type* type = arena_make(parser->ast.arena, Parser_Type, 1);
	ensure(type != NULL, "AST arena exhausted");
	type->kind = kind;
	return type;
}

static inline
void call_node_push_arg(Node* node, Node* arg){
    ensure(node && node->type == Node_Call, "not a call node");

    Node_List* args = &node->value.call.args;

    if(!args->first){
        args->first = arg;
        args->last = arg;
    }
    else {
      args->last->next = arg;
      args->last = arg;
    }
    arg->parent = node;
}

static inline
String parser_token_string(Parser const* parser, Token token){
	return (String){
		.v = &parser->scanner.source.v[token.start],
		.len = token.end - token.start,
	};
}

static inline
void parser_set_scanner_error(Parser* parser, Scanner_Result result){
	if(parser->error.typ == Err_None && result.error.typ != Err_None){
		parser->error = result.error;
	}
}

static inline
Token parser_peek(Parser* parser){
	Scanner_Result result = scan_peek_token(&parser->scanner);
	parser_set_scanner_error(parser, result);
	return result.token;
}

static inline
Token parser_next(Parser* parser){
	Scanner_Result result = scan_next_token(&parser->scanner);
	parser_set_scanner_error(parser, result);
	return result.token;
}

static inline
void parser_unexpected(Parser* parser, Token got, Token_Type expected){
	if(parser->error.typ != Err_None){
		return;
	}

	parser->error = (Error){
		.offset = got.start,
		.typ = Err_UnexpectedToken,
		.expected.token_type = expected,
		.got.token_type = got.type,
	};
}

static inline
bool parser_take_if(Parser* parser, Token_Type type){
	Token token = parser_peek(parser);
	if(parser->error.typ != Err_None || token.type != type){
		return false;
	}
	parser_next(parser);
	return true;
}

static inline
bool parser_expect(Parser* parser, Token_Type type){
	if(parser_take_if(parser, type)){
		return true;
	}
	parser_unexpected(parser, parser_peek(parser), type);
	return false;
}

static inline
bool prefix_binding_power(Token_Type op, int* right_bp){
	switch(op){
	case Tk_Plus:
	case Tk_Minus:
	case Tk_Tilde:
	case Tk_LogicNot:
		*right_bp = 80;
		return true;
	default:
		return false;
	}
}

static inline
bool infix_binding_power(Token_Type op, i32* lbp, i32* rbp){
	switch(op){
    case Tk_ParenOpen:
    case Tk_SquareOpen:
        *lbp = 100; *rbp = 101;
        return true;

	case Tk_Dot:
		*lbp = 90; *rbp = 91;
		return true;

	case Tk_Star:
	case Tk_Slash:
	case Tk_Modulo:
	case Tk_And:
	case Tk_ShiftLeft:
	case Tk_ShiftRight:
		*lbp = 70; *rbp = 71;
		return true;

	case Tk_Plus:
	case Tk_Minus:
	case Tk_Or:
	case Tk_Caret:
		*lbp = 60; *rbp = 61;
		return true;

	case Tk_Eq:
	case Tk_Neq:
	case Tk_Gt:
	case Tk_GtEq:
	case Tk_Lt:
	case Tk_LtEq:
		*lbp = 50; *rbp = 51;
		return true;

	case Tk_LogicAnd:
		*lbp = 40; *rbp = 41;
		return true;

	case Tk_LogicOr:
		*lbp = 30; *rbp = 31;
		return true;

	default:
		return false;
	}
}

static inline
Node* parse_expression_bp(Parser* parser, int minimum_bp);

static inline
Node* parse_prefix(Parser* parser){
	Token token = parser_next(parser);
	if(parser->error.typ != Err_None){
		return NULL;
	}

	switch(token.type){
	case Tk_Integer:
		return ast_make_integer(&parser->ast, token.value_int);
	case Tk_Real:
		return ast_make_real(&parser->ast, token.value_real);
	case Tk_True:
		return ast_make_boolean(&parser->ast, true);
	case Tk_False:
		return ast_make_boolean(&parser->ast, false);
	case Tk_Identifier:
		return ast_make_identifier(&parser->ast, parser_token_string(parser, token));
	case Tk_String: {
		String literal = parser_token_string(parser, token);
		String escaped = {
			.v = literal.v + 1,
			.len = literal.len - 2,
		};
		return ast_make_string(&parser->ast, escaped);
	}
	case Tk_ParenOpen: {
		Node* expression = parse_expression_bp(parser, 0);
		if(expression == NULL || !parser_expect(parser, Tk_ParenClose)){
			return NULL;
		}
		return expression;
	}
	default: {
		int right_bp = 0;
		if(prefix_binding_power(token.type, &right_bp)){
			Node* operand = parse_expression_bp(parser, right_bp);
			return (operand == NULL) ? NULL : ast_make_unary(&parser->ast, token.type, operand);
		}
		parser_unexpected(parser, token, Tk_Unknown);
		return NULL;
	}
	}
}

static inline
Node* parse_expression_bp(Parser* parser, int minimum_bp){
	Node* left = parse_prefix(parser);
	if(left == NULL){
		return NULL;
	}

	for(;;){
		Token operator = parser_peek(parser);
		if(parser->error.typ != Err_None){
			return NULL;
		}

		i32 left_bp = 0;
		i32 right_bp = 0;
		if(!infix_binding_power(operator.type, &left_bp, &right_bp)
			|| left_bp < minimum_bp){
			break;
		}

		parser_next(parser);
		Node* right = parse_expression_bp(parser, right_bp);
		if(right == NULL){
			return NULL;
		}
		left = ast_make_binary(&parser->ast, operator.type, left, right);
	}

	return left;
}

attribute_force_inline_func
bool has_error(Parser_Result result){
	return result.error.typ != Err_None;
}

Parser parser_make(String source, Arena* arena){
	return (Parser){
		.scanner.source = source,
		.ast.arena = arena,
	};
}

Parser_Result parse_expression(Parser* parser){
	if(parser->error.typ != Err_None){
		return (Parser_Result){.error = parser->error};
	}

	Node* node = parse_expression_bp(parser, 0);
	if(parser->error.typ == Err_None){
		parser->ast.root = node;
	}

	return (Parser_Result){
		.node = node,
		.error = parser->error,
	};
}

static inline
Parser_Type* parse_type_inner(Parser* parser){
	Token token = parser_next(parser);
	if(parser->error.typ != Err_None){
		return NULL;
	}

	switch(token.type){
	case Tk_Identifier: {
		Parser_Type* type = parser_type_make(parser, ParserType_Named);
		type->value.name = parser_token_string(parser, token);
		return type;
	}
	case Tk_Caret: {
		Parser_Type* element = parse_type_inner(parser);
		if(element == NULL){
			return NULL;
		}

		Parser_Type* type = parser_type_make(parser, ParserType_Pointer);
		type->value.element = element;
		return type;
	}
	case Tk_SquareOpen: {
		if(parser_take_if(parser, Tk_SquareClose)){
			Parser_Type* element = parse_type_inner(parser);
			if(element == NULL){
				return NULL;
			}

			Parser_Type* type = parser_type_make(parser, ParserType_Slice);
			type->value.element = element;
			return type;
		}

		Token length = parser_next(parser);
		if(parser->error.typ != Err_None){
			return NULL;
		}
		if(length.type != Tk_Integer){
			parser_unexpected(parser, length, Tk_Integer);
			return NULL;
		}
		if(!parser_expect(parser, Tk_SquareClose)){
			return NULL;
		}

		Parser_Type* element = parse_type_inner(parser);
		if(element == NULL){
			return NULL;
		}

		Parser_Type* type = parser_type_make(parser, ParserType_Array);
		type->value.element = element;
		type->value.length = length.value_int;
		return type;
	}
	default:
		parser_unexpected(parser, token, Tk_Identifier);
		return NULL;
	}
}

Parser_Type_Result parse_type(Parser* parser){
	if(parser->error.typ != Err_None){
		return (Parser_Type_Result){.error = parser->error};
	}

	Parser_Type* type = parse_type_inner(parser);
	return (Parser_Type_Result){
		.type = type,
		.error = parser->error,
	};
}

Parser_Result parse_identifier_list(Parser* parser){
	Node_List list = {0};

	for(;;){
		Token token = parser_next(parser);
		if(parser->error.typ != Err_None){
			break;
		}
		if(token.type != Tk_Identifier){
			parser_unexpected(parser, token, Tk_Identifier);
			break;
		}

		Node* ident = ast_make_identifier(&parser->ast, parser_token_string(parser, token));
		node_list_push(&list, ident);
		if(!parser_take_if(parser, Tk_Comma)){
			break;
		}
	}

	return (Parser_Result){
		.node = list.first,
		.last_node = list.last,
		.error = parser->error,
	};
}

Parser_Result parse_expression_list(Parser* parser, Token_Type end_delim){
	Node_List list = {0};

	for(;;){
		Parser_Result expression = parse_expression(parser);
		if(has_error(expression)){
			return expression;
		}
		node_list_push(&list, expression.node);

		Token current = parser_peek(parser);
		if(parser->error.typ != Err_None){
			break;
		}
		if(current.type == end_delim || current.type == Tk_EndOfFile){
			break;
		}
		if(current.type != Tk_Comma){
			parser_unexpected(parser, current, Tk_Comma);
			break;
		}

		parser_next(parser);
		Token lookahead = parser_peek(parser);
		if(parser->error.typ != Err_None){
			break;
		}
		if(lookahead.type == end_delim || lookahead.type == Tk_EndOfFile){
			parser_unexpected(parser, lookahead, Tk_Unknown);
			break;
		}
	}

	return (Parser_Result){
		.node = list.first,
		.last_node = list.last,
		.error = parser->error,
	};
}

Parser_Result parse_var_declaration(Parser* parser){
	if(parser->error.typ != Err_None){
		return (Parser_Result){.error = parser->error};
	}
	if(!parser_expect(parser, Tk_Var)){
		return (Parser_Result){.error = parser->error};
	}

	Parser_Result idents = parse_identifier_list(parser);
	if(has_error(idents) || !parser_expect(parser, Tk_Colon)){
		return (Parser_Result){.error = parser->error};
	}

	Parser_Type_Result type = parse_type(parser);
	if(type.error.typ != Err_None || !parser_expect(parser, Tk_Assign)){
		return (Parser_Result){.error = parser->error};
	}

	Parser_Result values = parse_expression_list(parser, Tk_Semicolon);
	if(has_error(values)){
		return (Parser_Result){.error = parser->error};
	}

	Node_List ident_list = {.first = idents.node, .last = idents.last_node};
	Node_List value_list = {.first = values.node, .last = values.last_node};
	i32 ident_count = node_list_cardinality(ident_list);
	i32 value_count = node_list_cardinality(value_list);
	if(ident_count != value_count){
		Token current = parser_peek(parser);
		parser->error = (Error){
			.offset = current.start,
			.typ = Err_MismatchedListCardinality,
			.expected.cardinality = ident_count,
			.got.cardinality = value_count,
		};
		return (Parser_Result){.error = parser->error};
	}

	Node* node = ast_make_var_definition(
		&parser->ast,
		ident_list,
		type.type,
		value_list
	);
	parser->ast.root = node;
	return (Parser_Result){.node = node};
}

typedef struct {
	IO_Writer writer;
	isize written;
	isize error;
} Node_Format_Context;

attribute_format(2, 3)
static bool node_format_write(Node_Format_Context* context, char const* fmt, ...){
	if(context->error < 0){
		return false;
	}

	va_list args;
	va_start(args, fmt);
	isize result = fmt_writev(context->writer, fmt, args);
	va_end(args);

	if(result < 0){
		context->error = result;
		return false;
	}
	context->written += result;
	return true;
}

static inline
void write_quoted_string(Node_Format_Context* context, String value){
	node_format_write(context, "\"");
	isize chunk_start = 0;

	for(isize i = 0; i < value.len && context->error == 0; i += 1){
		char const* escape = NULL;
		switch(value.v[i]){
		case '\t': escape = "\\t"; break;
		case '\r': escape = "\\r"; break;
		case '\n': escape = "\\n"; break;
		case '"':  escape = "\\\""; break;
		case '\\': escape = "\\\\"; break;
		}

		if(escape == NULL){
			continue;
		}
		if(i > chunk_start){
			node_format_write(context, "%.*s", (int)(i - chunk_start), &value.v[chunk_start]);
		}
		node_format_write(context, "%s", escape);
		chunk_start = i + 1;
	}

	if(context->error == 0 && chunk_start < value.len){
		node_format_write(
			context,
			"%.*s",
			(int)(value.len - chunk_start),
			&value.v[chunk_start]
		);
	}
	node_format_write(context, "\"");
}

static
void node_format_ctx(Node_Format_Context* context, Node* node){
	if(context->error != 0){
		return;
	}
	if(node == NULL){
		context->error = IO_Err_Other;
		return;
	}

	switch(node->type){
	case Node_Integer:
		node_format_write(context, "%lld", (long long)node->value.integer);
		break;
	case Node_Real:
		node_format_write(context, "%.17g", node->value.real);
		break;
	case Node_Boolean:
		node_format_write(context, node->value.boolean ? "true" : "false");
		break;
	case Node_String:
		write_quoted_string(context, node->value.str);
		break;
	case Node_Identifier:
		node_format_write(context, "%.*s", strf(node->value.ident));
		break;
	case Node_Unary:
		node_format_write(context, "(%.*s ", strf(token_type_name(node->value.unary.op)));
		node_format_ctx(context, node->value.unary.operand);
		node_format_write(context, ")");
		break;
	case Node_Binary:
		node_format_write(context, "(%.*s ", strf(token_type_name(node->value.binary.op)));
		node_format_ctx(context, node->value.binary.left);
		node_format_write(context, " ");
		node_format_ctx(context, node->value.binary.right);
		node_format_write(context, ")");
		break;
	case Node_Call:
		node_format_write(context, "(call ");
		node_format_ctx(context, node->value.call.callable);
		for(Node* arg = node->value.call.args.first; arg != NULL; arg = arg->next){
			node_format_write(context, " ");
			node_format_ctx(context, arg);
		}
		node_format_write(context, ")");
		break;
	case Node_Index:
		node_format_write(context, "([] ");
		node_format_ctx(context, node->value.index.object);
		node_format_write(context, " ");
		node_format_ctx(context, node->value.index.idx);
		node_format_write(context, ")");
		break;

	default:
		panic("invalid node type");
	}
}

isize node_format(IO_Writer writer, Node* node){
	Node_Format_Context context = {.writer = writer};
	node_format_ctx(&context, node);
	return context.error < 0 ? context.error : context.written;
}

Error parse(String source, AST* ast, Arena* arena){
	panic("todo!");
}