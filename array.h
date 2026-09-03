#pragma once
#include "base.h"

#define Array(T) union { T* v; Raw_Array raw; }

#define array_elem_size(arr) sizeof(*(arr).v)

#define array_elem_align(arr) alignof(typeof(*(arr).v))

#define array_elem_ptr_type(arr) typeof((arr).v)

#define array_elem_type(arr) typeof(*(arr).v)

#define array_elem_ptr(arr) typeof(*(arr).v)

#define array_init(arr, arena, cap) raw_array_init(&(arr)->raw, (arena), (cap), array_elem_size(*arr), array_elem_align(*arr))

#define array_push(arr, val) \
	raw_array_push(&(arr)->raw, (array_elem_type(*(arr))[1]){ (val) }, array_elem_size(*arr), array_elem_align(*arr))

#define array_get(arr, idx) \
	((array_elem_ptr_type((arr)))raw_array_get((arr).raw, (idx), array_elem_size(arr)))

#define array_reserve(arr, new_cap) \
	raw_array_reserve(&(arr)->raw, (new_cap), array_elem_size(*arr), array_elem_align(*arr))

#define array_pop(arr, out_ptr) \
	raw_array_pop(&(arr)->raw, (out_ptr), array_elem_size(*arr))

typedef struct {
	void* data;
	isize len;
	isize cap;
	Arena* arena;
} Raw_Array;

_Static_assert(offsetof(Raw_Array, data) == 0, "Invalid Raw_Array layout");

static inline
bool raw_array_reserve(Raw_Array* arr, usize new_cap, usize size, usize align){
	if(arr == NULL || arr->arena == NULL || arr->cap < 0 || size == 0
		|| new_cap > PTRDIFF_MAX || new_cap > SIZE_MAX / size
		|| (usize)arr->cap > SIZE_MAX / size){
		return false;
	}
	if((usize)arr->cap >= new_cap){
		return true;
	}

	void* new_data = arena_realloc(
		arr->arena,
		arr->data,
		size * (usize)arr->cap,
		size * new_cap,
		align
	);
	if(!new_data){
		return false;
	}

	arr->cap = (isize)new_cap;
	arr->data = new_data;
	return true;
}

static inline
bool raw_array_init(Raw_Array* arr, Arena* arena, isize initial_cap, usize size, usize align){
	if(arr == NULL){
		return false;
	}
	mem_zero(arr, sizeof(*arr));
	arr->arena = arena;
	if(initial_cap < 0){
		return false;
	}
	return raw_array_reserve(arr, (usize)initial_cap, size, align);
}

static inline
void* raw_array_get(Raw_Array arr, isize idx, usize size){
	uintptr ptr = (uintptr)arr.data;
	return (void*)(ptr + ((usize)idx * size));
}

static inline
bool raw_array_push(Raw_Array* arr, void const* elem, usize size, usize align){
	if(arr == NULL || elem == NULL || arr->len < 0 || arr->cap < 0 || arr->len > arr->cap){
		return false;
	}
	if(arr->len >= arr->cap ){
		if(arr->cap > PTRDIFF_MAX / 2){
			return false;
		}
		usize new_cap = (usize)max(8, arr->cap * 2);
		if(!raw_array_reserve(arr, new_cap, size, align)) {
			return false;
		}
	}

	void* last = raw_array_get(*arr, arr->len, size);
	mem_copy(last, elem, size);
	arr->len += 1;
	return true;
}

static inline
bool raw_array_pop(Raw_Array* arr, void* out, usize size){
	if(arr == NULL || arr->len <= 0 || arr->len > arr->cap){
		return false;
	}

	if(out){
		void* last = raw_array_get(*arr, arr->len - 1, size);
		mem_copy(out, last, size);
	}
	arr->len -= 1;

	return true;
}
