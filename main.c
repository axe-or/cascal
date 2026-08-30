#include "base.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

////~ Scanning

#define TOKEN_KEYWORDS \
	X(True, "true") \
	X(False, "false") \
	X(If, "if") \
	X(Else, "else") \
	X(For, "for") \
	X(Proc, "proc") \
	X(End, "end") \
	X(Break, "break") \
	X(Continue, "continue") \
	X(Return, "return") \
	X(Var, "var") \
	X(Const, "const") \
	X(Type, "type") \
	X(Record, "record") \
	X(Variant, "variant") \
	X(LogicAnd, "and") \
	X(LogicOr, "or") \
	X(LogicNot, "not") \

#define TOKEN_TYPES \
	X(Identifier, "Id") \
	X(Real, "Real") \
	X(Integer, "Int") \
	X(String, "String") \
	X(CurlyOpen, "{") \
	X(CurlyClose, "}") \
	X(SquareOpen, "[") \
	X(SquareClose, "]") \
	X(ParenOpen, "(") \
	X(ParenClose, ")") \
	X(Colon, ":") \
	X(Comma, ",") \
	X(Dot, ".") \
	X(Assign, "=") \
	X(Semicolon, ";") \
	X(Plus, "+") \
	X(Minus, "-") \
	X(Star, "*") \
	X(Slash, "/") \
	X(Modulo, "%") \
	X(And, "&") \
	X(Or, "|") \
	X(Tilde, "~") \
	X(ShiftLeft, "<<") \
	X(ShiftRight, ">>") \
	X(Eq, "==") \
	X(Neq, "!=") \
	X(Gt, ">") \
	X(GtEq, ">=") \
	X(Lt, "<") \
	X(LtEq, "<=") \
	TOKEN_KEYWORDS \
	X(EndOfFile, "<EOF>")

// Types of token
typedef enum {
	Tk_Unknown = 0,

#define X(name, sym) Tk_##name,
	TOKEN_TYPES
#undef X

	TokenType__COUNT,
} TokenType;

static String token_type_names[TokenType__COUNT] = {
	[Tk_Unknown] = strlit("<Unknown>"),

#define X(name, sym) [Tk_##name] = strlit(sym),
	TOKEN_TYPES
#undef X
};

enum Error_Type {
	Err_None = 0,

	// Scanner errors
	Err_UnexpectedChar,
	Err_UnknownChar,
	Err_InvalidBase,
	Err_InvalidNumber,
	Err_InvalidEscapeSequence,
	Err_InvalidStringChar,
	Err_UnclosedString,

	// Parser errors
	Err_UnexpectedToken,

	Error_Type__COUNT,
};

typedef struct {
	i32 offset;
	u8 typ;

	union {
		rune character;
		TokenType token_type;
	} expected;

	union {
		rune character;
		TokenType token_type;
	} got;
} Error;

static struct { String sym; TokenType type; } token_keywords[] = {
#define X(name, symbol) { .sym = strlit(symbol), .type = Tk_##name },
	TOKEN_KEYWORDS
#undef X
};

#define TOKEN_KEYWORD_COUNT (sizeof(token_keywords) / sizeof(token_keywords[0]))

String token_type_name(TokenType t){
	if(t < 0 || t >= TokenType__COUNT){
		return strlit("<INVALID TOKEN TYPE>");
	}
	return token_type_names[(int)t];
}

typedef struct {
	TokenType type;
	i32 start;
	i32 end;

	i64 value_int;
	f64 value_real;
} Token;

typedef struct {
	String source;
	i32 current;
} Scanner;

typedef struct {
	Token token;
	Error error;
} Scanner_Result;

rune scan_peek(Scanner const* sc, i32 delta){
	i32 pos = sc->current + delta;

	if(pos < 0 || pos >= sc->source.len){
		return '\0';
	}

	Rune_Decoded dec = rune_decode((u8 const*)(&sc->source.v[pos]), sc->source.len - pos);
	return dec.codepoint;
}

rune scan_next(Scanner* sc){
	if(sc->current >= sc->source.len){
		return '\0';
	}

	Rune_Decoded dec = rune_decode((u8 const*)(&sc->source.v[sc->current]), sc->source.len - sc->current);
	sc->current += dec.size;
	return dec.codepoint;
}

static bool scan_take_if(Scanner* sc, rune expected){
	if(scan_peek(sc, 0) != expected){
		return false;
	}

	scan_next(sc);
	return true;
}

