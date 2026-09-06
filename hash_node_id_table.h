#pragma once
/* Auto generated file. DO NOT EDIT. */

#include "lang.h"
#include "base.h"

typedef struct {
	u32 key;
	Type_ID value;
	u32 hash; // 0 when vacant
} Type_Interning_Table_Slot;

typedef struct {
	Type_Interning_Table_Slot* slots;
	u32 slot_count;
	u32 in_use;
	Arena* arena;
} Type_Interning_Table;

#define ht_valid_cap(n) (((n) & ((n) - 1)) == 0)

#define ht_needs_growth(tbl) \
	((tbl)->in_use >= (tbl)->slot_count) || ((tbl)->in_use >= (((tbl)->slot_count * 80) / 100))

static inline
u32 type_intern_key_hash(u32 key){
	// IMPORTANT: Hash 0 is reserved for empty slot
	u32 h = (u32)(key);
	return h ? h : 1;
}

static inline
void type_intern_init(Type_Interning_Table* tbl, u32 cap, Arena* arena){
	mem_zero(tbl, sizeof(*tbl));
	tbl->arena = arena;
	
	if(cap){
		tbl->slots = arena_make(tbl->arena, Type_Interning_Table_Slot, cap);
		ensure(tbl->slots, "allocation error");
		tbl->slot_count = cap;
	}
}

static inline
u32 type_intern_probe_distance(Type_Interning_Table const* tbl, u32 pos, u32 hash){
	u32 mask = tbl->slot_count - 1;
	u32 home = hash & mask;
	return (pos - home) & mask;
}

static inline
Type_Interning_Table_Slot* type_intern_find(Type_Interning_Table const* tbl, u32 hash, u32 key){
	if(tbl->slot_count == 0){
		return NULL;
	}

	u32 mask = tbl->slot_count - 1;
	u32 pos = hash & mask;
	u32 probe_dist = 0;

	for(;;){
		Type_Interning_Table_Slot* slot = &tbl->slots[pos];

		if(slot->hash == 0){
			return NULL;
		}

		u32 slot_dist = type_intern_probe_distance(tbl, pos, slot->hash);

		// Resident has travelled less than we have, so our key cannot appear after this point.
		if(slot_dist < probe_dist){
			return NULL;
		}

		if((slot->hash == hash) && u32_eq(slot->key, key)){
			return slot;
		}

		pos = (pos + 1) & mask;
		probe_dist += 1;
	}
}

static inline
Type_ID* type_intern_get(Type_Interning_Table const* tbl, u32 key){
	u32 hash = type_intern_key_hash(key);
	Type_Interning_Table_Slot* slot = type_intern_find(tbl, hash, key);
	return &slot->value;
}

static inline
void type_intern_insert(Type_Interning_Table* tbl, u32 key, Type_ID value){
	if(ht_needs_growth(tbl)){
		panic("todo: growth");
	}

	Type_Interning_Table_Slot incoming = {
		.key   = key,
		.value = value,
		.hash  = type_intern_key_hash(key),
	};

	u32 mask = tbl->slot_count - 1;
	u32 pos = incoming.hash & mask;
	u32 incoming_dist = 0;

	for(;;){
		Type_Interning_Table_Slot* slot = &tbl->slots[pos];

		if(slot->hash == 0){
			*slot = incoming;
			tbl->in_use += 1;
			return;
		}

		if((slot->hash == incoming.hash) && u32_eq(slot->key, incoming.key)){
			slot->value = incoming.value;
			return;
		}

		u32 slot_dist = type_intern_probe_distance(tbl, pos, slot->hash);

		if(slot_dist < incoming_dist){
			Type_Interning_Table_Slot tmp = *slot;
			*slot = incoming;
			incoming = tmp;

			incoming_dist = slot_dist;
		}

		pos = (pos + 1) & mask;
		incoming_dist += 1;
	}
}

static inline
bool type_intern_remove(Type_Interning_Table* tbl, u32 key){
	u32 mask = tbl->slot_count - 1;
	u32 hash = type_intern_key_hash(key);

	Type_Interning_Table_Slot* found = type_intern_find(tbl, hash, key);
	if(found == NULL){
		return false;
	}

	u32 hole_pos = (u32)(found - tbl->slots);
	u32 next_pos = (hole_pos + 1) & mask;

	for(;;){
		Type_Interning_Table_Slot* slot = &tbl->slots[next_pos];
		u32 dist = type_intern_probe_distance(tbl, next_pos, slot->hash);

		if(slot->hash == 0 || dist == 0){
			break;
		}

		tbl->slots[hole_pos] = *slot;

		hole_pos = next_pos;
		next_pos = (next_pos + 1) & mask;
	}

	mem_zero(&tbl->slots[hole_pos], sizeof(tbl->slots[hole_pos]));
	tbl->in_use -= 1;

	return true;
}

#undef ht_needs_growth
#undef ht_valid_cap

