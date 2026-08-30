#include "cgen.h"

#include <stdint.h>
#include <stdarg.h>

#include <stdio.h>

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

////~ Utilities
extern _Noreturn void abort();
extern void* memmove(void*, void const*, size_t);
extern void* memset(void*, int, size_t);
extern double strtod(char const*, char**);
extern int puts(char const*);

static inline
void mem_copy(void* dst, void const* src, size_t n){
	memmove(dst, src, n);
}

static inline
void mem_zero(void* ptr, size_t n){
	memset(ptr, 0, n);
}

static inline
char* stb_printf_writer_adapter(char const* buf, void* user, int len){
	Writer w = *((Writer*)user);
	write_to(w, buf, len);
	return NULL;
}

static
int write_vfmt(Writer w, char const* format, va_list args){
	char buf[STB_SPRINTF_MIN + 64] = {0};
	return stbsp_vsprintfcb(stb_printf_writer_adapter, &w, &buf[0], format, args);
}

static
void write_fmt(Writer w, char const* format, ...){
	va_list args;
	va_start(args, format);
	write_vfmt(w, format, args);
	va_end(args);
}

attribute_format(3, 4)
_Noreturn
void panic_ex(char const* file, int line, char const* fmt, ...){
	static uint8_t panic_msg_buf[256] = {0};
	ByteBuffer bb = {.data = &panic_msg_buf[0], .len = 0, .cap = sizeof(panic_msg_buf)};
	Writer w = byte_buffer_writer(&bb);

	write_fmt(w,"(%s:%d) panic: ", file, line);

	va_list args;
	va_start(args, fmt);
	write_vfmt(w, fmt, args);
	va_end(args);

	bb.data[bb.cap-1] = '\0';

	puts((char*)bb.data);
	abort();
}

void ensure_ex(bool predicate, char const* msg, char const* file, int line){
	if(!predicate){
		printf("(%s:%d) assertion failed: %s", file, line, msg);
		fflush(stdout);
		abort();
	}
}

bool str_equal(Str a, Str b){
	if(a.len != b.len){ return false; }

	for(int i = 0; i < a.len; i += 1){
		if(a.v[i] != b.v[i]){
			return false;
		}
	}
	return true;
}

int byte_buffer_write(void* impl, char const* buf, int buflen){
	ByteBuffer* bb = impl;
	int n = max(0, min(bb->cap - bb->len, buflen));
	memcpy(bb->data, buf, n);
	bb->len += n;
	return n;
}

////~ Scanning
static char const* token_type_name[] = {
	[Tk_Unknown] = "UNKNOWN",

	[Tk_Identifier] = "Id",
	[Tk_Tag] = "Tag",
	[Tk_Number] = "Number",
	[Tk_String] = "String",
	[Tk_CurlyOpen] = "{",
	[Tk_CurlyClose] = "}",
	[Tk_SquareOpen] = "[",
	[Tk_SquareClose] = "]",
	[Tk_ParenOpen] = "(",
	[Tk_ParenClose] = ")",
	[Tk_Colon] = ":",
	[Tk_Comma] = ",",
	[Tk_EndOfFile] = "EOF",
};

static inline
char scan_peek(Scanner const* sc, int delta){
	int pos = sc->current + delta;
	if(pos < 0 || pos >= sc->len){
		return 0;
	}

	return sc->source[pos];
}

static inline
bool scan_consume_if(Scanner* sc, char desired){
	if(scan_peek(sc, 0) != desired){
		return false;
	}
	sc->current += 1;
	return true;
}

static inline
char scan_consume_if_fn(Scanner* sc, bool (*fn)(char)){
	if(!fn(scan_peek(sc, 0))){
		return false;
	}
	sc->current += 1;
	return true;
}

static inline
char scan_consume(Scanner* sc){
	if(sc->current >= sc->len){
		return 0;
	}

	char c = sc->source[sc->current];
	sc->current += 1;
	return c;
}

static inline
bool is_upper(char c){
	return (c >= 'A') && (c <= 'Z');
}

