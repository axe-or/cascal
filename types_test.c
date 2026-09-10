#include "base.h"
#include "lang.h"
#include "testing.h"

// White-box fixture: isolate lookup/reserve from construction.
static Type_Arena type_test_fixture(Arena* arena, usize cap){
	Type_Arena ta = {.arena = arena, .cap = cap, .len = 1};
	ta.types = arena_make(arena, Type, cap);
	ta.next_hash = arena_make(arena, Type_ID, cap);
	ensure(ta.types && ta.next_hash, "test allocation failed");
	ensure(type_id_hash_init(&ta.id_by_hash, (u32)cap, arena), "test table allocation failed");
	return ta;
}

static void interner_missing(Test* t){
	t->name = "interner-missing";
	u8 memory[4096];
	Arena arena = arena_from_buffer(memory, sizeof(memory));
	Type_ID_By_Hash table;
	t_pred(t, type_id_hash_init(&table, 16, &arena));
	t_pred(t, type_id_hash_get(&table, 123) == NULL);
	// Keys 0 and 1 share the table's normalized hash, but remain different keys.
	type_id_hash_insert(&table, 0, (Type_ID){7});
	type_id_hash_insert(&table, 1, (Type_ID){8});
	t_pred(t, type_id_hash_get(&table, 0)->v == 7);
	t_pred(t, type_id_hash_get(&table, 1)->v == 8);
}

static void interner_chain(Test* t){
	t->name = "interner-chain";
	u8 memory[16384];
	Arena arena = arena_from_buffer(memory, sizeof(memory));
	Type_Arena ta = type_test_fixture(&arena, 16);
	Type values[] = {
		{.kind = Type_Primitive, .primitive = Prim_Int},
		{.kind = Type_Primitive, .primitive = Prim_Real},
		{.kind = Type_Primitive, .primitive = Prim_Bool},
	};
	// Inject a collision chain to exercise traversal without depending on hash outputs.
	for(u32 i = 0; i < 3; i += 1){
		ta.types[i + 1] = values[i];
		ta.next_hash[i + 1] = (Type_ID){i};
	}
	ta.len = 4;
	type_id_hash_insert(&ta.id_by_hash, 42, (Type_ID){3});
	for(u32 i = 0; i < 3; i += 1){
		t_pred(t, type_arena_find(&ta, 42, values[i]).v == i + 1);
	}
	t_pred(t, type_arena_find(&ta, 42, (Type){.kind = Type_Primitive, .primitive = Prim_Rune}).v == 0);
	t_pred(t, type_arena_get(&ta, (Type_ID){0}) == NULL);
	t_pred(t, type_arena_get(&ta, (Type_ID){4}) == NULL);
	t_pred(t, type_arena_get(&ta, (Type_ID){UINT32_MAX}) == NULL);
	t_pred(t, type_arena_reserve(&ta, 32));
	for(u32 i = 0; i < 3; i += 1){
		t_pred(t, type_eq(*type_arena_get(&ta, (Type_ID){i + 1}), values[i]));
		t_pred(t, type_arena_find(&ta, 42, values[i]).v == i + 1);
	}
}

static void interner_capacity(Test* t){
	t->name = "interner-capacity";
	u8 memory[16384];
	Arena arena = arena_from_buffer(memory, sizeof(memory));
	Type_Arena ta = type_test_fixture(&arena, 16);
	Type value = {.kind = Type_Primitive, .primitive = Prim_Int};
	// An empty chain isolates insertion from the missing-key lookup bug.
	type_id_hash_insert(&ta.id_by_hash, type_hash(&value), (Type_ID){0});
	Type_ID id = type_intern(&ta, value);
	t_pred(t, id.v == 1);
	t_pred(t, ta.cap == 16);
	t_pred(t, ta.len == 2);
	usize used = arena.offset;
	t_pred(t, type_intern(&ta, value).v == id.v);
	t_pred(t, ta.len == 2 && arena.offset == used);
}

static void interner_reserve_failure(Test* t){
	t->name = "interner-reserve-failure";
	u8 memory[16384];
	Arena arena = arena_from_buffer(memory, sizeof(memory));
	Type_Arena ta = type_test_fixture(&arena, 16);
	Type_Arena before = ta;
	Arena_Reg region = arena_region(&arena);
	// Allow the first replacement allocation but deny the second.
	uintptr current = (uintptr)arena.data + arena.offset;
	usize padding = mem_align_forward_ptr(current, alignof(Type)) - current;
	arena.capacity = arena.offset + padding + 32 * sizeof(Type);
	t_pred(t, !type_arena_reserve(&ta, 32));
	t_pred(t, ta.types == before.types && ta.next_hash == before.next_hash);
	t_pred(t, ta.cap == before.cap && ta.len == before.len);
	t_pred(t, arena.offset == region.offset);
	// Restoring usable memory must allow a subsequent reserve to succeed.
	arena.capacity = sizeof(memory);
	t_pred(t, type_arena_reserve(&ta, 32));
	t_pred(t, ta.cap == 32);
}

