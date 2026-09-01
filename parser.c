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
	case Tk_Dot:
		*lbp = 90;
		*rbp = 91;
		return true;

	case Tk_Star:
	case Tk_Slash:
	case Tk_Modulo:
	case Tk_And:
	case Tk_ShiftLeft:
	case Tk_ShiftRight:
		*lbp = 70;
		*rbp = 71;
		return true;

	case Tk_Plus:
	case Tk_Minus:
	case Tk_Or:
	case Tk_Caret:
		*lbp = 60;
		*rbp = 61;
		return true;

	case Tk_Eq:
	case Tk_Neq:
	case Tk_Gt:
	case Tk_GtEq:
	case Tk_Lt:
	case Tk_LtEq:
		*lbp = 50;
		*rbp = 51;
		return true;

	case Tk_LogicAnd:
		*lbp = 40;
		*rbp = 41;
		return true;

	case Tk_LogicOr:
		*lbp = 30;
		*rbp = 31;
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
