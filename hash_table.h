#pragma once
/* Auto generated file. DO NOT EDIT. */

#include "base.h"

typedef struct {
	String key;
	f32 value;
	u32 hash; // 0 when vacant
} Earnings_Slot;

typedef struct {
	Earnings_Slot* slots;
	u32 slot_count;
	u32 in_use;
	Arena* arena;
} Earnings;

#define ht_valid_cap(n) (((n) & ((n) - 1)) == 0)

#define ht_needs_growth(tbl) \
	((tbl)->in_use >= (tbl)->slot_count) || ((tbl)->in_use >= (((tbl)->slot_count * 80) / 100))

static inline
u32 earnings_key_hash(String key){
	// IMPORTANT: Hash 0 is reserved for empty slot
	u32 h = str_hash(key);
	return h ? h : 1;
}

static inline
void earnings_init(Earnings* tbl, u32 cap, Arena* arena){
	mem_zero(tbl, sizeof(*tbl));
	tbl->arena = arena;
	
	if(cap){
		tbl->slots = arena_make(tbl->arena, Earnings_Slot, cap);
		ensure(tbl->slots, "allocation error");
		tbl->slot_count = cap;
	}
}

static inline
u32 earnings_probe_distance(Earnings const* tbl, u32 pos, u32 hash){
	u32 mask = tbl->slot_count - 1;
	u32 home = hash & mask;
	return (pos - home) & mask;
}

static inline
Earnings_Slot* earnings_find(Earnings const* tbl, u32 hash, String key){
	if(tbl->slot_count == 0){
		return NULL;
	}

	u32 mask = tbl->slot_count - 1;
	u32 pos = hash & mask;
	u32 probe_dist = 0;

	for(;;){
		Earnings_Slot* slot = &tbl->slots[pos];

		if(slot->hash == 0){
			return NULL;
		}

		u32 slot_dist = earnings_probe_distance(tbl, pos, slot->hash);

		// Resident has travelled less than we have, so our key cannot appear after this point.
		if(slot_dist < probe_dist){
			return NULL;
		}

		if((slot->hash == hash) && str_equal(slot->key, key)){
			return slot;
		}

		pos = (pos + 1) & mask;
		probe_dist += 1;
	}
}

static inline
f32* earnings_get(Earnings const* tbl, String key){
	u32 hash = earnings_key_hash(key);
	Earnings_Slot* slot = earnings_find(tbl, hash, key);
	return &slot->value;
}

static inline
void earnings_insert(Earnings* tbl, String key, f32 value){
	if(ht_needs_growth(tbl)){
		panic("todo: growth");
	}

	Earnings_Slot incoming = {
		.key   = key,
		.value = value,
		.hash  = earnings_key_hash(key),
	};

	u32 mask = tbl->slot_count - 1;
	u32 pos = incoming.hash & mask;
	u32 incoming_dist = 0;

	for(;;){
		Earnings_Slot* slot = &tbl->slots[pos];

		if(slot->hash == 0){
			*slot = incoming;
			tbl->in_use += 1;
			return;
		}

		if((slot->hash == incoming.hash) && str_equal(slot->key, incoming.key)){
			slot->value = incoming.value;
			return;
		}

		u32 slot_dist = earnings_probe_distance(tbl, pos, slot->hash);

		if(slot_dist < incoming_dist){
			Earnings_Slot tmp = *slot;
			*slot = incoming;
			incoming = tmp;

			incoming_dist = slot_dist;
		}

		pos = (pos + 1) & mask;
		incoming_dist += 1;
	}
}

static inline
bool earnings_remove(Earnings* tbl, String key){
	u32 mask = tbl->slot_count - 1;
	u32 hash = earnings_key_hash(key);

	Earnings_Slot* found = earnings_find(tbl, hash, key);
	if(found == NULL){
		return false;
	}

	u32 hole_pos = (u32)(found - tbl->slots);
	u32 next_pos = (hole_pos + 1) & mask;

	for(;;){
		Earnings_Slot* slot = &tbl->slots[next_pos];
		u32 dist = earnings_probe_distance(tbl, next_pos, slot->hash);

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