static Scanner_Result scanner_result(TokenType type, i32 start, i32 end){
	return (Scanner_Result){
		.token = {
			.type = type,
			.start = start,
			.end = end,
		},
	};
}

static inline
rune escape_sequence(rune c){
	switch(c){
	case 't': return '\t';
	case 'r': return '\r';
	case 'n': return '\n';
	case '"': return '"';
	case '\'': return '\'';
	case '\\': return '\\';
	}
	return RUNE_ERROR;
}

static Scanner_Result scan_string(Scanner* sc, i32 start){
	Error error = {0};

	for(;;){
		i32 offset = sc->current;
		rune r = scan_next(sc);

		if(r == '\0'){
			Scanner_Result result = scanner_result(Tk_String, start, sc->current);
			result.error = (Error){
				.offset = offset,
				.typ = Err_UnclosedString,
			};
			return result;
		}

		if(r == '"'){
			Scanner_Result result = scanner_result(Tk_String, start, sc->current);
			result.error = error;
			return result;
		}

		if(r == '\\'){
			i32 escape_offset = sc->current;
			rune escaped = scan_next(sc);
			if(escaped == '\0'){
				Scanner_Result result = scanner_result(Tk_String, start, sc->current);
				result.error = (Error){
					.offset = escape_offset,
					.typ = Err_UnclosedString,
				};
				return result;
			}
			if(escape_sequence(escaped) == RUNE_ERROR && error.typ == Err_None){
				error = (Error){
					.offset = escape_offset,
					.typ = Err_InvalidEscapeSequence,
					.got.character = escaped,
				};
			}
			continue;
		}

		if((r == '\n' || r == '\r' || r == '\t') && error.typ == Err_None){
			error = (Error){
				.offset = offset,
				.typ = Err_InvalidStringChar,
				.got.character = r,
			};
		}
	}
}

static inline
i32 base_of(rune c){
	switch(c){
	case 'b': case 'B': return 2;
	case 'o': case 'O': return 8;
	case 'x': case 'X': return 16;
	}
	return -1;
}

static inline
i32 digit_of(rune c){
	if(c >= '0' && c <= '9'){
		return c - '0';
	}
	if(c >= 'a' && c <= 'f'){
		return c - 'a' + 10;
	}
	if(c >= 'A' && c <= 'F'){
		return c - 'A' + 10;
	}
	return -1;
}

static bool scan_digit_sequence(Scanner* sc, i32 base){
	bool has_digit = false;

	for(;;){
		rune r = scan_peek(sc, 0);
		if(r == '_'){
			scan_next(sc);
			continue;
		}

		i32 digit = digit_of(r);
		if(digit < 0 || digit >= base){
			break;
		}

		has_digit = true;
		scan_next(sc);
	}
	return has_digit;
}

static bool scan_exponent(Scanner* sc, rune lower, rune upper){
	rune r = scan_peek(sc, 0);
	if(r != lower && r != upper){
		return false;
	}

	Scanner parsed = *sc;
	scan_next(&parsed);
	if(scan_peek(&parsed, 0) == '+' || scan_peek(&parsed, 0) == '-'){
		scan_next(&parsed);
	}

	if(!scan_digit_sequence(&parsed, 10)){
		return false;
	}

	*sc = parsed;
	return true;
}

static bool scan_real_suffix(Scanner* sc, i32 base){
	Scanner parsed = *sc;
	bool has_fraction = false;

	if(scan_take_if(&parsed, '.')){
		if(scan_digit_sequence(&parsed, base)){
			has_fraction = true;
		} else {
			parsed = *sc;
		}
	}

	bool has_exponent = base == 16
		? scan_exponent(&parsed, 'p', 'P')
		: scan_exponent(&parsed, 'e', 'E');

	if(base == 16 ? !has_exponent : !has_fraction && !has_exponent){
		return false;
	}

	*sc = parsed;
	return true;
}

