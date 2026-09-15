use std::num::NonZeroU32;
use std::ops::{Index, IndexMut};

/// IDs are local to their arena and remain valid until it is dropped.
pub trait ArenaID: Copy {
    fn from_raw(raw: NonZeroU32) -> Self;
    fn raw(self) -> NonZeroU32;
}

#[macro_export]
macro_rules! arena_id {
    ($name:ident) => {
        #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
        #[repr(transparent)]
        pub struct $name(pub std::num::NonZeroU32);
        impl $crate::base::ArenaID for $name {
            fn from_raw(raw: std::num::NonZeroU32) -> Self { Self(raw) }
            fn raw(self) -> std::num::NonZeroU32 { self.0 }
        }
    };
}

/// Append-only typed arena. Moving storage never invalidates an ID.
/// Rust drops all values together with the arena; no individual removal is exposed.
#[derive(Debug)]
pub struct Arena<T, ID> {
    pub values: Vec<T>,
    pub marker: std::marker::PhantomData<ID>,
}

impl<T, ID: ArenaID> Default for Arena<T, ID> {
    fn default() -> Self { Self::with_capacity(0) }
}

impl<T, ID: ArenaID> Arena<T, ID> {
    pub fn with_capacity(capacity: usize) -> Self {
        Self { values: Vec::with_capacity(capacity), marker: std::marker::PhantomData }
    }

    pub fn alloc(&mut self, value: T) -> ID {
        let raw = u32::try_from(self.values.len()).ok()
            .and_then(|n| n.checked_add(1)).and_then(NonZeroU32::new)
            .expect("arena exhausted");
        self.values.push(value);
        ID::from_raw(raw)
    }

    pub fn get(&self, id: ID) -> Option<&T> { self.values.get(id.raw().get() as usize - 1) }
    pub fn get_mut(&mut self, id: ID) -> Option<&mut T> { self.values.get_mut(id.raw().get() as usize - 1) }
    pub fn len(&self) -> usize { self.values.len() }
    pub fn is_empty(&self) -> bool { self.values.is_empty() }
}

impl<T, ID: ArenaID> Index<ID> for Arena<T, ID> {
    type Output = T;
    fn index(&self, id: ID) -> &T { self.get(id).expect("invalid arena ID") }
}
impl<T, ID: ArenaID> IndexMut<ID> for Arena<T, ID> {
    fn index_mut(&mut self, id: ID) -> &mut T { self.get_mut(id).expect("invalid arena ID") }
}

pub fn murmur3_hash32(data: &[u8], seed: u32) -> u32 {
    let mut hash = seed;
    let mut chunks = data.chunks_exact(4);
    for bytes in &mut chunks {
        let block = u32::from_le_bytes(bytes.try_into().unwrap())
            .wrapping_mul(0xcc9e2d51).rotate_left(15).wrapping_mul(0x1b873593);
        hash ^= block;
        hash = hash.rotate_left(13).wrapping_mul(5).wrapping_add(0xe6546b64);
    }
    let mut tail = 0;
    for (i, byte) in chunks.remainder().iter().enumerate() { tail |= (*byte as u32) << (i * 8); }
    if !chunks.remainder().is_empty() {
        hash ^= tail.wrapping_mul(0xcc9e2d51).rotate_left(15).wrapping_mul(0x1b873593);
    }
    hash ^= data.len() as u32;
    hash ^= hash >> 16;
    hash = hash.wrapping_mul(0x85ebca6b);
    hash ^= hash >> 13;
    hash = hash.wrapping_mul(0xc2b2ae35);
    hash ^ (hash >> 16)
}

pub fn str_hash(s: &str) -> u32 { murmur3_hash32(s.as_bytes(), 0) }

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RuneDecoded { pub codepoint: char, pub size: usize }

pub fn rune_decode(bytes: &[u8]) -> RuneDecoded {
    if bytes.is_empty() { return RuneDecoded { codepoint: '\0', size: 0 }; }
    let size = match bytes[0] { 0..=0x7f => 1, 0xc2..=0xdf => 2, 0xe0..=0xef => 3, 0xf0..=0xf4 => 4, _ => 1 };
    if let Some(Ok(s)) = bytes.get(..size).map(std::str::from_utf8) {
        return RuneDecoded { codepoint: s.chars().next().unwrap(), size };
    }
    RuneDecoded { codepoint: char::REPLACEMENT_CHARACTER, size: 1 }
}

#[cfg(test)]
#[path = "base_test.rs"]
mod base_test;
