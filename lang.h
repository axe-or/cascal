#pragma once

#include "base.h"

////~ Scanning
#define TOKEN_KEYWORDS \
	X(True, "true") \
	X(False, "false") \
	X(If, "if") \
	X(Else, "else") \
	X(For, "for") \
	X(Proc, "proc") \
	X(Break, "break") \
	X(Continue, "continue") \
	X(Return, "return") \
	X(Var, "var") \
	X(Const, "const") \
	X(Type, "type") \
	X(Record, "struct") \
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
	X(Caret, "^") \
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

	Token_Type__COUNT,
} Token_Type;

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
	Err_UnclosedComment,

	// Parser errors
	Err_UnexpectedToken,

	Error_Type__COUNT,
};

typedef struct {
	i32 offset;
	u8 typ;

	union {
		rune character;
		Token_Type token_type;
	} expected;

	union {
		rune character;
		Token_Type token_type;
	} got;
} Error;

// Token Type to string
String token_type_name(Token_Type t);

typedef struct {
	Token_Type type;
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

// Scan the current token and advance lexer
Scanner_Result scan_next_token(Scanner* sc);

// Scan the current token but do not advance
Scanner_Result scan_peek_token(Scanner const* sc);
