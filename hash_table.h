#pragma once
#include "base.h"

static inline
u32 hash32(void const* data, usize len, u32 seed){
	// Murmur3 32 bit
	u8 const* bytes = (u8 const*)data;
	u32 hash = seed;
	usize block_count = len / 4;

	ensure(bytes != NULL || len == 0, "cannot hash null data");

	for(usize i = 0; i < block_count; i += 1){
		usize offset = i * 4;
		u32 block = ((u32)bytes[offset + 0] << 0)
		          | ((u32)bytes[offset + 1] << 8)
		          | ((u32)bytes[offset + 2] << 16)
		          | ((u32)bytes[offset + 3] << 24);

		block *= 0xcc9e2d51u;
		block = (block << 15) | (block >> 17);
		block *= 0x1b873593u;

		hash ^= block;
		hash = (hash << 13) | (hash >> 19);
		hash = hash * 5u + 0xe6546b64u;
	}

	u32 tail = 0;
	usize tail_offset = block_count * 4;
	switch(len & 3){
	case 3: tail ^= (u32)bytes[tail_offset + 2] << 16; /* fallthrough */
	case 2: tail ^= (u32)bytes[tail_offset + 1] << 8;  /* fallthrough */
	case 1:
		tail ^= (u32)bytes[tail_offset];
		tail *= 0xcc9e2d51u;
		tail = (tail << 15) | (tail >> 17);
		tail *= 0x1b873593u;
		hash ^= tail;
	}

	hash ^= (u32)len;
	hash ^= hash >> 16;
	hash *= 0x85ebca6bu;
	hash ^= hash >> 13;
	hash *= 0xc2b2ae35u;
	hash ^= hash >> 16;
	return hash;
}

typedef struct {
	String key;
	f32 value;
	u32 hash; // 0 when vacant
} Hash_Table_Slot;

typedef struct {
	Hash_Table_Slot* slots;
	u32 slot_count;
	u32 in_use;
	Arena* arena;
} Hash_Table;

static inline
bool ht_valid_cap(u32 n){
	return (n & (n - 1)) == 0;
}

static inline
u32 ht_key_hash(String k){
	// IMPORTANT: Hash 0 is reserved for empty slot
	u32 h = hash32(k.v, k.len, 0);
	return h ? h : 1;
}

static inline
void ht_init(Hash_Table* tbl, u32 cap, Arena* arena){
	mem_zero(tbl, sizeof(*tbl));
	tbl->arena = arena;
	
	if(cap){
		tbl->slots = arena_make(tbl->arena, Hash_Table_Slot, cap);
		ensure(tbl->slots, "allocation error");
		tbl->slot_count = cap;
	}
}

static inline
u32 ht_probe_distance(Hash_Table const* tbl, u32 pos, u32 hash){
	u32 mask = tbl->slot_count - 1;
	u32 home = hash & mask;
	return (pos - home) & mask;
}

static inline
Hash_Table_Slot* ht_find(Hash_Table const* tbl, u32 hash, String key){
	if(tbl->slot_count == 0){
		return NULL;
	}

	u32 mask = tbl->slot_count - 1;
	u32 pos = hash & mask;
	u32 probe_dist = 0;

	for(;;){
		Hash_Table_Slot* slot = &tbl->slots[pos];

		if(slot->hash == 0){
			return NULL;
		}

		u32 slot_dist = ht_probe_distance(tbl, pos, slot->hash);

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
f32* ht_get(Hash_Table const* tbl, String key){
	u32 hash = ht_key_hash(key);
	Hash_Table_Slot* slot = ht_find(tbl, hash, key);
	return &slot->value;
}

static inline
bool ht_needs_growth(Hash_Table const* tbl){
	u32 threshold = (tbl->slot_count * 80) / 100;
	return (tbl->in_use >= tbl->slot_count) || (tbl->in_use >= threshold);
}

static inline
void ht_put(Hash_Table* tbl, String key, f32 value){
	if(ht_needs_growth(tbl)){
		panic("todo: growth");
	}

	u32 mask = tbl->slot_count - 1;

	Hash_Table_Slot incoming = {
		.key   = key,
		.value = value,
		.hash  = ht_key_hash(key),
	};

	u32 pos = incoming.hash & mask;
	u32 incoming_dist = 0;

	for(;;){
		Hash_Table_Slot* slot = &tbl->slots[pos];

		if(slot->hash == 0){
			*slot = incoming;
			tbl->in_use += 1;
			return;
		}

		if((slot->hash == incoming.hash) && str_equal(slot->key, incoming.key)){
			slot->value = incoming.value;
			return;
		}

		u32 slot_dist = ht_probe_distance(tbl, pos, slot->hash);

		if(slot_dist < incoming_dist){
			Hash_Table_Slot tmp = *slot;
			*slot = incoming;
			incoming = tmp;

			incoming_dist = slot_dist;
		}

		pos = (pos + 1) & mask;
		incoming_dist += 1;
	}
}

static inline
bool ht_remove(Hash_Table* tbl, String key){
	u32 hash = ht_key_hash(key);
	Hash_Table_Slot* found = ht_find(tbl, hash, key);
	if(found == NULL){
		return false;
	}

	u32 mask = tbl->slot_count - 1;
	u32 hole = (u32)(found - tbl->slots);
	u32 next = (hole + 1) & mask;

	for(;;){
		Hash_Table_Slot* slot = &tbl->slots[next];

		// Actual end of cluster.
		if(slot->hash == 0){
			break;
		}

		u32 dist = ht_probe_distance(tbl, next, slot->hash);

		// This entry is already at home. It doesn't depend on the
		// hole, and neither does the following Robin Hood run.
		if(dist == 0){
			break;
		}

		// Collapse the displaced run backward by one.
		tbl->slots[hole] = *slot;

		hole = next;
		next = (next + 1) & mask;
	}

	mem_zero(&tbl->slots[hole], sizeof(tbl->slots[hole]));
	tbl->in_use -= 1;
	return true;
}

