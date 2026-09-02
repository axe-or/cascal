#include "base.h"
#include "lang.h"

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