static inline
bool is_lower(char c){
	return (c >= 'a') && (c <= 'z');
}

static inline
bool is_alpha(char c){
	return is_lower(c) || is_upper(c);
}

static inline
bool is_dec_digit(char c){
	return (c >= '0') && (c <= '9');
}

static inline
bool is_hex_digit(char c){
	return is_dec_digit(c)
		|| (c >= 'a' && c <= 'f')
		|| (c >= 'A' && c <= 'F');
}

static inline
int hex_digit_value(char c){
	if(is_dec_digit(c)){ return c - '0'; }
	if(c >= 'a' && c <= 'f'){ return c - 'a' + 10; }
	return c - 'A' + 10;
}

static inline
int32_t escape_sequence(char c){
    switch(c){
    case 'r': return '\r';
    case 'n': return '\n';
    case 't': return '\t';
    case '0': return '\0';
    case '\'': return '\'';
    case '\\': return '\\';
    case '"': return '"';
    }

    return -1; /* Invalid */
}

static inline
int scan_number_digits(Scanner* sc, char* buf, int buflen){
	ensure(is_dec_digit(scan_peek(sc, 0)), "expected digit");
	int consumed = 0;

	do {
		ensure(consumed < buflen - 1, "number is too long");
		buf[consumed] = scan_consume(sc);
		consumed += 1;

		if(scan_consume_if(sc, '_')){
			while(scan_consume_if(sc, '_')){}
			ensure(is_dec_digit(scan_peek(sc, 0)), "invalid digit separator");
		}
	} while(is_dec_digit(scan_peek(sc, 0)));

	return consumed;
}

Token scan_number(Scanner* sc){
	ensure(is_dec_digit(scan_peek(sc, 0))
		|| (scan_peek(sc, 0) == '-' && is_dec_digit(scan_peek(sc, 1))), "not on a number");

	Token res = {0};
	char digits[128] = {0};
	int digits_buflen = (int)sizeof(digits);
	int digits_len = 0;
	int start = sc->current;

	bool negative = scan_consume_if(sc, '-');
	if(negative){
		digits[digits_len] = '-';
		digits_len += 1;
	}

	if(scan_peek(sc, 0) == '0'
		&& (scan_peek(sc, 1) == 'x' || scan_peek(sc, 1) == 'X')){
		scan_consume_if(sc, '0');
		bool consumed_prefix = scan_consume_if(sc, 'x') || scan_consume_if(sc, 'X');
		ensure(consumed_prefix, "invalid hexadecimal prefix");
		ensure(is_hex_digit(scan_peek(sc, 0)), "expected hexadecimal digit");

		double value = 0;
		do {
			value = value * 16 + hex_digit_value(scan_consume(sc));
			if(scan_consume_if(sc, '_')){
				while(scan_consume_if(sc, '_')){}
				ensure(is_hex_digit(scan_peek(sc, 0)), "invalid digit separator");
			}
		} while(is_hex_digit(scan_peek(sc, 0)));

		res.number_value = negative ? -value : value;
		res.lexeme = (Str){ .v = &sc->source[start], .len = sc->current - start };
		res.type = Tk_Number;
		return res;
	}

	digits_len += scan_number_digits(sc, digits + digits_len, digits_buflen - digits_len);

	if(scan_consume_if(sc, '.')){
		ensure(digits_len < digits_buflen - 1, "number is too long");
		digits[digits_len] = '.';
		digits_len += 1;
		digits_len += scan_number_digits(sc, digits + digits_len, digits_buflen - digits_len);
	}

	if(scan_consume_if(sc, 'e') || scan_consume_if(sc, 'E')){
		ensure(digits_len < digits_buflen - 1, "number is too long");
		digits[digits_len] = 'e';
		digits_len += 1;

		if(scan_consume_if(sc, '+')){
			ensure(digits_len < digits_buflen - 1, "number is too long");
			digits[digits_len] = '+';
			digits_len += 1;
		}else if(scan_consume_if(sc, '-')){
			ensure(digits_len < digits_buflen - 1, "number is too long");
			digits[digits_len] = '-';
			digits_len += 1;
		}

		digits_len += scan_number_digits(sc, digits + digits_len, digits_buflen - digits_len);
	}

	char* end = NULL;
	res.number_value = strtod(digits, &end);
	ensure(end == &digits[digits_len], "failed to parse number");
	res.lexeme = (Str){ .v = &sc->source[start], .len = sc->current - start };
	res.type = Tk_Number;

	return res;
}