static void interner_table_growth(Test* t){
	t->name = "interner-table-growth";
	u8 memory[65536];
	Arena arena = arena_from_buffer(memory, sizeof(memory));
	Type_ID_By_Hash table;
	t_pred(t, type_id_hash_init(&table, 16, &arena));
	for(u32 i = 0; i < 12; i += 1) type_id_hash_insert(&table, i, (Type_ID){i + 1});
	usize used = arena.offset;
	type_id_hash_insert(&table, 0, (Type_ID){99});
	t_pred(t, type_id_hash_get(&table, 0)->v == 99);
	t_pred(t, table.slot_count == 16 && table.in_use == 12 && arena.offset == used);
	for(u32 i = 0; i < 128; i += 1) type_id_hash_insert(&table, i, (Type_ID){i + 1});
	for(u32 i = 0; i < 128; i += 1) t_pred(t, type_id_hash_get(&table, i)->v == i + 1);
	// Cluster keys at the end of the table to exercise wraparound while growing.
	for(u32 i = 0; i < 128; i += 1) type_id_hash_insert(&table, 511 + i * 512, (Type_ID){i + 1});
	for(u32 i = 0; i < 128; i += 1) t_pred(t, type_id_hash_get(&table, 511 + i * 512)->v == i + 1);
	t_pred(t, type_id_hash_get(&table, UINT32_MAX) == NULL);
	t_pred(t, type_id_hash_init(&table, 0, &arena));
	t_pred(t, type_id_hash_get(&table, 0) == NULL);
	type_id_hash_insert(&table, 0, (Type_ID){1});
	t_pred(t, type_id_hash_get(&table, 0)->v == 1);
}

static void interner_names(Test* t){
	t->name = "interner-names";
	u8 memory[16384];
	Arena arena = arena_from_buffer(memory, sizeof(memory));
	Type_Arena ta;
	type_arena_make(&ta, 1, &arena);
	t_pred(t, ta.len == 1 && ta.id_by_hash.in_use == 0);
	Type_ID child = type_intern(&ta, (Type){.kind = Type_Primitive, .primitive = Prim_Int});
	char name[] = "Meters";
	Type value = {.kind = Type_Distinct, .distinct = {.inner = child, .name = {.v = name, .len = 6}}};
	Type_ID id = type_intern(&ta, value);
	name[0] = 'X';
	t_pred(t, str_equal(type_arena_get(&ta, id)->distinct.name, strlit("Meters")));
	value.distinct.name = strlit("Meters");
	usize used = arena.offset;
	t_pred(t, type_intern(&ta, value).v == id.v);
	t_pred(t, arena.offset == used);
	value.distinct.name = (String){0};
	id = type_intern(&ta, value);
	t_pred(t, type_intern(&ta, value).v == id.v);
}

static void interner_roundtrip(Test* t){
	t->name = "interner-roundtrip";
	static u8 memory[1024 * 1024];
	Arena arena = arena_from_buffer(memory, sizeof(memory));
	Type_Arena ta = {0};
	ta = type_arena_make(&ta, 16, &arena);
	t_pred(t, ta.len == 1 && ta.cap == 16 && ta.arena == &arena);
	t_pred(t, type_arena_get(&ta, (Type_ID){0}) == NULL);
	Type integer = {.kind = Type_Primitive, .primitive = Prim_Int};
	Type_ID child = type_intern(&ta, integer);
	t_pred(t, child.v != 0);
	Type values[] = {
		integer,
		{.kind = Type_Pointer, .pointer.inner = child},
		{.kind = Type_Slice, .slice.inner = child},
		{.kind = Type_Array, .array = {.inner = child, .size = 8}},
		{.kind = Type_Distinct, .distinct = {.inner = child, .name = strlit("Meters")}},
	};
	Type_ID ids[5];
	for(u32 i = 0; i < 5; i += 1){
		ids[i] = type_intern(&ta, values[i]);
		t_pred(t, ids[i].v != 0);
		for(u32 j = 0; j < i; j += 1) t_pred(t, ids[i].v != ids[j].v);
	}
	for(i32 i = 0; i < 256; i += 1){
		Type a = {.kind = Type_Array, .array = {.inner = child, .size = i}};
		Type_ID id = type_intern(&ta, a);
		t_pred(t, type_intern(&ta, a).v == id.v);
	}
	for(u32 i = 0; i < 5; i += 1){
		t_pred(t, type_eq(*type_arena_get(&ta, ids[i]), values[i]));
		t_pred(t, type_intern(&ta, values[i]).v == ids[i].v);
	}
}

