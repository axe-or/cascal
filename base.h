#pragma once

////~ Attributes and Compiler specifics
#if __STDC_VERSION__ >= 202311L
	/* Nice, we have native typeof support */
#else
	#if defined(__clang__) || defined(__GNUC__) || defined(_MSC_VER) || defined(__TINYC__)
		#define typeof __typeof__
	#else
		#error "Could not define typeof macro"
	#endif
#endif

#if defined(_MSC_VER)
	#define attribute_force_inline      __forceinline
	#define attribute_force_inline_func __forceinline static
#elif defined(__clang__) || defined(__GNUC__)
	#define attribute_force_inline      __attribute__((always_inline))
	#define attribute_force_inline_func __attribute__((always_inline)) static inline
#else
	#define attribute_force_inline
	#define attribute_force_inline_func static inline
#endif


#if defined(__clang__) || defined(__GNUC__)
	#define attribute_format(fmt_pos, args_pos) __attribute__((format (printf, fmt_pos, args_pos)))
#else
	#define attribute_format(fmt, args)
#endif

////~ Auto platform detection
#if !defined(BUILD_PLATFORM_WINDOWS) && !defined(BUILD_PLATFORM_LINUX) && !defined(BUILD_PLATFORM_WASI)
	#if defined(_WIN32) || defined(_WIN64)
		#define BUILD_PLATFORM_WINDOWS
	#elif defined(__linux__)
		#define BUILD_PLATFORM_LINUX
	#elif defined(__wasi__)
		#define BUILD_PLATFORM_WASI
	#endif
#endif

#if defined(BUILD_PLATFORM_WINDOWS)
	#ifndef _CRT_SECURE_NO_WARNINGS
		#define _CRT_SECURE_NO_WARNINGS
	#endif

	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif
#elif defined(BUILD_PLATFORM_LINUX)
	#ifndef _DEFAULT_SOURCE
		#define _DEFAULT_SOURCE
	#endif
#elif defined(BUILD_PLATFORM_WASI)
#else
	#error "Undefined platform macro"
#endif

#include <stddef.h>
#include <stdarg.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

////~ Basic types & Utilities
typedef int8_t i8;
typedef uint8_t u8;

typedef int16_t i16;
typedef uint16_t u16;

typedef int32_t i32;
typedef uint32_t u32;

typedef int64_t i64;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

typedef int32_t rune;
typedef uintptr_t uintptr;

typedef size_t    usize;
typedef ptrdiff_t isize;

typedef const char* cstring;

typedef _Atomic(int) AtomicInt;
typedef _Atomic(bool) AtomicBool;

#define min(x, y) (((x) < (y)) ? (x) : (y))

#define max(x, y) (((x) > (y)) ? (x) : (y))

#define clamp(lo, x, hi) min(max((lo), (x)), (hi))

// Helpers that use preprocessor expansion tricks to "glue" identifiers
#define ident_concat0(x, y) x##y
#define ident_concat1(x, y) ident_concat0(x, y)
#define ident_concat2(x, y) ident_concat1(x, y)
#define ident_concat(x, y)  ident_concat2(x, y)
#define ident_counter(x)    ident_concat(x, __COUNTER__)

////~ Assertions

// Abort program with a message 
_Noreturn void panic_ex(char const* file, int line, char const* fmt, ...);

// Assert that predicate is true, panic otherwhise
void ensure_ex(bool predicate, char const* msg, char const* file, int line);

#define panic(fmt, ...) panic_ex(__FILE__, __LINE__, "" fmt "" __VA_OPT__(,) __VA_ARGS__)

#define ensure(pred, msg) ensure_ex((pred), (msg), __FILE__, __LINE__)

// Helper for unimplemented sections of code
#define TODO() panic_ex("TODO", __FILE__, __LINE__)

////~ String

// Expansion to use `String` with printf `%.*s` formatting
#define strf(s) (int)((s).len), ((s).v)

// UTF-8 encoded slice of bytes
typedef struct {
	char const* v;
	isize len;
} String;

// Helper macro to turn usual C-strings into sized strings
#define strlit(s) ((String){.v = (char const*)("" s ""), .len = (sizeof(s) - 1)})

// Compare two strings by length and contents.
bool str_equal(String a, String b);

// Length of a C-style string
static inline
isize cstring_len(cstring cs) {
	isize n = 0;
	while(cs[n] != 0){
		n += 1;
	}
	return n;
}

// The error unicode codepoint
#define RUNE_ERROR ((rune)0xfffd)

// Decoded form of a unicode codepoint
typedef struct {
	rune codepoint;
	i32  size;
} Rune_Decoded;

// Encoded form of a unicode codepoint
typedef struct {
	u8  bytes[4];
	i32 size;
} Rune_Encoded;

// Encode a codepoint `r` to UTF-8
Rune_Encoded rune_encode(rune r);

// Decode the first rune of a UTF-8 encoded buffer
Rune_Decoded rune_decode(u8 const* buf, u32 buflen);

////~ Slice
#define Slice(T) struct { T* v; isize len; }

#define slice_len(S) ((S).len)

#define slice_take(S, N) \
	(ensure_ex((N) <= (S).len, "cannot take more than slice length", __FILE__, __LINE__) \
	? (typeof(S)){ .v = (S).v, .len = (N) } \
	: (typeof(S)){0})

