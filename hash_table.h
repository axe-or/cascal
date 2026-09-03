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
Hash_Table_Slot* ht_find(Hash_Table* tbl, u32 hash, String key){
	if(tbl->slot_count == 0){ return NULL; }

	u32 mask = tbl->slot_count - 1;
	u32 pos = hash & mask;

	for(u32 probe = 0; probe < tbl->slot_count; probe += 1){
		Hash_Table_Slot* slot = &tbl->slots[pos];
		if(slot->hash == 0){
			return NULL;
		}

		if((hash == slot->hash) && str_equal(key, slot->key)){
			return slot;
		}

		pos = (pos + 1) & mask;
	}

	return NULL;
}

static inline
bool ht_needs_growth(Hash_Table const* tbl){
	u32 threshold = (tbl->slot_count * 80) / 100;
	return (tbl->in_use >= tbl->slot_count) || (tbl->in_use >= threshold);
}

static inline
void ht_put(Hash_Table* tbl, String key, f32 value){
	if(ht_needs_growth(tbl)){ // TODO: Defer rezising to after we add new key, here just cehck minimum vacancy
		panic("todo: growth");
	}

	u32 mask = tbl->slot_count - 1;
	u32 hash = ht_key_hash(key);
	u32 first_pos = hash & mask;

	u32 pos = first_pos;
	do {
		Hash_Table_Slot* slot = &tbl->slots[pos];

		if(slot->hash == 0){
			// Vacant slot, place key
			slot->key = key;
			slot->value = value;
			slot->hash = hash;
			tbl->in_use += 1;
			return;
		}

		if((hash == slot->hash) && str_equal(key, slot->key)){
			slot->value = value;
			return;
		}

		pos = (pos + 1) & mask;

	} while(pos != first_pos);
}


static inline
void ht_remove(Hash_Table* tbl, String key){
}
