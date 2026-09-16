use crate::arena::Arena;
use crate::base::{murmur3_hash32, Str};
use std::collections::HashMap;

crate::def_arena_handle!(TypeID);

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
#[repr(u32)]
pub enum PrimitiveType {
    Int = 1,
    Real,
    Bool,
    Rune,
    String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum Type {
    Primitive(PrimitiveType),
    Distinct { inner: TypeID, name: Str },
    Pointer { inner: TypeID },
    Array { inner: TypeID, size: i32 },
    Slice { inner: TypeID },
}

pub fn type_hash_mix_u32(current_hash: u32, data: u32) -> u32 {
    murmur3_hash32(&data.to_le_bytes(), current_hash)
}

impl Type {
    pub fn hash(&self) -> u32 {
        let kind = match self {
            Self::Primitive(_) => 1,
            Self::Distinct { .. } => 2,
            Self::Pointer { .. } => 3,
            Self::Array { .. } => 4,
            Self::Slice { .. } => 5,
        };
        let mut h = type_hash_mix_u32(0, kind);

        match self {
            Self::Primitive(primitive) => type_hash_mix_u32(h, *primitive as u32),
            Self::Pointer { inner } => type_hash_mix_u32(h, inner.0.get()),
            Self::Slice { inner } => type_hash_mix_u32(h, inner.0.get()),
            Self::Array { inner, size } => {
                type_hash_mix_u32(type_hash_mix_u32(h, *size as u32), inner.0.get())
            }
            Self::Distinct { inner, name } => {
                let len = name.len() as u64;
                h = type_hash_mix_u32(h, len as u32);
                h = type_hash_mix_u32(h, (len >> 32) as u32);
                h = murmur3_hash32(name.as_bytes(), h);
                type_hash_mix_u32(h, inner.0.get())
            }
        }
    }
}

/// Interned set of types.
///
/// The `next_hash` vector keeps a linked list of IDs for types with the same hash,
/// so lookup can compare the types and find the right one even when hashes collide.
/// In the example below, `D` collided with `C`, which collided with `A`.
/// Each ID is a one-based index into `types`; its collision link is stored at
/// `next_hash[id - 1]`. The separate `id_by_hash` map stores the head of each list.
///
/// | ID        | 1    | 2    | 3       | 4       | 5    | 6    |
/// |-----------|------|------|---------|---------|------|------|
/// | hash      | x    | y    | x       | x       | z    | w    |
/// | types     | A    | B    | C       | D       | E    | F    |
/// | next_hash | None | None | Some(1) | Some(3) | None | None |
///
/// | Hash | ID head | Chain     |
/// |------|---------|-----------|
/// | x    | 4       | 4 → 3 → 1 |
/// | y    | 2       | 2         |
/// | z    | 5       | 5         |
/// | w    | 6       | 6         |
#[derive(Debug, Default)]
pub struct TypeArena {
    pub types: Arena<Type, TypeID>,
    pub next_hash: Vec<Option<TypeID>>,
    pub id_by_hash: HashMap<u32, TypeID>,
}

impl TypeArena {
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            types: Arena::with_capacity(capacity),
            next_hash: Vec::with_capacity(capacity),
            id_by_hash: HashMap::with_capacity(capacity),
        }
    }

    pub fn get(&self, id: TypeID) -> Option<&Type> {
        self.types.get(id)
    }

    pub fn find(&self, hash: u32, ty: &Type) -> Option<TypeID> {
        let mut current = self.id_by_hash.get(&hash).copied();
        while let Some(id) = current {
            if &self.types[id] == ty {
                return Some(id);
            }
            current = self.next_hash[id.0.get() as usize - 1];
        }
        None
    }

    pub fn intern(&mut self, ty: Type) -> TypeID {
        let hash = ty.hash();
        if let Some(id) = self.find(hash, &ty) {
            return id;
        }
        let inner = match &ty {
            Type::Primitive(_) => None,

            Type::Distinct { inner, .. }
            | Type::Pointer { inner }
            | Type::Array { inner, .. }
            | Type::Slice { inner } => Some(*inner),
        };
        if let Some(id) = inner {
            assert!(self.types.get(id).is_some(), "invalid child type ID");
        }
        let id = self.types.alloc(ty);
        self.next_hash.push(self.id_by_hash.insert(hash, id));
        id
    }
}

#[cfg(test)]
#[path = "types_test.rs"]
mod types_test;
