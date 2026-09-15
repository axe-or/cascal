use crate::base::{murmur3_hash32, Arena};
use std::collections::HashMap;

crate::arena_id!(TypeID);

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(u32)]
pub enum PrimitiveType { Int = 1, Real, Bool, Rune, String }

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Type {
    Primitive(PrimitiveType),
    Distinct { inner: TypeID, name: String },
    Pointer { inner: TypeID },
    Array { inner: TypeID, size: i32 },
    Slice { inner: TypeID },
}

pub fn type_eq(a: &Type, b: &Type) -> bool { a == b }
pub fn type_hash_mix_u32(current_hash: u32, data: u32) -> u32 {
    murmur3_hash32(&data.to_le_bytes(), current_hash)
}
pub fn type_hash(ty: &Type) -> u32 {
    use Type::*;
    let kind = match ty { Primitive(_) => 1, Distinct { .. } => 2, Pointer { .. } => 3, Array { .. } => 4, Slice { .. } => 5 };
    let mut h = type_hash_mix_u32(0, kind);
    match ty {
        Primitive(primitive) => type_hash_mix_u32(h, *primitive as u32),
        Pointer { inner } | Slice { inner } => type_hash_mix_u32(h, inner.0.get()),
        Array { inner, size } => type_hash_mix_u32(type_hash_mix_u32(h, *size as u32), inner.0.get()),
        Distinct { inner, name } => {
            let len = name.len() as u64;
            h = type_hash_mix_u32(h, len as u32);
            h = type_hash_mix_u32(h, (len >> 32) as u32);
            h = murmur3_hash32(name.as_bytes(), h);
            type_hash_mix_u32(h, inner.0.get())
        }
    }
}

/// Interned types and an out-of-band hash collision chain, as in the C implementation.
/// Child IDs must belong to this arena.
#[derive(Debug, Default)]
pub struct TypeArena {
    pub types: Arena<Type, TypeID>,
    pub next_hash: Vec<Option<TypeID>>,
    pub id_by_hash: HashMap<u32, TypeID>,
}
pub fn type_arena_make(capacity: usize) -> TypeArena {
    TypeArena {
        types: Arena::with_capacity(capacity), next_hash: Vec::with_capacity(capacity),
        id_by_hash: HashMap::with_capacity(capacity),
    }
}
pub fn type_arena_get(arena: &TypeArena, id: TypeID) -> Option<&Type> { arena.types.get(id) }
pub fn type_arena_find(arena: &TypeArena, hash: u32, ty: &Type) -> Option<TypeID> {
    let mut current = arena.id_by_hash.get(&hash).copied();
    while let Some(id) = current {
        if type_eq(&arena.types[id], ty) { return Some(id); }
        current = arena.next_hash[id.0.get() as usize - 1];
    }
    None
}
pub fn type_intern(arena: &mut TypeArena, ty: Type) -> TypeID {
    let hash = type_hash(&ty);
    if let Some(id) = type_arena_find(arena, hash, &ty) { return id; }
    let inner = match &ty {
        Type::Primitive(_) => None,
        Type::Distinct { inner, .. } | Type::Pointer { inner } | Type::Array { inner, .. } | Type::Slice { inner } => Some(*inner),
    };
    if let Some(id) = inner { assert!(arena.types.get(id).is_some(), "invalid child type ID"); }
    let id = arena.types.alloc(ty);
    arena.next_hash.push(arena.id_by_hash.insert(hash, id));
    id
}

#[cfg(test)]
#[path = "types_test.rs"]
mod types_test;
