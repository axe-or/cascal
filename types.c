#include "base.h"
#include "lang.h"

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

bool type_eq(Type const* a, Type const* b){
    if(a->kind != b->kind){
        return false;
    }

    switch((enum Type_Kind)a->kind){
    case Type_Primitive:
        return a->primitive == b->primitive;

    case Type_Pointer:
        return a->pointer.inner.v == b->pointer.inner.v;

    case Type_Slice:
        return a->slice.inner.v == b->slice.inner.v;

    case Type_Distinct:
        return str_equal(a->distinct.name, b->distinct.name)
            && a->distinct.inner.v == b->distinct.inner.v;

    case Type_Array:
        return (a->array.size == b->array.size)
            && a->array.inner.v == b->array.inner.v;

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


	Type* types = arena_make(arena, Type, cap);
	ensure(types, "allocation error");

	Type_ID* next_hash = arena_make(arena, Type_ID, cap);
	ensure(next_hash, "allocation error");

	Type_Arena res = {
		.types = types,
		.next_hash = next_hash,
		.cap = cap,
		.len = 0,

		.arena = arena,
	};

	return res;
}

// Type_ID type_intern(Type_Arena* ta, Type* t){
// 	u32 hash = type_hash(t);
// }

// static inline
// Type_ID type_arena_push_type(Type_Arena* ta){
// }

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