Token scan_string(Scanner* sc){
	int start = sc->current;
	ensure(scan_consume_if(sc, '"'), "not on a string");
	StringBuilder value = sb_make(16, sc->arena);
	Token res = {0};

	while(!scan_consume_if(sc, '"')){
		char c = scan_consume(sc);
		ensure(c != 0, "unterminated string");
		ensure(c != '\n' && c != '\r' && c != '\t', "invalid character in string");

		if(c == '\\'){
			int32_t escaped = escape_sequence(scan_consume(sc));
			ensure(escaped >= 0, "invalid escape sequence");
			c = (char)escaped;
		}

		sb_write(&value, &c, 1);
	}

	res.lexeme = (Str){ .v = &sc->source[start], .len = sc->current - start };
	res.type = Tk_String;
	res.string_value = sb_build(&value);
	return res;
}

static inline
bool is_identifier_part(char c){
	return c == '_' || is_alpha(c) || is_dec_digit(c);
}

Token scan_identifier(Scanner* sc){
	char c = scan_peek(sc, 0);
	ensure(c == '_' || is_alpha(c), "not on an identifier");
	int start = sc->current;

	while(scan_consume_if_fn(sc, is_identifier_part)){}
	
	Str lexeme = { .v = &sc->source[start], .len = sc->current - start };
	return (Token){
		.lexeme = lexeme,
		.string_value = lexeme,
		.type = Tk_Identifier,
	};
}

Token scan_tag(Scanner* sc){
	int start = sc->current;
	ensure(scan_consume_if(sc, '@'), "not on a tag");

	Token res = scan_identifier(sc);
	res.lexeme = (Str){ .v = &sc->source[start], .len = sc->current - start };
	res.type = Tk_Tag;
	return res;
}

static inline
bool is_whitespace(char c){
	return (c == ' ')
		|| (c == '\t')
		|| (c == '\r')
		|| (c == '\n');
}

static inline
void consume_until_newline(Scanner* sc){
	for(; sc->current < sc->len; sc->current += 1){
		if(sc->source[sc->current] == '\n'){
			break;
		}
	}
}

Token scan_next_token(Scanner* sc){
	// Skip whitespace
	while(sc->current < sc->len){
		if(is_whitespace(sc->source[sc->current])){
			sc->current += 1;
		} else {
			break;
		}
	}

	char c = scan_peek(sc, 0);

	if(!c){
		return (Token){.type = Tk_EndOfFile};
	}

	if(c == '/' && scan_peek(sc, 1) == '/'){
		sc->current += 2; 
		consume_until_newline(sc);
		return scan_next_token(sc);
	}

	if(c == '"'){
		return scan_string(sc);
	}

	if(c == '@'){
		return scan_tag(sc);
	}

	if(c == '_' || is_alpha(c)){
		return scan_identifier(sc);
	}

	if(is_dec_digit(c) || (c == '-' && is_dec_digit(scan_peek(sc, 1)))){
		return scan_number(sc);
	}

	Token res = {0};
	switch (c) {
	case '{': res.type = Tk_CurlyOpen; break;
	case '}': res.type = Tk_CurlyClose; break;
	case '[': res.type = Tk_SquareOpen; break;
	case ']': res.type = Tk_SquareClose; break;
	case '(': res.type = Tk_ParenOpen; break;
	case ')': res.type = Tk_ParenClose; break;
	case ':': res.type = Tk_Colon; break;
	case ',': res.type = Tk_Comma; break;
	}

	if(res.type == Tk_Unknown){
		printf("unknown tk: '%c'\n", c);
	}
	ensure(res.type != Tk_Unknown, "unknown token");
	sc->current += 1;

	return res;
}

