#include "base.h"
#include <stdio.h>

enum Error_Type {
	Err_None = 0,

	Err_UnexpectedChar,

	Error_Type__COUNT,
};

typedef struct {
	u8 typ;

	union {
		rune character;
	} expected;
} Error;

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
} Token;

typedef struct {} Scanner;

int main(){
	for(int i = 0; i < TokenType__COUNT; i ++){
		printf("%.*s = %d\n", strf(token_type_name((TokenType)i)), i);
	}

	for(int i = 0; i < TOKEN_KEYWORD_COUNT; i ++){
		printf("* %.*s\n", strf(token_keywords[i].sym));
	}
}
