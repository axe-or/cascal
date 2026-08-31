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

////~ Parsing

#define NODE_AST_POOL_BITS 5ull

#define NODE_AST_POOL_OFFSET_BITS (32ull - NODE_AST_POOL_BITS)

#define NODE_POOL_SIZE 1024ull

#define AST_POOL_COUNT (1ull << NODE_AST_POOL_BITS)

_Static_assert(NODE_POOL_SIZE < (1ull << NODE_AST_POOL_OFFSET_BITS), "node pool size is too big");

_Static_assert((NODE_POOL_SIZE & (NODE_POOL_SIZE - 1)) == 0, "node pool size must be a power of 2");

typedef struct {
    Scanner scanner;
} Parser;

typedef struct {
    u32 v;
} Node_ID;

typedef struct {
    Node_ID left;
    Node_ID right;
    Token_Type op;
} Binary;

typedef struct {
    Node_ID operand;
    Token_Type op;
} Unary;

typedef enum {
	Node_Unknown = 0,

	Node_Integer,
	Node_Real,
	Node_String,
	Node_Identifier,
	Node_Unary,
	Node_Binary,

	Node_Type__COUNT,
} Node_Type;

typedef struct {
    union {
        Binary binary;
        Unary  unary;
		String ident;
		i64 integer;
		f64 real;
    } value;

	Node_Type type;
} Node;

typedef struct {
    Node nodes[NODE_POOL_SIZE];
    i32 usage;
} Node_Pool;

typedef struct {
    Node_Pool* pools[AST_POOL_COUNT];
    u32 active_pool_count;
    Arena* arena;
} AST;
