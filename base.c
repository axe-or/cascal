#include "base.h"

#define STB_SPRINTF_IMPLEMENTATION
#include "stb_sprintf.h"

#include <stdio.h>

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

// static inline
// char* stb_printf_writer_adapter(char const* buf, void* user, int len){
// 	Writer w = *((Writer*)user);
// 	write_to(w, buf, len);
// 	return NULL;
// }

// static
// int write_vfmt(Writer w, char const* format, va_list args){
// 	char buf[STB_SPRINTF_MIN + 64] = {0};
// 	return stbsp_vsprintfcb(stb_printf_writer_adapter, &w, &buf[0], format, args);
// }

// static
// void write_fmt(Writer w, char const* format, ...){
// 	va_list args;
// 	va_start(args, format);
// 	write_vfmt(w, format, args);
// 	va_end(args);
// }

attribute_format(3, 4)
_Noreturn
void panic_ex(char const* file, int line, char const* fmt, ...){
	static uint8_t panic_msg_buf[300] = {0};
	usize pos = 0;

	int n = stbsp_snprintf((char*)(&panic_msg_buf[0]), sizeof(panic_msg_buf), "(%s:%d) panic: ", file, line);
	if(n > 0){
		pos += n;
	} else {
		abort();
	}

	va_list args;
	va_start(args, fmt);
	stbsp_snprintf((char*)(&panic_msg_buf[0]), sizeof(panic_msg_buf), fmt, args);
	va_end(args);

	puts((char*)(&panic_msg_buf[0]));
	abort();
}

void ensure_ex(bool predicate, char const* msg, char const* file, int line){
	if(!predicate){
		printf("(%s:%d) assertion failed: %s", file, line, msg);
		fflush(stdout);
		abort();
	}
}

bool str_equal(String a, String b){
	if(a.len != b.len){ return false; }

	for(int i = 0; i < a.len; i += 1){
		if(a.v[i] != b.v[i]){
			return false;
		}
	}
	return true;
}

// int byte_buffer_write(void* impl, char const* buf, int buflen){
// 	Byte_Buffer* bb = impl;
// 	int n = max(0, min(bb->cap - bb->len, buflen));
// 	memcpy(bb->data, buf, n);
// 	bb->len += n;
// 	return n;
// }



////~ Arena

static inline
bool mem_valid_alignment(size_t align){
	return (align != 0) && ((align & (align - 1)) == 0);
}

static inline
uintptr_t mem_align_forward_ptr(uintptr_t p, uintptr_t a){
	ensure(mem_valid_alignment(a), "alignment must be a power of 2 greater than 0");
	uintptr_t mod = p & (a - 1); /* Fast modulo for powers of 2 */
	if(mod > 0){
		p += (a - mod);
	}
	return p;
}

void arena_reset(Arena* a){
	a->offset = 0;
	a->last_allocation = NULL;
	a->last_allocation_size = 0;
}

bool arena_owns(Arena const* a, void const* ptr){
	uintptr_t p    = (uintptr_t)ptr;
	uintptr_t base = (uintptr_t)a->data;
	uintptr_t end  = base + a->capacity;

	return p >= base && p < end;
}

void* arena_alloc(Arena* a, size_t size, size_t align){
	if(size == 0){
		return NULL;
	}

	ensure(mem_valid_alignment(align), "invalid arena alignment");

	uintptr_t base    = (uintptr_t)a->data;
	uintptr_t current = base + a->offset;
	uintptr_t aligned = mem_align_forward_ptr(current, align);

	size_t padding = aligned - current;

	// Avoid `padding + size` overflowing.
	if(padding > a->capacity - a->offset){
		return NULL;
	}

	size_t available = a->capacity - a->offset - padding;
	if(size > available){
		return NULL;
	}

	size_t required = padding + size;

	a->offset += required;

	void* allocation = (void*)aligned;

	mem_zero(allocation, size);

	a->last_allocation = allocation;
	a->last_allocation_size = size;
	a->peak_usage = max(a->peak_usage, a->offset);

	return allocation;
}

