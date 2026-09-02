#pragma once

#include "base.h"

////~ Scanning
#define TOKEN_KEYWORDS \
	X(True, "true") \
	X(False, "false") \
	X(If, "if") \
	X(Else, "else") \
	X(For, "for") \
	X(While, "while") \
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
	X(Arrow, "->") \
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
	Err_MismatchedListCardinality,

	Error_Type__COUNT,
};

typedef struct {
	String file;
	i32 offset;
	u8 type;

	union {
		rune character;
		Token_Type token_type;
		i32 cardinality;
	} expected;

	union {
		rune character;
		Token_Type token_type;
		i32 cardinality;
	} got;
} Error;

String error_type_name(enum Error_Type t);

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

#define NODE_AST_POOL_BITS (5ull)

#define NODE_AST_POOL_OFFSET_BITS (32ull - NODE_AST_POOL_BITS)

#define NODE_POOL_SIZE (1024ull)

#define AST_POOL_COUNT (1ull << NODE_AST_POOL_BITS)

_Static_assert(NODE_POOL_SIZE < (1ull << NODE_AST_POOL_OFFSET_BITS), "node pool size is too big");

_Static_assert((NODE_POOL_SIZE & (NODE_POOL_SIZE - 1)) == 0, "node pool size must be a power of 2");

typedef struct Node Node;
typedef struct Parser_Type Parser_Type;

typedef enum {
	ParserType_Unknown = 0,

	ParserType_Named,
	ParserType_Slice,
	ParserType_Array,
	ParserType_Pointer,

	ParserType__COUNT,
} Parser_Type_Kind;

struct Parser_Type {
	union {
		String name;

		struct {
			Node* element;
			u32 length;
		};
	} value;

	Parser_Type_Kind kind;
};

typedef struct {
	Node* first;
	Node* last;
} Node_List;


typedef struct {
    Node* left;
    Node* right;
    Token_Type op;
} Binary;

typedef struct {
    Node* operand;
    Token_Type op;
} Unary;

typedef struct {
	Node* object;
	Node* idx;
} Index;

typedef struct {
	String identifier;
	Node* type;
} Field;

typedef struct {
	String name;
	Node_List args; // All `Field`
	Node_List returns; // Optional, list of type nodes
	Node* body;
} Proc_Definition;

typedef struct {
	Node_List left;
	Node_List right;
} Assignment;

typedef struct {
	Node_List idents; // All must be identifiers
	Node* type;
	Node_List values;
} Var_Definition;

typedef struct {
	Node* callable;
	Node_List args;
} Call;

typedef struct {
	Node_List statements;
} Block;

typedef struct {
	Node_List values;
} Return_Statement;

typedef struct {
	Node* condition;
	Node* then_block;
	Node* else_branch;
} If_Statement;

typedef struct {
	Node* condition;
	Node* body;
} While_Statement;

typedef enum {
	Node_Unknown = 0,

	Node_Integer,
	Node_Real,
	Node_Boolean,
	Node_String,
	Node_Identifier,
	Node_Unary,
	Node_Binary,
	Node_Index,
	Node_Call,
	Node_Field,
	Node_ParserType,
	Node_VarDefinition,
	Node_Assignment,
	Node_Block,
	Node_Return,
	Node_Break,
	Node_Continue,
	Node_If,
	Node_While,
	Node_ProcDefinition,

	Node_Type__COUNT,
} Node_Type;

struct Node {
	Node* parent;
	Node* next;

    union {
		bool boolean;
		i64 integer;
		f64 real;
		String str;
		String ident;

        Unary unary;
        Binary binary;
		Index index;
		Call call;
		Field field;
		Parser_Type parser_type;
		Var_Definition var_definition;
		Assignment assignment;
		Block block;
		Return_Statement return_statement;
		If_Statement if_statement;
		While_Statement while_statement;
		Proc_Definition proc_definition;
    } value;

	Node_Type type;
};

static inline
i32 node_list_cardinality(Node_List l){
	i32 n = 0;
	for(Node* cur = l.first; cur != NULL; cur = cur->next){
		n += 1;
	}
	return n;
}

// Format a node as an S-expression
isize node_format(IO_Writer writer, Node* node);

typedef struct {
	Node* root;
    Arena* arena;
} AST;

typedef struct {
    Scanner scanner;
	AST ast;
	Error error;
} Parser;

typedef struct {
	Node* node;
	Node* last_node;
	Error error;
} Parser_Result;

Error parse(String source, AST* ast, Arena* arena);

////~ Types

enum Type_Kind {
    Type_None = 0,
    Type_Primitive,
    Type_Distinct,
    Type_Pointer,
    Type_Array,
    Type_Slice,

    Type_Kind__COUNT,
};

typedef struct Type Type;

typedef struct {
    Type* inner;
} Pointer_Type;

typedef struct {
    Type* inner;
} Slice_Type;

typedef struct {
    Type* inner;
    i32 size;
} Array_Type;

typedef struct {
    String name;
    Type* inner;
} Distinct_Type;

typedef struct {
    Type** returns;
    Type** args;

    u16 returns_len;
    u16 args_len;
} Proc_Type;

typedef enum {
	Prim_None = 0,

	Prim_Int,
	Prim_Real,
	Prim_Bool,
	Prim_Rune,
	Prim_String,

	Prim__COUNT,
} Primitive_Type;

struct Type {
    union {
        Primitive_Type primitive;
        Distinct_Type distinct;
        Pointer_Type pointer;
        Slice_Type slice;
        Array_Type array;
        Proc_Type proc;
    };
    u8 kind;
};

typedef struct { u32 v; } Type_ID;

// Type interner
typedef struct {
	Type* types;
	// map[type_hash]array(Type_ID)
	isize len;
	isize cap;
} Type_Arena;

bool type_eq(Type const* a, Type const* b);

// Type_ID type_intern(Type_Arena* ta, Type* t);

