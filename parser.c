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
	Node* type,
	Node_List values
){
	ensure(type != NULL && type->type == Node_ParserType,
		"variable definition type must be a type node");

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
	type->parent = node;
	return node;
}

static inline
void node_list_set_parent(Node_List list, Node* parent){
	for(Node* node = list.first; node != NULL; node = node->next){
		node->parent = parent;
	}
}

static inline
Node* ast_make_index(AST* ast, Node* object, Node* idx){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Index,
		.value.index = {
			.object = object,
			.idx = idx,
		},
	};
	object->parent = node;
	idx->parent = node;
	return node;
}

static inline
Node* ast_make_call(AST* ast, Node* callable, Node_List args){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Call,
		.value.call = {
			.callable = callable,
			.args = args,
		},
	};
	callable->parent = node;
	node_list_set_parent(args, node);
	return node;
}

static inline
Node* ast_make_field(AST* ast, String identifier, Node* type){
	ensure(identifier.len > 0, "field name must not be empty");
	ensure(type != NULL && type->type == Node_ParserType, "field must have a type node");

	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Field,
		.value.field = {
			.identifier = identifier,
			.type = type,
		},
	};
	return node;
}

static inline
Node* ast_make_parser_type_node(AST* ast, Parser_Type type){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_ParserType,
		.value.parser_type = type,
	};
	if(type.kind != ParserType_Named){
		ensure(type.value.element != NULL
			&& type.value.element->type == Node_ParserType,
			"compound type must have a type node element");
		type.value.element->parent = node;
	}
	return node;
}

static inline
Node* ast_make_assignment(AST* ast, Node_List left, Node_List right){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Assignment,
		.value.assignment = {
			.left = left,
			.right = right,
		},
	};
	node_list_set_parent(left, node);
	node_list_set_parent(right, node);
	return node;
}

static inline
Node* ast_make_block(AST* ast, Node_List statements){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Block,
		.value.block.statements = statements,
	};
	node_list_set_parent(statements, node);
	return node;
}

static inline
Node* ast_make_return(AST* ast, Node_List values){
	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_Return,
		.value.return_statement.values = values,
	};
	node_list_set_parent(values, node);
	return node;
}

static inline
Node* ast_make_branch_control(AST* ast, Node_Type type, String label){
	ensure(type == Node_Break || type == Node_Continue, "invalid branch control node type");

	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = type,
		.value.ident = label,
	};
	return node;
}

static inline
Node* ast_make_if(AST* ast, Node* condition, Node* then_block, Node* else_branch){
	ensure(then_block != NULL && then_block->type == Node_Block, "if body must be a block");
	ensure(else_branch == NULL
		|| else_branch->type == Node_Block
		|| else_branch->type == Node_If,
		"else branch must be a block or if statement");

	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_If,
		.value.if_statement = {
			.condition = condition,
			.then_block = then_block,
			.else_branch = else_branch,
		},
	};
	condition->parent = node;
	then_block->parent = node;
	if(else_branch != NULL){
		else_branch->parent = node;
	}
	return node;
}

static inline
Node* ast_make_while(AST* ast, Node* condition, Node* body){
	ensure(body != NULL && body->type == Node_Block, "while body must be a block");

	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_While,
		.value.while_statement = {
			.condition = condition,
			.body = body,
		},
	};
	condition->parent = node;
	body->parent = node;
	return node;
}

