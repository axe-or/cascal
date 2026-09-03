#include "base.h"
#include "lang.h"

static inline
u32 type_hash_mix_bytes(u32 current_hash, u8 const* data, usize data_size){
    for(usize i = 0; i < data_size; i += 1){
        current_hash ^= data[i];
        current_hash *= 16777619u;
    }
    return current_hash;
}

static inline
u32 type_hash_mix_u32(u32 current_hash, u32 data){
    u8 const bytes[] = {
        (u8)(data >> 0),
        (u8)(data >> 8),
        (u8)(data >> 16),
        (u8)(data >> 24),
    };
    return type_hash_mix_bytes(current_hash, bytes, sizeof(bytes));
}

static inline
u32 type_hash_(u32 h, Type const* t){
    ensure(t != NULL, "cannot hash a null type");
    h = type_hash_mix_u32(h, (u32)t->kind);

    switch((enum Type_Kind)t->kind){
    case Type_Primitive:
        return type_hash_mix_u32(h, (u32)t->primitive);

    case Type_Pointer:
        return type_hash_(h, t->pointer.inner);

    case Type_Slice:
        return type_hash_(h, t->slice.inner);

    case Type_Distinct:
        ensure(t->distinct.name.len >= 0, "invalid distinct type name");
        u64 name_len = (u64)t->distinct.name.len;
        h = type_hash_mix_u32(h, (u32)name_len);
        h = type_hash_mix_u32(h, (u32)(name_len >> 32));
        h = type_hash_mix_bytes(h, (u8 const*)t->distinct.name.v, (usize)t->distinct.name.len);
        return type_hash_(h, t->distinct.inner);

    case Type_Array:
        h = type_hash_mix_u32(h, t->array.size);
        return type_hash_(h, t->array.inner);

    default:
        panic("invalid type kind");
    }
}

u32 type_hash(Type const* t){
    u32 hash = type_hash_(2166136261u, t);

    // murmur3 final avalanche
    hash ^= hash >> 16;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 13;
    hash *= 0xc2b2ae35u;
    hash ^= hash >> 16;
    return hash;
}

bool type_eq(Type const* a, Type const* b){
    if(a->kind != b->kind){
        return false;
    }

    switch((enum Type_Kind)a->kind){
    case Type_Primitive:
        return a->primitive == b->primitive;

    case Type_Pointer:
        return type_eq(a->pointer.inner, b->pointer.inner);

    case Type_Slice:
        return type_eq(a->slice.inner, b->slice.inner);

    case Type_Distinct:
        return str_equal(a->distinct.name, b->distinct.name)
            && type_eq(a->distinct.inner, b->distinct.inner);

    case Type_Array:
        return (a->array.size == b->array.size)
            && type_eq(a->array.inner, b->array.inner);

    default:
        panic("invalid type kind");
    }
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