Token scan_peek_token(Scanner const* sc){
	Scanner sc_copy = *sc;
	return scan_next_token(&sc_copy);
}

Token scan_peek_token2(Scanner const* sc){
	Scanner sc_copy = *sc;
	scan_next_token(&sc_copy);
	return scan_next_token(&sc_copy);
}

////~ Parsing

void node_append_sibling(Node* target, Node* sibling){
	ensure(target, "cannot append sibling to null node");
	ensure(sibling, "cannot append null");

	Node* next = target->next;
	target->next = sibling;
	sibling->parent = target->parent;
	sibling->prev = target;
	sibling->next = next;

	if(next){
		next->prev = sibling;
	}
	else if(target->parent){
		target->parent->last = sibling;
	}
}

void node_append_child(Node* target, Node* child){
	ensure(target, "cannot append child to null node");
	ensure(child, "cannot append null");

	if(target->last){
		node_append_sibling(target->last, child);
	}
	else {
		target->first = child;
		target->last = child;
		child->parent = target;
		child->prev = NULL;
		child->next = NULL;
	}
}

Node* node_make(Arena* a, NodeType type){
	Node* node = arena_make(a, Node, 1);
	ensure(node != NULL, "exhausted node storage");
	node->type = type;
	return node;
}

Token parser_advance(Parser* parser){
	return scan_next_token(&parser->scan);
}

bool parser_advance_if(Parser* parser, TokenType t){
	Token tok = scan_peek_token(&parser->scan);
	if(tok.type != t){
		return false;
	}
	scan_next_token(&parser->scan);
	return true;
}

Token parser_peek(Parser* parser){
	return scan_peek_token(&parser->scan);
}

Token parser_peek2(Parser* parser){
	return scan_peek_token2(&parser->scan);
}

static inline
bool parser_done(Parser const* parser){
	return parser->scan.current >= parser->scan.len;
}

Token parser_expect(Parser* parser, TokenType expect){
	Token t = parser_advance(parser);
	if(t.type != expect){
		printf("expected %s found %s\n", token_type_name[expect], token_type_name[t.type]);
		panic("unexpected token");
	}
	return t;
}

static inline
Node* parse_value(Parser* parser);

static inline
Node* parse_value_list(Parser* parser, Node* target, TokenType close_delim){
	while(!parser_done(parser)){
		Token tok = parser_peek(parser);

		if(tok.type == Tk_EndOfFile){
			break;
		}
		else if(tok.type == close_delim){
			break;
		}
		else {
			Node* value = parse_value(parser);
			node_append_child(target, value);

			if(parser_advance_if(parser, Tk_Comma)){
				continue;
			} else if(parser_peek(parser).type == close_delim) {
				break;
			} else {
				Str s = parser_peek(parser).lexeme;
				panic("expected `,` or closing delimiter found `%.*s`", strf(s));
			}
		}
	}
	parser_expect(parser, close_delim);

	return target;
}

static inline
Node* parse_array(Parser* parser){
	Node* res = node_make(parser->arena, Node_Array);
	parser_expect(parser, Tk_SquareOpen);
	return parse_value_list(parser, res, Tk_SquareClose);
}

static inline
Node* parse_args(Parser* parser, Node* target);

static inline
Node* parse_tag(Parser* parser){
	Node* tag = node_make(parser->arena, Node_Tag);

	Token tok = parser_expect(parser, Tk_Tag);

	Str name = (Str){ .v = &tok.lexeme.v[1], .len=tok.lexeme.len - 1};
	tag->string_value = name;

	if(parser_peek(parser).type == Tk_ParenOpen){
		parse_args(parser, tag);
	}

	return tag;
}

static inline
Node* parse_key_value(Parser* parser);

