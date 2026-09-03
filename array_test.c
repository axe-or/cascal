#include "array.h"
#include "testing.h"

typedef struct {
	i32 x;
	i32 y;
} Array_Test_Point;

typedef struct {
	alignas(32) u8 bytes[32];
	i32 value;
} Array_Test_Aligned;

void array_tests(Test* t){
	t->name = "array";

	{
		u8 memory[4096];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		Array(i32) array;
		t_pred(t, array_init(&array, &arena, 0));

		t_pred(t, array.v == NULL);
		t_pred(t, array.raw.len == 0);
		t_pred(t, array.raw.cap == 0);
		t_pred(t, array.raw.arena == &arena);
		t_pred(t, array_reserve(&array, 8));
		t_pred(t, array.raw.cap == 8);

		for(i32 i = 0; i < 40; i += 1){
			t_ensure(t, array_push(&array, i * 3), "array push failed");
		}

		bool values_are_preserved = true;
		for(i32 i = 0; i < 40; i += 1){
			values_are_preserved = values_are_preserved
				&& *array_get(array, i) == i * 3;
		}
		t_pred(t, values_are_preserved);
		t_pred(t, array.raw.len == 40);
		t_pred(t, array.raw.cap >= array.raw.len);
		t_pred(t, array_reserve(&array, 4));
		t_pred(t, array.raw.cap >= 40);

		i32 popped = 0;
		t_pred(t, array_pop(&array, &popped));
		t_pred(t, popped == 39 * 3);
		t_pred(t, array_pop(&array, NULL));
		t_pred(t, array.raw.len == 38);
		while(array_pop(&array, NULL)){}
		t_pred(t, array.raw.len == 0);
		t_pred(t, !array_pop(&array, &popped));
	}

	{
		u8 memory[4096];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		Array(Array_Test_Point) array;
		t_pred(t, array_init(&array, &arena, 2));

		Array_Test_Point first = {.x = 10, .y = 20};
		Array_Test_Point second = {.x = 30, .y = 40};
		t_pred(t, array_push(&array, first));
		t_pred(t, array_push(&array, second));
		t_pred(t, array_get(array, 0)->x == 10);
		t_pred(t, array_get(array, 0)->y == 20);
		t_pred(t, array_get(array, 1)->x == 30);
		t_pred(t, array_get(array, 1)->y == 40);
	}

	{
		u8 memory[4096];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		Array(i32) array;
		t_pred(t, array_init(&array, &arena, 16));

		for(i32 i = 0; i < 16; i += 1){
			t_ensure(t, array_push(&array, i), "initial array push failed");
		}
		i32* original = array.v;
		t_ensure(t, arena_make(&arena, u8, 1) != NULL, "blocker allocation failed");
		t_pred(t, array_push(&array, 16));
		t_pred(t, array.v != original);

		bool values_are_preserved = true;
		for(i32 i = 0; i < 17; i += 1){
			values_are_preserved = values_are_preserved && array.v[i] == i;
		}
		t_pred(t, values_are_preserved);
	}

	{
		u8 memory[4096];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		Array(Array_Test_Aligned) array;
		t_pred(t, array_init(&array, &arena, 1));
		Array_Test_Aligned value = {.value = 42};

		t_pred(t, array_push(&array, value));
		t_pred(t, (uintptr)array.v % alignof(Array_Test_Aligned) == 0);
		t_pred(t, array.v[0].value == 42);
	}

	{
		u8 memory[32];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		Array(i64) array;
		t_pred(t, !array_init(&array, &arena, 8));

		t_pred(t, !array_push(&array, 123));
		t_pred(t, array.v == NULL);
		t_pred(t, array.raw.len == 0);
		t_pred(t, array.raw.cap == 0);
	}

	{
		u8 memory[96];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		Array(i32) array;
		t_pred(t, array_init(&array, &arena, 8));
		for(i32 i = 0; i < 8; i += 1){
			t_ensure(t, array_push(&array, i), "initial array push failed");
		}

		i32* original = array.v;
		t_ensure(t, arena_make(&arena, u8, 32) != NULL, "blocker allocation failed");
		t_pred(t, !array_push(&array, 99));
		t_pred(t, array.v == original);
		t_pred(t, array.raw.len == 8);
		t_pred(t, array.raw.cap == 8);

		bool values_are_preserved = true;
		for(i32 i = 0; i < 8; i += 1){
			values_are_preserved = values_are_preserved && array.v[i] == i;
		}
		t_pred(t, values_are_preserved);
	}

	{
		u8 memory[64];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		Array(i32) array;
		t_pred(t, !array_init(&array, &arena, -1));
		t_pred(t, array.v == NULL);
		t_pred(t, array.raw.len == 0);
		t_pred(t, array.raw.cap == 0);
	}

	{
		u8 memory[64];
		Arena arena = arena_from_buffer(memory, sizeof(memory));
		Array(i32) array;
		t_pred(t, array_init(&array, &arena, 0));
		usize overflowing_cap = SIZE_MAX / sizeof(*array.v) + 2;

		t_pred(t, !array_reserve(&array, overflowing_cap));
		t_pred(t, array.v == NULL);
		t_pred(t, array.raw.cap == 0);
	}
}
