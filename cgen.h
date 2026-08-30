#pragma once
// Code generation library based on a superset of JSON with tags
//
// e.g.
//
// {
//    @align(64)
//    @struct
//    S:{
//        foo: int,
//        bar: "char*",
//    },
//
//    @enum(i32, prefix: "Thing")
//    E: [ A, B, C ],
// }
//
//                                map
//                               /   \
//  struct <-- align <-(tags)-- S     E --(tags)--> enum
//               |            /        \            |    \
//              64          map       array         i32   prefix
//                        /   |      /  |  \                |
//                     foo   bar    A   B   C             "Thing"
//                      |     |
//                     int   char*
//

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <string.h>

#if defined(__clang__) || defined(__GNUC__)
	#define attribute_format(fmt_pos, args_pos) __attribute__((format(printf, fmt_pos, args_pos)))
#else
	#define attribute_format(fmt_pos, args_pos)
#endif

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))
#define abs(x) ((x) < (0) ? (-(x)) : (x))

// Expansion to use `Str` with printf `%.*s` formatting
#define strf(s) (int)((s).len), ((s).v)

// Abort program with a message 
_Noreturn void panic_ex(char const* file, int line, char const* fmt, ...);

// Assert that predicate is true, panic otherwhise
void ensure_ex(bool predicate, char const* msg, char const* file, int line);

#define panic(fmt, ...) panic_ex(__FILE__, __LINE__, "" fmt "" __VA_OPT__(,) __VA_ARGS__)

#define ensure(pred, msg) ensure_ex((pred), (msg), __FILE__, __LINE__)

// String
typedef struct {
	char const* v;
	int len;
} Str;

// Wrap a C string literal in a Str declaration
#define str_lit(s) ((Str){ .v = "" s "", .len = sizeof(s) - 1 })

// Whether string `a` is equal to `b`
bool str_equal(Str a, Str b);

// Core writer method
typedef int (*IO_Stram_Func)(void* impl, char const* buf, int buflen);

// Interface used to write buffers to an arbitrary destination.
typedef struct {
	void* impl;
	IO_Stram_Func fn;
} IO_Stream;

// Write `buf` to writer, returns bytes written. Negative values indicate an error.
static inline
int io_write(IO_Stream w, char const* buf, int buflen){
	ensure(buflen >= 0, "buflen cannot be negative");
	return w.fn(w.impl, buf, buflen);
}

// Byte slice that can be written to
typedef struct {
	uint8_t* data;
	int len;
	int cap;
} Byte_Buffer;

// Write to byte buffer, this already conforms to Writer
int byte_buffer_write(void* impl, char const* buf, int buflen);

// Get byte buffer as a writer
static inline
Writer byte_buffer_writer(Byte_Buffer* b){
	return (Writer){
		.fn = byte_buffer_write,
		.impl = b,
	};
}

////~ Scanning

// Tokenizer state, advanced by the parser
typedef struct {
	char const* source;
	int len;
	int current;
	Arena* arena;
} Scanner;

// Types of token
typedef enum {
	Tk_Unknown = 0,

	Tk_Identifier,
	Tk_Tag,
	Tk_Number,
	Tk_String,

	Tk_CurlyOpen,
	Tk_CurlyClose,
	Tk_SquareOpen,
	Tk_SquareClose,
	Tk_ParenOpen,
	Tk_ParenClose,
	Tk_Colon,
	Tk_Comma,

	Tk_EndOfFile,

	TokenType__COUNT,
} TokenType;

// Token produced by scanner, may be attached to a literal value
typedef struct {
	Str lexeme;
	TokenType type;
	union {
		double number_value;
		Str string_value;
	};
} Token;

// Scan a number at the current position.
Token scan_number(Scanner* sc);

// Scan a double-quoted string at the current position.
Token scan_string(Scanner* sc);

// Scan an identifier at the current position.
Token scan_identifier(Scanner* sc);

// Scan a tag at the current position.
Token scan_tag(Scanner* sc);