static inline
Node* parse_tag_chain(Parser* parser){
	ensure(parser_peek(parser).type == Tk_Tag, "not on a tag chain");
	Node* first = parse_tag(parser);
	Node* cur = first;

	while(parser_peek(parser).type == Tk_Tag){
		Node* next = parse_tag(parser);
		node_append_sibling(cur, next);
		cur = next;
	}

	return first;
}

static inline
Node* parse_key_value(Parser* parser){
	Node* kv = node_make(parser->arena, Node_Key);

	Token key = parser_advance(parser);
	ensure(key.type == Tk_Identifier || key.type == Tk_String, "not on key");

	parser_expect(parser, Tk_Colon);
	Node* val = parse_value(parser);

	kv->string_value = key.type == Tk_Identifier ? key.lexeme : key.string_value;
	node_append_child(kv, val);

	return kv;
}

static inline
Node* parse_args(Parser* parser, Node* target){
	parser_expect(parser, Tk_ParenOpen);

	while(!parser_done(parser)){
		Token tok = parser_peek(parser);
		if(tok.type == Tk_EndOfFile){
			break;
		}
		if(tok.type == Tk_ParenClose){
			break;
		}

		bool is_key = parser_peek2(parser).type == Tk_Colon;
		if(is_key && tok.type == Tk_Identifier){
			Node* kv = parse_key_value(parser);
			node_append_child(target, kv);
		}
		else {
			Node* v = parse_value(parser);
			node_append_child(target, v);
		}

		if(parser_advance_if(parser, Tk_Comma)){
			continue;
		} else if(parser_peek(parser).type == Tk_ParenClose){
			break;
		} else {
			panic("(%d): expected `,` or `)`, found: %s\n", parser->scan.current, token_type_name[tok.type]);
		}
	}

	parser_expect(parser, Tk_ParenClose);
	return target;
}

static inline
Node* parse_object(Parser* parser){
	parser_expect(parser, Tk_CurlyOpen);
	Node* res = node_make(parser->arena, Node_Map);

	while(!parser_done(parser)){
		Token tok = parser_peek(parser);
		if(tok.type == Tk_EndOfFile){
			break;
		}
		if(tok.type == Tk_CurlyClose){
			break;
		}

		if(tok.type == Tk_Tag || tok.type == Tk_Identifier || tok.type == Tk_String){
			Node* tag = NULL;
			if(tok.type == Tk_Tag){
				tag = parse_tag_chain(parser);
			}
			
			Node* kv = parse_key_value(parser);
			for(Node* cur = tag; cur != NULL; cur = cur->next){
				cur->parent = kv;
			}
			kv->tag = tag;
			node_append_child(res, kv);

			if(parser_advance_if(parser, Tk_Comma)){
				continue;
			} else if(parser_peek(parser).type == Tk_CurlyClose){
				break;
			} else {
				printf("(%d): expected `,` or `}`, found: %s\n", parser->scan.current, token_type_name[tok.type]);
				panic("unexpected");
			}
		}
		else {
			printf("(%d): expected identifier or tag, found: %s\n", parser->scan.current, token_type_name[tok.type]);
			panic("expected identifier or tag");
		}
	}

	parser_expect(parser, Tk_CurlyClose);
	return res;
}

static inline
Node* parse_value(Parser* parser){
	Token tok = parser_peek(parser);

	if(tok.type == Tk_CurlyOpen){
		return parse_object(parser);
	}
	else if(tok.type == Tk_SquareOpen){
		return parse_array(parser);
	}
	else if(tok.type == Tk_Number){
		parser_advance(parser);
		Node* node = node_make(parser->arena, Node_Number);
		node->number_value = tok.number_value;
		return node;
	}
	else if(tok.type == Tk_Identifier || tok.type == Tk_String){
		parser_advance(parser);
		Node* node = node_make(parser->arena, tok.type == Tk_Identifier ? Node_Identifier : Node_String);
		node->string_value = tok.type == Tk_Identifier ? tok.lexeme : tok.string_value;
		return node;
	}

	printf("unexpected token: %d\n", tok.type);
	panic("unexpected token");
}

