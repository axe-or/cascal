#pragma once
#include "base.h"

#define Array(T) union { T* v; Raw_Array raw; }

typedef struct {
	void* data;
	isize len;
	isize cap;
	Arena* arena;
} Raw_Array;

static inline
bool raw_array_reserve(Raw_Array* arr, usize new_cap, u32 size, u32 align){
	if(arr->cap >= new_cap){
		return true;
	}

	void* new_data = arena_realloc(arr->arena, arr->data, size * arr->cap, size * new_cap, align);
	if(!new_data){
		return false;
	}

	arr->cap = new_cap;
	arr->data = new_data;
	return true;
}

static inline
void* raw_array_get(Raw_Array* arr, isize idx, u32 size){
	uintptr ptr = (uintptr)arr->data;
	return (void*)(ptr + (idx * size));
}

static inline
bool raw_array_push(Raw_Array* arr, void const* elem, u32 size, u32 align){
	if(arr->len >= arr->cap ){
		usize new_cap = max(16, arr->cap * 2);
		if(!raw_array_reserve(arr, new_cap, size, align)) {
			return false;
		}
	}

	void* last = raw_array_get(arr, arr->len, size);
	mem_copy(last, elem, size);
	arr->len += 1;
	return true;
}

static inline
bool raw_array_pop(Raw_Array* arr, void* out, u32 size){
	if(arr->len <= 0){
		return false;
	}

	if(out){
		void* last = raw_array_get(arr, arr->len - 1, size);
		mem_copy(out, last, size);
	}
	arr->len -= 1;

	return true;
}