bool arena_resize(Arena* a, void* ptr, size_t new_size){
	if(ptr == NULL){
		return false;
	}

	ensure(arena_owns(a, ptr), "pointer not owned by arena");

	if(ptr != a->last_allocation){
		return false;
	}

	uintptr_t base      = (uintptr_t)a->data;
	uintptr_t allocation = (uintptr_t)ptr;

	size_t allocation_offset = allocation - base;

	if(new_size > a->capacity - allocation_offset){
		return false;
	}

	size_t old_size = a->last_allocation_size;

	a->offset = allocation_offset + new_size;
	a->last_allocation_size = new_size;

	if(new_size > old_size){
		mem_zero(
			(uint8_t*)ptr + old_size,
			new_size - old_size
		);
	}

	a->peak_usage = max(a->peak_usage, a->offset);
	return true;
}

void* arena_realloc(Arena* a, void* ptr, size_t old_size, size_t new_size, size_t align){
	if(ptr == NULL){
		return arena_alloc(a, new_size, align);
	}

	ensure(arena_owns(a, ptr), "pointer not owned by arena");

	if(arena_resize(a, ptr, new_size)){
		return ptr;
	}

	void* new_ptr = arena_alloc(a, new_size, align);
	if(new_ptr == NULL){
		return NULL;
	}

	mem_copy(new_ptr, ptr, min(old_size, new_size));

	return new_ptr;
}

Arena arena_from_buffer(void* buffer, size_t buffer_size){
	ensure(buffer != NULL, "invalid arena buffer");

	return (Arena){
		.data = buffer,
		.capacity = buffer_size,
		.offset = 0,
		.last_allocation = NULL,
		.last_allocation_size = 0,
	};
}

////~ String builder

String_Builder sb_make(int cap, Arena* a){
	char* buf = arena_make(a, char, cap);
	ensure(buf, "alloc error");

	return (String_Builder){
		.buf = buf,
		.len = 0,
		.cap = cap,
		.arena = a,
	};
}

void sb_destroy(String_Builder* sb){
	arena_resize(sb->arena, sb->buf, 0);
	*sb = (String_Builder){0};
}

void sb_clear(String_Builder* sb){
	sb->len = 0;
	if(sb->cap > 0) sb->buf[0] = 0;
}

void sb_write(String_Builder* sb, char const* data, int n){
	if((sb->len + n) >= sb->cap){
		int new_cap = max(16, max(sb->len + n + 1, sb->cap * 2));
		sb->buf = arena_realloc(sb->arena, sb->buf, sb->cap, new_cap, alignof(char));
		ensure(sb->buf, "alloc error");
		sb->cap = new_cap;
	}

	mem_copy(&sb->buf[sb->len], data, n);
	sb->len += n;
	sb->buf[sb->len] = 0;
}

static inline
char* stb_printf_str_builder_adapter(char const* buf, void* user, int len){
	String_Builder* sb = (String_Builder*)user;
	sb_write(sb, buf, len);
	return NULL;
}

int sb_write_vfmt(String_Builder* sb, char const* format, va_list args){
	char buf[STB_SPRINTF_MIN + 64] = {0};
	return stbsp_vsprintfcb(stb_printf_str_builder_adapter, sb, &buf[0], format, args);
}

attribute_format(2, 3)
int sb_write_fmt(String_Builder* sb, char const* format, ...){
	va_list args;
	va_start(args, format);
	int res = sb_write_vfmt(sb, format, args);
	va_end(args);
	return res;
}

void sb_write_char(String_Builder* sb, char c){
	sb_write(sb, &c, 1);
}

String sb_build(String_Builder* sb){
	char* data = sb->buf;
	sb_write_char(sb, '\0');

	String res = {.v = data, .len = sb->len - 1};
	sb->cap = 0;
	sb->len = 0;
	sb->buf = NULL;

	return res;
}

String sb_get(String_Builder* sb){
	char* data = sb->buf;

	if(sb->len == sb->cap){ // Force append a null terminator if full
		sb_write_char(sb, '\0');
		sb->len -= 1;
	}
	else{
		sb->buf[sb->len] = '\0';
	}

	String res = {.v = data, .len = sb->len};
	sb->cap = 0;
	sb->len = 0;
	sb->buf = NULL;

	return res;
}
