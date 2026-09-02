#include "base.h"

#define INLINE_COUNT 2

#define T i32

// Array that stores up to INLINE_COUNT elements inline, if it blows past the
// capacity it becomes heap allocated
typedef struct {
    union {
        T* data;
        T inline_data[INLINE_COUNT];
    };

    i32 len;
    i32 cap;
    Arena* arena;
} Small_Array;

static inline
void sm_arr_init(Small_Array* arr, Arena* arena){
    mem_zero(arr, sizeof(*arr));
    arr->cap = INLINE_COUNT;
    arr->arena = arena;
}

// static inline
// bool sm_arr_push(Small_Array* arr, T val){
// }

static inline
bool sm_arr_reserve(Small_Array* arr, i32 new_cap){
    if(new_cap < arr->cap){ return true; }

    if(arr->cap <= INLINE_COUNT){
        // inline -> heap
        T* new_data = arena_make(arr->arena, T, new_cap);
        if(!new_data){ return false; }

        // IMPORTANT: Copy over inline data before setting data pointer
        mem_copy(new_data, &arr->inline_data[0], sizeof(T) * arr->len);

        arr->cap = new_cap;
        arr->data = new_data;
    }
    else {
        // heap -> heap
        T* new_data = (T*)arena_realloc(arr->arena, arr->data, arr->cap * sizeof(T), new_cap * sizeof(T), alignof(T));
        if(!new_data){ return false; }

        arr->cap = new_cap;
        arr->data = new_data;
    }

    return true;
}

static inline
bool sm_arr_push(Small_Array* arr, T val){
    if(arr->cap > INLINE_COUNT){
    }
    else {
        if(arr->len >= arr->cap){
            sm_arr_reserve(arr, arr->cap * 2);
        }
    }
}

#undef T