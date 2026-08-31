#include "base.h"
#include "lang.h"

#include <errno.h>
#include <stdlib.h>

static String token_type_names[Token_Type__COUNT] = {
	[Tk_Unknown] = strlit("<Unknown>"),

#define X(name, sym) [Tk_##name] = strlit(sym),
	TOKEN_TYPES
#undef X
};

static struct { String sym; Token_Type type; } token_keywords[] = {
#define X(name, symbol) { .sym = strlit(symbol), .type = Tk_##name },
	TOKEN_KEYWORDS
#undef X
};

#define TOKEN_KEYWORD_COUNT (sizeof(token_keywords) / sizeof(token_keywords[0]))

String token_type_name(Token_Type t){
	if(t < 0 || t >= Token_Type__COUNT){
		return strlit("<INVALID TOKEN TYPE>");
	}
	return token_type_names[(int)t];
}

static inline
rune scan_peek(Scanner const* sc, i32 delta){
	i32 pos = sc->current + delta;

	if(pos < 0 || pos >= sc->source.len){
		return '\0';
	}

	Rune_Decoded dec = rune_decode((u8 const*)(&sc->source.v[pos]), sc->source.len - pos);
	return dec.codepoint;
}

static inline
rune scan_next(Scanner* sc){
	if(sc->current >= sc->source.len){
		return '\0';
	}

	Rune_Decoded dec = rune_decode((u8 const*)(&sc->source.v[sc->current]), sc->source.len - sc->current);
	sc->current += dec.size;
	return dec.codepoint;
}

// Advance scanner iff the current rune == `expected`
static inline
bool scan_take_if(Scanner* sc, rune expected){
	if(scan_peek(sc, 0) != expected){
		return false;
	}

	scan_next(sc);
	return true;
}

static inline
Scanner_Result scanner_result(Token_Type type, i32 start, i32 end){
	return (Scanner_Result){
		.token = {
			.type = type,
			.start = start,
			.end = end,
		},
	};
}

static inline
Scanner_Result scan_string(Scanner* sc, i32 start){
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
Error scan_comment(Scanner* sc){
	rune kind = scan_next(sc);
	if(kind == '/'){
		while(sc->current < sc->source.len){
			rune r = scan_next(sc);
			if(r == '\n'){
				break;
			}
			if(r == '\r'){
				scan_take_if(sc, '\n');
				break;
			}
		}
		return (Error){0};
	}

	i32 depth = 1;
	while(sc->current < sc->source.len){
		rune r = scan_next(sc);
		if(r == '/' && scan_take_if(sc, '*')){
			depth += 1;
		} else if(r == '*' && scan_take_if(sc, '/')){
			depth -= 1;
			if(depth == 0){
				return (Error){0};
			}
		}
	}

	return (Error){
		.offset = sc->current,
		.typ = Err_UnclosedComment,
	};
}

static inline
bool is_identifier_start(rune r){
	return (r >= 'a' && r <= 'z')
		|| (r >= 'A' && r <= 'Z')
		|| r == '_';
}

static inline
bool is_identifier_continue(rune r){
	return is_identifier_start(r) || (r >= '0' && r <= '9');
}

static inline
Scanner_Result scan_identifier(Scanner* sc, i32 start){
	while(is_identifier_continue(scan_peek(sc, 0))){
		scan_next(sc);
	}

	String identifier = {
		.v = &sc->source.v[start],
		.len = sc->current - start,
	};
	Token_Type type = Tk_Identifier;

	for(usize i = 0; i < TOKEN_KEYWORD_COUNT; i += 1){
		if(str_equal(identifier, token_keywords[i].sym)){
			type = token_keywords[i].type;
			break;
		}
	}

	return scanner_result(type, start, sc->current);
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

static inline
bool scan_digit_sequence(Scanner* sc, i32 base){
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

static inline
bool scan_exponent(Scanner* sc, rune lower, rune upper){
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

static inline
bool scan_real_suffix(Scanner* sc, i32 base){
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

static inline
Scanner_Result scan_real(Scanner const* sc, i32 start){
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

static inline
Scanner_Result scan_integer(Scanner* sc, i32 start, rune first){
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

Scanner_Result scan_next_token(Scanner* sc){
	i32 start;
	rune r;

	for(;;){
		start = sc->current;
		r = scan_next(sc);
		if(r != '/' || (scan_peek(sc, 0) != '/' && scan_peek(sc, 0) != '*')){
			break;
		}

		Error error = scan_comment(sc);
		if(error.typ != Err_None){
			Scanner_Result result = scanner_result(Tk_Unknown, start, sc->current);
			result.error = error;
			return result;
		}
	}

	Token_Type type = Tk_Unknown;
	if(r >= '0' && r <= '9'){
		return scan_integer(sc, start, r);
	}
	if(r == '"'){
		return scan_string(sc, start);
	}
	if(is_identifier_start(r)){
		return scan_identifier(sc, start);
	}

	switch(r){
	case '\0': return scanner_result(Tk_EndOfFile, start, start);

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
	case '^': type = Tk_Caret; break;

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
	return scan_next_token(&copy);
}