static const struct { String name; TestFunc* run; } interner_cases[] = {
	{strlit("interner-missing"), interner_missing},
	{strlit("interner-chain"), interner_chain},
	{strlit("interner-capacity"), interner_capacity},
	{strlit("interner-reserve-failure"), interner_reserve_failure},
	{strlit("interner-table-growth"), interner_table_growth},
	{strlit("interner-roundtrip"), interner_roundtrip},
	{strlit("interner-names"), interner_names},
};

void type_tests(Test* t){
	t->name = "types";

	Type int_a = {.primitive = Prim_Int, .kind = Type_Primitive};
	Type int_b = {.primitive = Prim_Int, .kind = Type_Primitive};
	Type real = {.primitive = Prim_Real, .kind = Type_Primitive};

	t_pred(t, type_eq(int_a, int_b));
	t_pred(t, type_hash(&int_a) == type_hash(&int_b));
	t_pred(t, !type_eq(int_a, real));
	t_pred(t, type_hash(&int_a) != type_hash(&real));

	Type int_pointer_a = {
		.pointer.inner = {1},
		.kind = Type_Pointer,
	};
	Type int_pointer_b = {
		.pointer.inner = {1},
		.kind = Type_Pointer,
	};
	Type int_slice = {
		.slice.inner = {1},
		.kind = Type_Slice,
	};

	t_pred(t, type_eq(int_pointer_a, int_pointer_b));
	t_pred(t, type_hash(&int_pointer_a) == type_hash(&int_pointer_b));
	t_pred(t, !type_eq(int_pointer_a, int_slice));
	t_pred(t, type_hash(&int_pointer_a) != type_hash(&int_slice));

	Type array_8_a = {
		.array = {.inner = {2}, .size = 8},
		.kind = Type_Array,
	};
	Type array_8_b = {
		.array = {.inner = {2}, .size = 8},
		.kind = Type_Array,
	};
	Type array_9 = {
		.array = {.inner = {2}, .size = 9},
		.kind = Type_Array,
	};

	t_pred(t, type_eq(array_8_a, array_8_b));
	t_pred(t, type_hash(&array_8_a) == type_hash(&array_8_b));
	t_pred(t, !type_eq(array_8_a, array_9));
	t_pred(t, type_hash(&array_8_a) != type_hash(&array_9));

	char meters_a[] = "Meters";
	char meters_b[] = "Meters";
	Type meters_type_a = {
		.distinct = {
			.name = {.v = meters_a, .len = sizeof(meters_a) - 1},
			.inner = {3},
		},
		.kind = Type_Distinct,
	};
	Type meters_type_b = {
		.distinct = {
			.name = {.v = meters_b, .len = sizeof(meters_b) - 1},
			.inner = {3},
		},
		.kind = Type_Distinct,
	};
	Type seconds_type = {
		.distinct = {
			.name = strlit("Seconds"),
			.inner = {3},
		},
		.kind = Type_Distinct,
	};

	t_pred(t, type_eq(meters_type_a, meters_type_b));
	t_pred(t, type_hash(&meters_type_a) == type_hash(&meters_type_b));
	t_pred(t, !type_eq(meters_type_a, seconds_type));
	t_pred(t, type_hash(&meters_type_a) != type_hash(&seconds_type));
	t_pred(t, type_hash(&meters_type_a) == type_hash(&meters_type_a));

	// Changing only the child ID must change type identity for each wrapper.
	Type originals[] = {int_pointer_a, int_slice, array_8_a, meters_type_a};
	Type changed[] = {int_pointer_a, int_slice, array_8_a, meters_type_a};
	changed[0].pointer.inner.v = 99;
	changed[1].slice.inner.v = 99;
	changed[2].array.inner.v = 99;
	changed[3].distinct.inner.v = 99;
	for(usize i = 0; i < sizeof(originals) / sizeof(originals[0]); i += 1){
		t_pred(t, !type_eq(originals[i], changed[i]));
	}
}