Node* parse(Str source, Arena* a){
	Parser p = {
		.scan = {
			.source = source.v,
			.len = source.len,
			.current = 0,
			.arena = a,
		},
		.arena = a,
	};

	return parse_value(&p);
}

static void write_str(Writer w, Str str){
	ensure(write_to(w, str.v, str.len) == str.len, "failed to write AST");
}

static void write_char(Writer w, char c){
	write_str(w, (Str){ .v = &c, .len = 1 });
}

static void write_quoted_str(Writer w, Str str){
	write_str(w, str_lit("\""));
	int i = 0;
	while(i < str.len){
		char c = str.v[i];
		switch(c){
		case 0:    write_str(w, str_lit("\\0")); break;
		case '\n': write_str(w, str_lit("\\n")); break;
		case '\r': write_str(w, str_lit("\\r")); break;
		case '\t': write_str(w, str_lit("\\t")); break;
		case '"':  write_str(w, str_lit("\\\"")); break;
		case '\\': write_str(w, str_lit("\\\\")); break;
		default: write_char(w, c); break;
		}
		i += 1;
	}
	write_str(w, str_lit("\""));
}

void write_ast(Writer w, Node const* node){
	ensure(w.fn, "AST write function is null");
	ensure(node, "cannot print null AST node");

	switch(node->type){
	case Node_Number: {
		char buf[64] = {0};
		int len = snprintf(buf, sizeof(buf), "%g", node->number_value);
		ensure(len >= 0 && len < (int)sizeof(buf), "failed to format number");
		write_str(w, (Str){ .v = buf, .len = len });
		break;
	}

	case Node_String:
		write_quoted_str(w, node->string_value);
		break;

	case Node_Identifier:
		write_str(w, node->string_value);
		break;

	case Node_Tag:
		write_str(w, str_lit("(tag "));
		write_str(w, node->string_value);

		for(Node* cur = node->first; cur != NULL; cur = cur->next){
			write_str(w, str_lit(" "));
			write_ast(w, cur);
		}

		write_str(w, str_lit(")"));
		break;

	case Node_Key: {
		write_str(w, str_lit("(:"));
		write_str(w, node->string_value);
		Node const* value = node->first;
		while(value){
			write_str(w, str_lit(" "));
			write_ast(w, value);
			value = value->next;
		}
		write_str(w, str_lit(")"));
		break;
	}

	case Node_Array: {
		write_str(w, str_lit("(array"));
		Node const* value = node->first;
		while(value){
			write_str(w, str_lit(" "));
			write_ast(w, value);
			value = value->next;
		}
		write_str(w, str_lit(")"));
		break;
	}

	case Node_Map: {
		write_str(w, str_lit("(map"));
		Node const* child = node->first;
		while(child){
			ensure(child->type == Node_Tag || child->type == Node_Key,
				"map child is not a tag or key");
			Node const* applied = child;
			while(applied->type == Node_Tag){
				ensure(applied->last, "tag does not apply to a node");
				applied = applied->last;
			}
			ensure(applied->type == Node_Key, "map tag does not apply to a key");
			write_str(w, str_lit(" "));
			write_ast(w, child);
			child = child->next;
		}
		write_str(w, str_lit(")"));
		break;
	}

	default:
		panic("cannot print AST node type");
	}
}

static int print_write(void* userdata, char const* buf, int buflen){
	(void)userdata;
	int written = 0;
	while(written < buflen){
		if(printf("%c", (unsigned char)buf[written]) < 0){
			return -1;
		}
		written += 1;
	}
	return written;
}

void print_ast(Node const* node){
	write_ast((Writer){ .fn = print_write }, node);
}

////~ Tree exploration

Node* map_get(Node* map, Str key){
	ensure(map, "cannot get key from null node");
	ensure(map->type == Node_Map, "node is not a map");

	Node* child = map->first;
	while(child){
		Node* key_node = child;
		while(key_node->type == Node_Tag){
			ensure(key_node->last, "tag does not apply to a node");
			key_node = key_node->last;
		}
		ensure(key_node->type == Node_Key, "map child does not resolve to a key");

		if(str_equal(key_node->string_value, key)){
			return key_node;
		}
		child = child->next;
	}
	return NULL;
}

