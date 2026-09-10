#include "base.h"
#include "lang.h"

static inline
Type_ID type_arena_push_type(Type_Arena* ta, Type t, Type_ID next_hash);

static inline
u32 type_hash_mix_u32(u32 current_hash, u32 data){
    u8 const bytes[] = {
        (u8)(data >> 0),
        (u8)(data >> 8),
        (u8)(data >> 16),
        (u8)(data >> 24),
    };
    return murmur3_hash32(bytes, sizeof(bytes), current_hash);
}

u32 type_hash(Type const* t){
    ensure(t != NULL, "cannot hash a null type");
    u32 h = type_hash_mix_u32(0, (u32)t->kind);

    switch((enum Type_Kind)t->kind){
    case Type_Primitive:
        return type_hash_mix_u32(h, (u32)t->primitive);

    case Type_Pointer:
        return type_hash_mix_u32(h, t->pointer.inner.v);

    case Type_Slice:
        return type_hash_mix_u32(h, t->slice.inner.v);

    case Type_Distinct:
        ensure(t->distinct.name.len >= 0, "invalid distinct type name");
        u64 name_len = (u64)t->distinct.name.len;
        h = type_hash_mix_u32(h, (u32)name_len);
        h = type_hash_mix_u32(h, (u32)(name_len >> 32));
        h = murmur3_hash32(t->distinct.name.v, (usize)t->distinct.name.len, h);
        return type_hash_mix_u32(h, t->distinct.inner.v);

    case Type_Array:
        h = type_hash_mix_u32(h, t->array.size);
        return type_hash_mix_u32(h, t->array.inner.v);

    default:
        panic("invalid type kind");
    }
}

bool type_eq(Type a, Type b){
    if(a.kind != b.kind){
        return false;
    }

    switch((enum Type_Kind)a.kind){
    case Type_Primitive:
        return a.primitive == b.primitive;

    case Type_Pointer:
        return a.pointer.inner.v == b.pointer.inner.v;

    case Type_Slice:
        return a.slice.inner.v == b.slice.inner.v;

    case Type_Distinct:
        return str_equal(a.distinct.name, b.distinct.name)
            && a.distinct.inner.v == b.distinct.inner.v;

    case Type_Array:
        return (a.array.size == b.array.size)
            && a.array.inner.v == b.array.inner.v;

    default:
        panic("invalid type kind");
    }
}

static inline
bool type_arena_valid_capacity(usize n){
	return ((n & (n - 1)) == 0) && n != 0;
}

Type_Arena type_arena_make(Type_Arena* ta, usize cap, Arena* arena){
	ensure(type_arena_valid_capacity(cap), "capacity must be a power of 2");
	ensure(cap <= UINT32_MAX && cap <= SIZE_MAX / sizeof(Type), "capacity too large");

	Type* types = arena_make(arena, Type, cap);
	ensure(types, "allocation error");

	Type_ID* next_hash = arena_make(arena, Type_ID, cap);
	ensure(next_hash, "allocation error");

	Type_Arena res = {
		.types = types,
		.next_hash = next_hash,
		.cap = cap,
		.len = 1, // Slot zero is a sentinel, never an interned type.

		.arena = arena,
	};
	ensure(type_id_hash_init(&res.id_by_hash, (u32)cap, arena), "failed to initialize type interning table");
	*ta = res;
	return res;
}

static inline
bool type_arena_reserve(Type_Arena* ta, usize new_cap){
    if(ta->cap >= new_cap){
        return true;
    }
    ensure(type_arena_valid_capacity(new_cap), "invalid capacity");
    ensure(new_cap <= UINT32_MAX && new_cap <= SIZE_MAX / sizeof(Type), "capacity too large");

    Arena_Reg restore = arena_region(ta->arena);

    Type* new_types = arena_realloc(ta->arena, ta->types,
        ta->cap * sizeof(*new_types),
        new_cap * sizeof(*new_types),
        alignof(typeof(*new_types))
    );
    if(!new_types){
        goto fail;
    }

    Type_ID* new_next_hash = arena_realloc(ta->arena, ta->next_hash,
        ta->cap * sizeof(*new_next_hash),
        new_cap * sizeof(*new_next_hash),
        alignof(typeof(*new_next_hash))
    );
    if(!new_next_hash){
        goto fail;
    }

    ta->cap = new_cap;
    ta->types = new_types;
    ta->next_hash = new_next_hash;
    return true;

fail:
    arena_region_restore(restore);
    return false;
}

Type const* type_arena_get(Type_Arena const* ta, Type_ID id){
    if(!id.v || id.v >= ta->len) {
        return NULL;
    }
    return &ta->types[id.v];
}

static inline
Type_ID type_arena_get_first_id(Type_Arena const* ta, u32 hash){
	Type_ID* id = type_id_hash_get(&ta->id_by_hash, hash);
	if(!id){
		return (Type_ID){0};
	}
	return *id;
}

static inline
Type_ID type_arena_find(Type_Arena const* ta, u32 hash, Type t){
	for (
		Type_ID cur = type_arena_get_first_id(ta, hash);
		cur.v != 0;
		cur = ta->next_hash[cur.v]
	){
		Type const* candidate = type_arena_get(ta, cur);
		if(type_eq(*candidate, t)){
			return cur;
		}
	}

	return (Type_ID){0};
}

static inline
Type_ID type_arena_push_type(Type_Arena* ta, Type t, Type_ID next_hash_head){
	u32 hash = type_hash(&t);
	ensure_dbg(type_arena_find(ta, hash, t).v == 0, "Type already present in arena");

    if(ta->len >= ta->cap){
        ensure(ta->cap <= UINT32_MAX / 2 && ta->cap <= SIZE_MAX / 2, "type arena exhausted");
        usize new_cap = max(16, ta->cap * 2);
        bool ok = type_arena_reserve(ta, new_cap);
        ensure(ok, "allocation error");
    }

    if(t.kind == Type_Distinct && t.distinct.name.len > 0){
        char* name = arena_make(ta->arena, char, (usize)t.distinct.name.len);
        ensure(name, "allocation error");
        mem_copy(name, t.distinct.name.v, (usize)t.distinct.name.len);
        t.distinct.name.v = name;
    }

    Type_ID new_id = {(u32)ta->len};
    ta->types[ta->len] = t;
    ta->len += 1;

   	ta->next_hash[new_id.v] = next_hash_head;           // new.next = head
    type_id_hash_insert(&ta->id_by_hash, hash, new_id); // head = new

    return new_id;
}

Type_ID type_intern(Type_Arena* ta, Type t){
	u32 h = type_hash(&t);
	Type_ID id = type_arena_find(ta, h, t);
	if(id.v){
		return id;
	}

	Type_ID first =  type_arena_get_first_id(ta, h);
	return type_arena_push_type(ta, t, first);
}

// Type type_from_node(Node* node, Arena* arena){
//     ensure(node->type == Node_ParserType, "not a parser type");

//     switch(node->value.parser_type.kind){
//     case ParserType_Named:

//     case ParserType_Slice:

//     case ParserType_Array:

//     case ParserType_Pointer:

//     default: panic("unknown parser type")
//     }
// }
