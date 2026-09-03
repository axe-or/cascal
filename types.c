#include "base.h"
#include "lang.h"

static inline
u32 type_hash_mix_bytes(u32 current_hash, u8 const* data, usize data_size){
    panic("todo");
}

static inline
u32 type_hash_mix_u32(u32 current_hash, u32 data){
    panic("todo");
}

static inline
u32 type_hash_(u32 h, Type const* t){
    h = type_hash_mix_u32(h, (u32)t->kind);

    switch((enum Type_Kind)t->kind){
    case Type_Primitive:
        return type_hash_mix_u32(h, (u32)t->primitive);

    case Type_Pointer:
        return type_hash_(h, t->pointer.inner);

    case Type_Slice:
        return type_hash_(h, t->slice.inner);

    case Type_Distinct:
        h = type_hash_mix_bytes(h, (u8 const*)t->distinct.name.v, t->distinct.name.len);
        return type_hash_(h, t->distinct.inner);

    case Type_Array:
        h = type_hash_mix_u32(h, t->array.size);
        return type_hash_(h, t->array.inner);

    default:
        panic("invalid type kind");
    }
}

static inline
u32 type_hash(Type const* t){
    // TODO: Better seed value
    return type_hash_(0, t);
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