// Advance the scanner, returning the scanned token. Numbers may be decimal or `0x`-prefixed hexadecimal.
Token scan_next_token(Scanner* sc);

// Peek the N+1 token
Token scan_peek_token(Scanner const* sc);

// Peek the N+2 token
Token scan_peek_token2(Scanner const* sc);


typedef struct Node Node;

// Type of AST node
typedef enum {
	Node_Unknown = 0,

	Node_Key,
	Node_Tag,
	Node_Array,
	Node_Map,

	Node_Identifier,
	Node_Number,
	Node_String,

	NodeType__COUNT,
} NodeType;

// Node of the AST, `string_value` is equivalent to lexeme for identifiers
struct Node {
	NodeType type;

	double number_value;
	Str string_value;
	Node* tag;

	Node* parent; // Parent node
	Node* first;  // First child
	Node* last;   // Last child
	Node* next;   // Next sibling
	Node* prev;   // Previous sibling
};

// Parser state to produce an AST
typedef struct {
	Scanner scan;
	Arena* arena;
} Parser;

// Append a sibling to a node, adjusting link pointers
void node_append_sibling(Node* target, Node* sibling);

// Append a child to a node, adjusting link pointers
void node_append_child(Node* target, Node* child);

// Allocate node
Node* node_make(Arena* a, NodeType type);

// Read a number from a node that carries one. Returns if read was successful. Output is set to a zero-value if failed
static inline
bool read_number(Node* node, double* out){
	bool ok = node && node->type == Node_Number;
	*out = ok ? node->number_value : 0;
	return ok;
}

// Read a string from a node that carries one. Returns if read was successful. Output is set to a zero-value if failed
static inline
bool read_str(Node* node, Str* out){
	bool ok = node && (node->type == Node_String || node->type == Node_Identifier || node->type == Node_Key);
	*out = ok ? node->string_value : (Str){0};
	return ok;
}

// Read an identifier from a node that carries one. Returns if read was successful. Output is set to a zero-value if failed
static inline
bool read_identifier(Node* node, Str* out){
	bool ok = node && (node->type == Node_Identifier || node->type == Node_Key);
	*out = ok ? node->string_value : (Str){0};
	return ok;
}

// Write S-expression style representation of AST
void write_ast(Writer w, Node const* node);

// Print ast to stdout (TODO: Remove)
void print_ast(Node const* node);

// Parse source and allocate nodes and their data on arena
Node* parse(Str source, Arena* a);

// Get a key from a map node, unnesting tags as needed. Returns NULL if the key is not found.
Node* map_get(Node* map, Str key);

// Get the N-th element of an array. Returns NULL if it is not found.
Node* array_get(Node* arr, int idx);

// Find a tag by scanning a key's tag chain. Returns NULL if it is not found.
Node* find_tag(Node* key, Str tag_name);

// Run callback for each node in root that has tag with name `tag_name`
void tag_foreach(Node* root, Str tag_name, void* userdata, void (*f) (Node* node, void* userdata));

// Get the N-th unnamed argument. Named arguments are not counted.
//
// Example: ("yes", x: 10, y: 20, [foo])
// arg_get_unnamed(tag, 1) --> [foo]
Node* arg_get_unnamed(Node* tag, int idx);

// Get a named argument from a tag. Returns NULL if it is not found.
Node* arg_get_named(Node* tag, Str name);

// Read the numerical value of a node
bool read_number(Node* node, double* out);

// Read the string value of a node
bool read_str(Node* node, Str* out);

// Clone string onto arena
Str str_clone(Str s, Arena* arena);

// Concat 2 strings onto arena
Str str_concat(Str a, Str b, Arena* arena);

// Converts string to snake case form
Str str_snake_case(Str s, Arena* a);

// Converts string to Ada case form
Str str_ada_case(Str s, Arena* a);

// Create a formatted string directly in arena
attribute_format(2, 3)
Str str_format(Arena* a, char const* fmt, ...);