static Scanner_Result scan_real(Scanner const* sc, i32 start){
	i32 raw_len = sc->current - start;
	char normalized[raw_len + 1];
	i32 normalized_len = 0;

	for(i32 i = start; i < sc->current; i += 1){
		if(sc->source.v[i] != '_'){
			normalized[normalized_len] = sc->source.v[i];
			normalized_len += 1;
		}
	}
	normalized[normalized_len] = '\0';

	char* end = NULL;
	errno = 0;
	f64 value = strtod(normalized, &end);
	Scanner_Result result = scanner_result(Tk_Real, start, sc->current);
	if(end != normalized + normalized_len || errno == ERANGE){
		result.error = (Error){
			.offset = start + (i32)(end - normalized),
			.typ = Err_InvalidNumber,
		};
		return result;
	}

	result.token.value_real = value;
	return result;
}

static Scanner_Result scan_integer(Scanner* sc, i32 start, rune first){
	i32 base = 10;
	bool has_body = true;
	bool has_digit = true;
	i64 value = first - '0';
	Error error = {0};

	if(first == '0'){
		i32 prefixed_base = base_of(scan_peek(sc, 0));
		if(prefixed_base != -1){
			base = prefixed_base;
			has_body = false;
			has_digit = false;
			value = 0;
			scan_next(sc);
		}
	}

	for(;;){
		rune r = scan_peek(sc, 0);
		if(r == '_'){
			has_body = true;
			scan_next(sc);
			continue;
		}

		i32 digit = digit_of(r);
		if(digit < 0 || digit >= base){
			break;
		}

		has_body = true;
		has_digit = true;
		i32 digit_offset = sc->current;
		scan_next(sc);

		if(error.typ == Err_None){
			if(value > (INT64_MAX - digit) / base){
				error = (Error){
					.offset = digit_offset,
					.typ = Err_InvalidNumber,
					.got.character = r,
				};
			} else {
				value = value * base + digit;
			}
		}
	}

	if((base == 10 || base == 16) && has_digit){
		Scanner real = *sc;
		if(scan_real_suffix(&real, base)){
			*sc = real;
			return scan_real(sc, start); }
	}

	if(!has_body){
		error = (Error){
			.offset = sc->current,
			.typ = Err_InvalidNumber,
			.got.character = scan_peek(sc, 0),
		};
	}

	Scanner_Result result = scanner_result(Tk_Integer, start, sc->current);
	result.token.value_int = value;
	result.error = error;
	return result;
}

Scanner_Result scanner_next_token(Scanner* sc){
	i32 start = sc->current;
	rune r = scan_next(sc);
	TokenType type = Tk_Unknown;
	if(r >= '0' && r <= '9'){
		return scan_integer(sc, start, r);
	}
	if(r == '"'){
		return scan_string(sc, start);
	}

	switch(r){
	case '\0':
		return scanner_result(Tk_EndOfFile, start, start);

	case '{': type = Tk_CurlyOpen; break;
	case '}': type = Tk_CurlyClose; break;
	case '[': type = Tk_SquareOpen; break;
	case ']': type = Tk_SquareClose; break;
	case '(': type = Tk_ParenOpen; break;
	case ')': type = Tk_ParenClose; break;
	case ':': type = Tk_Colon; break;
	case ',': type = Tk_Comma; break;
	case '.': type = Tk_Dot; break;
	case ';': type = Tk_Semicolon; break;
	case '+': type = Tk_Plus; break;
	case '-': type = Tk_Minus; break;
	case '*': type = Tk_Star; break;
	case '/': type = Tk_Slash; break;
	case '%': type = Tk_Modulo; break;
	case '&': type = Tk_And; break;
	case '|': type = Tk_Or; break;
	case '~': type = Tk_Tilde; break;

	case '=': type = scan_take_if(sc, '=') ? Tk_Eq : Tk_Assign; break;

	case '!': type = scan_take_if(sc, '=') ? Tk_Neq : Tk_Unknown; break;

	case '>':
		type = scan_take_if(sc, '=')
			? Tk_GtEq
			: scan_take_if(sc, '>')
			? Tk_ShiftRight
			: Tk_Gt;
		break;

	case '<':
		type = scan_take_if(sc, '=')
			? Tk_LtEq
			: scan_take_if(sc, '<')
			? Tk_ShiftLeft
			: Tk_Lt;
		break;
	}

	Scanner_Result result = scanner_result(type, start, sc->current);

	if(type == Tk_Unknown){
		result.error = (Error){
			.offset = start,
			.typ = Err_UnknownChar,
			.got.character = r,
		};
	}
	return result;
}

Scanner_Result scan_peek_token(Scanner const* sc){
	Scanner copy = *sc;
	return scanner_next_token(&copy);
}

int main(){
}
