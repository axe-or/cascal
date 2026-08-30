#include "base.h"
#include <stdio.h>

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

Scanner_Result scanner_next_token(Scanner* sc){
	i32 start = sc->current;
	rune r = scan_next(sc);
	TokenType type = Tk_Unknown;

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