#define slice_skip(S, N) \
	(ensure_ex((N) <= (S).len, "cannot skip more than slice length", __FILE__, __LINE__) \
	? (typeof(S)){ .v = &(S).v[(N)], .len = (S).len - (N) } \
	: (typeof(S)){0})

#define slice(S, A, B) \
	(ensure_ex((B) <= (S).len && (A) <= (B), "invalid slice indices", __FILE__, __LINE__) \
	? (typeof(S)){ .v = &(S).v[(A)], .len = (B) - (A) } \
	: (typeof(S)){0})


// Arena (linear) allocator, backed by a static sized buffer
typedef struct {
	u8* data;
	usize capacity;
	usize offset;

	void* last_allocation;
	usize last_allocation_size;
	usize peak_usage;
} Arena;

#define arena_declare_static(name, size) static u8 name ##_memory[(size)]; Arena name = {.data = &name ##_memory[0], .capacity = (size)}

// Allocate `count` instances of `type` in arena
#define arena_make(arena, type, count) \
	(type*)arena_alloc((arena), sizeof(type) * (count), alignof(type))

// Push a value to area
#define arena_make(arena, type, count) \
	(type*)arena_alloc((arena), sizeof(type) * (count), alignof(type))

// Discard all allocations without modifying the backing buffer.
void arena_reset(Arena* a);

// Check whether a pointer lies inside the arena's backing buffer.
bool arena_owns(Arena const* a, void const* ptr);

// Allocate `size` bytes with the requested alignment. Returns NULL if the buffer does not have enough space.
void* arena_alloc(Arena* a, size_t size, size_t align);

// Attempt to resize an allocation in-place. This only succeeds for the most recent allocation.
bool arena_resize(Arena* a, void* ptr, size_t new_size);

// Reallocate an arena allocation. Attempts an in-place resize first, then falls back to allocate + copy.
void* arena_realloc(Arena* a, void* ptr, size_t old_size, size_t new_size, size_t align);

// Create an arena backed by caller-owned memory.
Arena arena_from_buffer(void* buffer, size_t buffer_size);

// Restore point for arena
typedef struct {
	Arena* arena;
	size_t offset;
} Arena_Reg;

// Get a checkpoint of arena state
static inline
Arena_Reg arena_region_begin(Arena* a){
	return (Arena_Reg){.arena = a, .offset = a->offset};
}

// Reset arena to a checkpoint
static inline
void arena_region_end(Arena_Reg r){
	r.arena->offset = r.offset;
}

////~ String Builder

// String builder that grows dynamically using an arena
typedef struct {
	char* buf;
	isize len;
	isize cap;
	Arena* arena;
} String_Builder;

// Create a string builder with preallocated capacity.
String_Builder sb_make(int cap, Arena* a);

// Destroy the builder.
void sb_destroy(String_Builder* sb);

// Clear the builder.
void sb_clear(String_Builder* sb);

// Append data to the builder.
isize sb_write(String_Builder* sb, char const* data, isize n);

// Write single byte to buffer
isize sb_write_byte(String_Builder* sb, u8 c);

// Write an UTF-8 encoded rune to buffer
isize sb_write_rune(String_Builder* sb, rune r);

// Pop the string from the builder and reset it. The result is null terminated for C-string compatibility.
String sb_build(String_Builder* sb);

// Get the builder's string and invalidate the builder. The result is null
// terminated for C-string compatibility. Modifying the builder makes the string invalid
String sb_get(String_Builder* sb);

////~ Memory

extern void* memset(void *dst, int val, size_t size);
extern void* memmove(void*, void const*, usize);

attribute_force_inline_func
void mem_copy(void* dst, void const* src, usize n){
	memmove(dst, src, n);
}

attribute_force_inline_func
void mem_zero(void* ptr, usize n){
	memset(ptr, 0, n);
}

////~ IO interface

typedef enum {
	IO_Query = 0,
	IO_Read  = 1 << 0,
	IO_Write = 1 << 1,
	IO_Close = 1 << 2,
} IO_Mode;

typedef enum {
	IO_Err_None = 0,

	IO_Err_EOF = -1,
	IO_Err_Closed = -2,
	IO_Err_TooBig = -3,
	IO_Err_Unsupported = -4,

	IO_Err_Other = -255,
} IO_Error;

// Perform an IO operation, returns < 0 on failure, check IO_Error for details.
typedef isize (*IO_Stream_Func)(void* impl, IO_Mode mode, u8* buf, isize buflen);

// General reader/writer interface abstraction
typedef struct {
	void* impl;
	IO_Stream_Func fn;
} IO_Stream;

static inline
i32 io_query(IO_Stream w, u8* buf, isize buflen){
	return w.fn(w.impl, IO_Query, buf, buflen);
}

static inline
isize io_read(IO_Stream r, u8* buf, isize buflen){
	return r.fn(r.impl, IO_Read, buf, buflen);
}

static inline
isize io_write(IO_Stream w, u8* buf, isize buflen){
	return w.fn(w.impl, IO_Write, buf, buflen);
}

// Write-only type guard
typedef struct { IO_Stream stream; } IO_Writer;

// Read-only type guard
typedef struct { IO_Stream stream; } IO_Reader;

// Create a non-owning writer that appends to a string builder.
IO_Writer sb_writer(String_Builder* sb);

////~ Formatting

attribute_format(2, 3)
isize fmt_write(IO_Writer w, char const* fmt, ...);

isize fmt_writev(IO_Writer w, char const* fmt, va_list args);