Node* array_get(Node* arr, int idx){
	ensure(arr, "cannot get element from null node");
	ensure(arr->type == Node_Array, "node is not an array");
	if(idx < 0){ return NULL; }

	Node* value = arr->first;
	int current = 0;
	while(value && current < idx){
		value = value->next;
		current += 1;
	}
	return value;
}

Node* find_tag(Node* key, Str tag_name){
	ensure(key->type == Node_Key, "not a key");

	for(Node* tag = key->tag; tag != NULL; tag = tag->next){
		if(str_equal(tag->string_value, tag_name)){
			return tag;
		}
	}

	return NULL;
}

Node* arg_get_unnamed(Node* tag, int idx){
	ensure(tag->type == Node_Tag, "not a tag");

	int cur_idx = 0;

	for(Node* cur = tag->first; cur != NULL; cur = cur->next){
		if(cur->type != Node_Key){
			if(cur_idx == idx){
				return cur;
			} else {
				cur_idx += 1;
			}
		}
	}

	return NULL;
}

Node* arg_get_named(Node* tag, Str name){
	ensure(tag->type == Node_Tag, "not a tag");
	for(Node* cur = tag->first; cur != NULL; cur = cur->next){
		if(cur->type == Node_Key && str_equal(cur->string_value, name)){
			return cur;
		}
	}

	return NULL;
}

void tag_foreach(Node* root, Str tag_name, void* userdata, void (*f) (Node* node, void* userdata)){
	for(Node* node = root->first; node != NULL; node = node->next){
		if(find_tag(node, tag_name) != NULL){
			f(node, userdata);
		}
	}
}

////~ Non essential utilities

Str str_clone(Str s, Arena* arena){
	StringBuilder sb = sb_make(s.len + 1, arena);
	sb_write(&sb, s.v, s.len);
	return sb_build(&sb);
}

Str str_concat(Str a, Str b, Arena* arena){
	StringBuilder sb = sb_make(a.len + b.len + 1, arena);
	sb_write(&sb, a.v, a.len);
	sb_write(&sb, b.v, b.len);
	return sb_build(&sb);
}

static inline
Str str_word_case(Str s, Arena* a, bool capitalize_words){
	if(s.len == 0 || s.v == NULL){ return (Str){0}; }

	StringBuilder sb = sb_make(max(16, s.len + 1), a);
	bool word_start = true;
	int i = 0;
	while(i < s.len){
		char c = s.v[i];
		if(is_upper(c)){
			char prev = i > 0 ? s.v[i - 1] : 0;
			char next = i + 1 < s.len ? s.v[i + 1] : 0;

			bool starts_word = (i > 0)
				&& (prev != '_')
				&& (is_lower(prev)
					|| is_dec_digit(prev)
					|| (is_upper(prev) && is_lower(next)));

			if(starts_word){
				sb_write_char(&sb, '_');
				word_start = true;
			}
		}

		if(c == '_'){
			word_start = true;
		}
		else if(is_alpha(c)){
			if(capitalize_words && word_start){
				if(is_lower(c)){
					c = (char)(c - 'a' + 'A');
				}
			}
			else if(is_upper(c)){
				c = (char)(c - 'A' + 'a');
			}
			word_start = false;
		}
		else {
			word_start = false;
		}

		sb_write_char(&sb, c);
		i += 1;
	}

	return sb_build(&sb);
}

Str str_snake_case(Str s, Arena* a){
	return str_word_case(s, a, false);
}

Str str_ada_case(Str s, Arena* a){
	return str_word_case(s, a, true);
}

Str str_format(Arena* a, char const* fmt, ...){
	StringBuilder sb = sb_make(16, a);

	va_list args;
	va_start(args, fmt);
	sb_write_vfmt(&sb, fmt, args);
	va_end(args);

	return sb_build(&sb);
}