static inline
Node* ast_make_proc_definition(
	AST* ast,
	String name,
	Node_List args,
	Node_List returns,
	Node* body
){
	ensure(name.len > 0, "procedure name must not be empty");
	ensure(body != NULL && body->type == Node_Block, "procedure body must be a block");
	for(Node* field = args.first; field != NULL; field = field->next){
		ensure(field->type == Node_Field, "procedure parameter must be a field");
	}
	for(Node* return_type = returns.first; return_type != NULL; return_type = return_type->next){
		ensure(return_type->type == Node_ParserType, "procedure result must be a type node");
	}

	Node* node = ast_make_node(ast);
	*node = (Node){
		.type = Node_ProcDefinition,
		.value.proc_definition = {
			.name = name,
			.args = args,
			.returns = returns,
			.body = body,
		},
	};
	node_list_set_parent(args, node);
	node_list_set_parent(returns, node);
	body->parent = node;
	return node;
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
		if(operator.type == Tk_ParenOpen){
			Node_List args = {0};
			if(!parser_take_if(parser, Tk_ParenClose)){
				for(;;){
					Node* arg = parse_expression_bp(parser, 0);
					if(arg == NULL){
						return NULL;
					}
					node_list_push(&args, arg);

					if(!parser_take_if(parser, Tk_Comma)){
						if(!parser_expect(parser, Tk_ParenClose)){
							return NULL;
						}
						break;
					}
					if(parser_take_if(parser, Tk_ParenClose)){
						break;
					}
				}
			}
			left = ast_make_call(&parser->ast, left, args);
			continue;
		}

		if(operator.type == Tk_SquareOpen){
			Node* idx = parse_expression_bp(parser, 0);
			if(idx == NULL || !parser_expect(parser, Tk_SquareClose)){
				return NULL;
			}
			left = ast_make_index(&parser->ast, left, idx);
			continue;
		}

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
Node* parse_type_inner(Parser* parser){
	Token token = parser_next(parser);
	if(parser->error.typ != Err_None){
		return NULL;
	}

	switch(token.type){
	case Tk_Identifier:
		return ast_make_parser_type_node(
			&parser->ast,
			(Parser_Type){
				.value.name = parser_token_string(parser, token),
				.kind = ParserType_Named,
			}
		);
	case Tk_Caret: {
		Node* element = parse_type_inner(parser);
		if(element == NULL){
			return NULL;
		}

		return ast_make_parser_type_node(
			&parser->ast,
			(Parser_Type){
				.value.element = element,
				.kind = ParserType_Pointer,
			}
		);
	}
	case Tk_SquareOpen: {
		if(parser_take_if(parser, Tk_SquareClose)){
			Node* element = parse_type_inner(parser);
			if(element == NULL){
				return NULL;
			}

			return ast_make_parser_type_node(
				&parser->ast,
				(Parser_Type){
					.value.element = element,
					.kind = ParserType_Slice,
				}
			);
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

		Node* element = parse_type_inner(parser);
		if(element == NULL){
			return NULL;
		}

		return ast_make_parser_type_node(
			&parser->ast,
			(Parser_Type){
				.value = {
					.element = element,
					.length = length.value_int,
				},
				.kind = ParserType_Array,
			}
		);
	}
	default:
		parser_unexpected(parser, token, Tk_Identifier);
		return NULL;
	}
}

Parser_Result parse_type(Parser* parser){
	if(parser->error.typ != Err_None){
		return (Parser_Result){.error = parser->error};
	}

	Node* type = parse_type_inner(parser);
	return (Parser_Result){
		.node = type,
		.last_node = type,
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

	Parser_Result type = parse_type(parser);
	if(has_error(type) || !parser_expect(parser, Tk_Assign)){
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
		type.node,
		value_list
	);
	parser->ast.root = node;
	return (Parser_Result){.node = node};
}

static inline
void parser_cardinality_error(Parser* parser, i32 expected, i32 got){
	Token current = parser_peek(parser);
	parser->error = (Error){
		.offset = current.start,
		.typ = Err_MismatchedListCardinality,
		.expected.cardinality = expected,
		.got.cardinality = got,
	};
}

static inline
Parser_Result parse_field_list(Parser* parser){
	Node_List fields = {0};

	for(;;){
		Parser_Result idents = parse_identifier_list(parser);
		if(has_error(idents) || !parser_expect(parser, Tk_Colon)){
			break;
		}

		Parser_Result type = parse_type(parser);
		if(has_error(type)){
			break;
		}

		for(Node* ident = idents.node; ident != NULL;){
			Node* next = ident->next;
			ident->next = NULL;
			ensure(ident->type == Node_Identifier, "field name must be an identifier");
			Node* field = ast_make_field(&parser->ast, ident->value.ident, type.node);
			node_list_push(&fields, field);
			ident = next;
		}

		if(!parser_take_if(parser, Tk_Comma)){
			break;
		}
		if(parser_peek(parser).type == Tk_ParenClose){
			break;
		}
	}

	return (Parser_Result){
		.node = fields.first,
		.last_node = fields.last,
		.error = parser->error,
	};
}

static inline
Parser_Result parse_proc_return_types(Parser* parser){
	if(!parser_take_if(parser, Tk_Arrow)){
		return (Parser_Result){0};
	}

	if(!parser_take_if(parser, Tk_ParenOpen)){
		Parser_Result result = parse_type(parser);
		if(has_error(result)){
			return result;
		}
		return result;
	}

	Node_List types = {0};
	for(;;){
		Parser_Result result = parse_type(parser);
		if(has_error(result)){
			return result;
		}
		node_list_push(&types, result.node);

		if(!parser_take_if(parser, Tk_Comma)){
			break;
		}
	}

	if(!parser_expect(parser, Tk_ParenClose)){
		return (Parser_Result){.error = parser->error};
	}
	return (Parser_Result){
		.node = types.first,
		.last_node = types.last,
	};
}

static inline
Parser_Result parse_proc_definition(Parser* parser);

static inline
Parser_Result parse_block(Parser* parser);

static inline
Parser_Result parse_statement(Parser* parser);

static inline
Parser_Result parse_expression_or_assignment(Parser* parser){
	Parser_Result first = parse_expression(parser);
	if(has_error(first)){
		return first;
	}

	Node_List left = {0};
	node_list_push(&left, first.node);
	while(parser_take_if(parser, Tk_Comma)){
		Parser_Result expression = parse_expression(parser);
		if(has_error(expression)){
			return expression;
		}
		node_list_push(&left, expression.node);
	}

	if(!parser_take_if(parser, Tk_Assign)){
		if(left.first != left.last){
			parser_unexpected(parser, parser_peek(parser), Tk_Assign);
			return (Parser_Result){.error = parser->error};
		}
		return (Parser_Result){.node = left.first};
	}

	Parser_Result right_result = parse_expression_list(parser, Tk_Semicolon);
	if(has_error(right_result)){
		return right_result;
	}
	Node_List right = {
		.first = right_result.node,
		.last = right_result.last_node,
	};

	i32 left_count = node_list_cardinality(left);
	i32 right_count = node_list_cardinality(right);
	if(left_count != right_count){
		parser_cardinality_error(parser, left_count, right_count);
		return (Parser_Result){.error = parser->error};
	}

	return (Parser_Result){.node = ast_make_assignment(&parser->ast, left, right)};
}

static inline
Parser_Result parse_return_statement(Parser* parser){
	if(!parser_expect(parser, Tk_Return)){
		return (Parser_Result){.error = parser->error};
	}

	Node_List values = {0};
	if(parser_peek(parser).type != Tk_Semicolon){
		Parser_Result result = parse_expression_list(parser, Tk_Semicolon);
		if(has_error(result)){
			return result;
		}
		values = (Node_List){.first = result.node, .last = result.last_node};
	}
	return (Parser_Result){.node = ast_make_return(&parser->ast, values)};
}

static inline
Parser_Result parse_branch_control(Parser* parser, Token_Type token_type){
	Node_Type node_type = token_type == Tk_Break ? Node_Break : Node_Continue;
	if(!parser_expect(parser, token_type)){
		return (Parser_Result){.error = parser->error};
	}

	String label = {0};
	Token token = parser_peek(parser);
	if(token.type == Tk_Identifier){
		parser_next(parser);
		label = parser_token_string(parser, token);
	}
	return (Parser_Result){.node = ast_make_branch_control(&parser->ast, node_type, label)};
}

static inline
Parser_Result parse_if_statement(Parser* parser){
	if(!parser_expect(parser, Tk_If)){
		return (Parser_Result){.error = parser->error};
	}

	Parser_Result condition = parse_expression(parser);
	if(has_error(condition)){
		return condition;
	}
	Parser_Result then_block = parse_block(parser);
	if(has_error(then_block)){
		return then_block;
	}

	Node* else_branch = NULL;
	if(parser_take_if(parser, Tk_Else)){
		Parser_Result result = parser_peek(parser).type == Tk_If
			? parse_if_statement(parser)
			: parse_block(parser);
		if(has_error(result)){
			return result;
		}
		else_branch = result.node;
	}

	return (Parser_Result){
		.node = ast_make_if(&parser->ast, condition.node, then_block.node, else_branch),
	};
}

static inline
Parser_Result parse_while_statement(Parser* parser){
	if(!parser_expect(parser, Tk_While)){
		return (Parser_Result){.error = parser->error};
	}

	Parser_Result condition = parse_expression(parser);
	if(has_error(condition)){
		return condition;
	}
	Parser_Result body = parse_block(parser);
	if(has_error(body)){
		return body;
	}

	return (Parser_Result){
		.node = ast_make_while(&parser->ast, condition.node, body.node),
	};
}

static inline
Parser_Result parse_statement(Parser* parser){
	Token token = parser_peek(parser);
	if(parser->error.typ != Err_None){
		return (Parser_Result){.error = parser->error};
	}

	Parser_Result result = {0};
	bool needs_semicolon = false;
	switch(token.type){
	case Tk_Var:
		result = parse_var_declaration(parser);
		needs_semicolon = true;
		break;
	case Tk_Return:
		result = parse_return_statement(parser);
		needs_semicolon = true;
		break;
	case Tk_Break:
	case Tk_Continue:
		result = parse_branch_control(parser, token.type);
		needs_semicolon = true;
		break;
	case Tk_If:
		result = parse_if_statement(parser);
		break;
	case Tk_While:
		result = parse_while_statement(parser);
		break;
	case Tk_Proc:
		result = parse_proc_definition(parser);
		break;
	default:
		result = parse_expression_or_assignment(parser);
		needs_semicolon = true;
		break;
	}

	if(has_error(result)){
		return result;
	}
	if(needs_semicolon && !parser_expect(parser, Tk_Semicolon)){
		return (Parser_Result){.error = parser->error};
	}
	return result;
}

static inline
Parser_Result parse_block(Parser* parser){
	if(!parser_expect(parser, Tk_CurlyOpen)){
		return (Parser_Result){.error = parser->error};
	}

	Node_List statements = {0};
	while(parser_peek(parser).type != Tk_CurlyClose){
		if(parser_peek(parser).type == Tk_EndOfFile){
			parser_unexpected(parser, parser_peek(parser), Tk_CurlyClose);
			return (Parser_Result){.error = parser->error};
		}
		Parser_Result statement = parse_statement(parser);
		if(has_error(statement)){
			return statement;
		}
		node_list_push(&statements, statement.node);
	}

	if(!parser_expect(parser, Tk_CurlyClose)){
		return (Parser_Result){.error = parser->error};
	}
	return (Parser_Result){.node = ast_make_block(&parser->ast, statements)};
}

static inline
Parser_Result parse_proc_definition(Parser* parser){
	if(!parser_expect(parser, Tk_Proc)){
		return (Parser_Result){.error = parser->error};
	}

	Token name_token = parser_next(parser);
	if(name_token.type != Tk_Identifier){
		parser_unexpected(parser, name_token, Tk_Identifier);
		return (Parser_Result){.error = parser->error};
	}
	String name = parser_token_string(parser, name_token);

	if(!parser_expect(parser, Tk_ParenOpen)){
		return (Parser_Result){.error = parser->error};
	}

	Node_List fields = {0};
	if(parser_peek(parser).type != Tk_ParenClose){
		if(!parser_take_if(parser, Tk_Comma)){
			Parser_Result result = parse_field_list(parser);
			if(has_error(result)){
				return result;
			}
			fields = (Node_List){.first = result.node, .last = result.last_node};
		}
	}
	if(!parser_expect(parser, Tk_ParenClose)){
		return (Parser_Result){.error = parser->error};
	}

	Parser_Result returns = parse_proc_return_types(parser);
	if(has_error(returns)){
		return returns;
	}
	Parser_Result body = parse_block(parser);
	if(has_error(body)){
		return body;
	}

	Node* node = ast_make_proc_definition(
		&parser->ast,
		name,
		fields,
		(Node_List){.first = returns.node, .last = returns.last_node},
		body.node
	);
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
void parser_type_format_ctx(Node_Format_Context* context, Node* node){
	if(context->error != 0){
		return;
	}
	if(node == NULL || node->type != Node_ParserType){
		context->error = IO_Err_Other;
		return;
	}

	Parser_Type type = node->value.parser_type;
	switch(type.kind){
	case ParserType_Named:
		node_format_write(context, "%.*s", strf(type.value.name));
		break;
	case ParserType_Slice:
		node_format_write(context, "[]");
		parser_type_format_ctx(context, type.value.element);
		break;
	case ParserType_Array:
		node_format_write(context, "[%u]", type.value.length);
		parser_type_format_ctx(context, type.value.element);
		break;
	case ParserType_Pointer:
		node_format_write(context, "^");
		parser_type_format_ctx(context, type.value.element);
		break;
	default:
		context->error = IO_Err_Other;
		break;
	}
}

static
void node_format_ctx(Node_Format_Context* context, Node* node);

static
void node_list_format_ctx(
	Node_Format_Context* context,
	Node_List list,
	bool leading_space
){
	for(Node* node = list.first; node != NULL; node = node->next){
		if(leading_space){
			node_format_write(context, " ");
		}
		node_format_ctx(context, node);
		leading_space = true;
	}
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
	case Node_Field:
		node_format_write(context, "(field %.*s ", strf(node->value.field.identifier));
		parser_type_format_ctx(context, node->value.field.type);
		node_format_write(context, ")");
		break;
	case Node_ParserType:
		parser_type_format_ctx(context, node);
		break;
	case Node_VarDefinition:
		node_format_write(context, "(var (");
		node_list_format_ctx(context, node->value.var_definition.idents, false);
		node_format_write(context, ") ");
		parser_type_format_ctx(context, node->value.var_definition.type);
		node_format_write(context, " (");
		node_list_format_ctx(context, node->value.var_definition.values, false);
		node_format_write(context, "))");
		break;
	case Node_Assignment:
		node_format_write(context, "(= (");
		node_list_format_ctx(context, node->value.assignment.left, false);
		node_format_write(context, ") (");
		node_list_format_ctx(context, node->value.assignment.right, false);
		node_format_write(context, "))");
		break;
	case Node_Block:
		node_format_write(context, "(block");
		node_list_format_ctx(context, node->value.block.statements, true);
		node_format_write(context, ")");
		break;
	case Node_Return:
		node_format_write(context, "(return");
		node_list_format_ctx(context, node->value.return_statement.values, true);
		node_format_write(context, ")");
		break;
	case Node_Break:
	case Node_Continue:
		node_format_write(
			context,
			node->type == Node_Break ? "(break" : "(continue"
		);
		if(node->value.ident.len > 0){
			node_format_write(context, " %.*s", strf(node->value.ident));
		}
		node_format_write(context, ")");
		break;
	case Node_If:
		node_format_write(context, "(if ");
		node_format_ctx(context, node->value.if_statement.condition);
		node_format_write(context, " ");
		node_format_ctx(context, node->value.if_statement.then_block);
		if(node->value.if_statement.else_branch != NULL){
			node_format_write(context, " ");
			node_format_ctx(context, node->value.if_statement.else_branch);
		}
		node_format_write(context, ")");
		break;
	case Node_While:
		node_format_write(context, "(while ");
		node_format_ctx(context, node->value.while_statement.condition);
		node_format_write(context, " ");
		node_format_ctx(context, node->value.while_statement.body);
		node_format_write(context, ")");
		break;
	case Node_ProcDefinition:
		node_format_write(
			context,
			"(proc %.*s (",
			strf(node->value.proc_definition.name)
		);
		node_list_format_ctx(context, node->value.proc_definition.args, false);
		node_format_write(context, ") (");
		node_list_format_ctx(context, node->value.proc_definition.returns, false);
		node_format_write(context, ") ");
		node_format_ctx(context, node->value.proc_definition.body);
		node_format_write(context, ")");
		break;

	default:
		context->error = IO_Err_Other;
		break;
	}
}

isize node_format(IO_Writer writer, Node* node){
	Node_Format_Context context = {.writer = writer};
	node_format_ctx(&context, node);
	return context.error < 0 ? context.error : context.written;
}

Error parse(String source, AST* ast, Arena* arena){
	ensure(ast != NULL, "AST must not be null");
	ensure(arena != NULL, "AST arena must not be null");

	Parser parser = parser_make(source, arena);
	Node_List definitions = {0};
	while(parser_peek(&parser).type != Tk_EndOfFile){
		Token token = parser_peek(&parser);
		if(parser.error.typ != Err_None){
			break;
		}
		if(token.type != Tk_Proc){
			parser_unexpected(&parser, token, Tk_Proc);
			break;
		}

		Parser_Result result = parse_proc_definition(&parser);
		if(has_error(result)){
			break;
		}
		ensure(result.node != NULL && result.node->type == Node_ProcDefinition,
			"top-level node must be a procedure definition");
		node_list_push(&definitions, result.node);
	}

	parser.ast.root = definitions.first;
	*ast = parser.ast;
	return parser.error;
}
