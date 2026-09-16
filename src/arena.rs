use std::marker::PhantomData;
use std::num::NonZeroU32;
use std::ops::{Index, IndexMut};

/// A typed, one-based index into an arena.
/// `from_raw` and `raw` must preserve the index unchanged.
pub trait ArenaHandle: Copy {
    fn from_raw(raw: NonZeroU32) -> Self;

    fn raw(self) -> NonZeroU32;
}

/// Append-only storage addressed by typed handles that remain valid as it grows.
/// All values are dropped with the arena. Handles must come from this arena;
/// their origin is not checked. Indexing with an out-of-bounds handle panics.
#[derive(Debug)]
pub struct Arena<T, Handle: ArenaHandle> {
    elems: Vec<T>,
    marker: PhantomData<Handle>,
}

impl<T, Handle: ArenaHandle> Default for Arena<T, Handle> {
    fn default() -> Self {
        Self::with_capacity(0)
    }
}

impl<T, Handle: ArenaHandle> Arena<T, Handle> {
    /// Creates an empty arena with room for at least `capacity` elements.
    pub fn with_capacity(capacity: usize) -> Self {
        Self {
            elems: Vec::with_capacity(capacity),
            marker: PhantomData,
        }
    }

    /// Appends a value and returns its handle. Panics if all `u32` handles are used.
    pub fn alloc(&mut self, value: T) -> Handle {
        let raw = u32::try_from(self.elems.len())
            .ok()
            .and_then(|n| n.checked_add(1))
            .and_then(NonZeroU32::new)
            .expect("arena exhausted");
        self.elems.push(value);
        Handle::from_raw(raw)
    }

    /// Returns the value at a handle, or `None` if its index is out of bounds.
    pub fn get(&self, id: Handle) -> Option<&T> {
        self.elems.get(id.raw().get() as usize - 1)
    }

    /// Returns a mutable value at a handle, or `None` if its index is out of bounds.
    pub fn get_mut(&mut self, id: Handle) -> Option<&mut T> {
        self.elems.get_mut(id.raw().get() as usize - 1)
    }

    pub fn len(&self) -> usize {
        self.elems.len()
    }

    pub fn is_empty(&self) -> bool {
        self.elems.is_empty()
    }
}

impl<T, Handle: ArenaHandle> Index<Handle> for Arena<T, Handle> {
    type Output = T;
    fn index(&self, id: Handle) -> &T {
        self.get(id).expect("invalid arena handle")
    }
}

impl<T, Handle: ArenaHandle> IndexMut<Handle> for Arena<T, Handle> {
    fn index_mut(&mut self, id: Handle) -> &mut T {
        self.get_mut(id).expect("invalid arena handle")
    }
}

/// Defines a distinct, four-byte arena handle; `Option<Handle>` is also four bytes.
#[macro_export]
macro_rules! def_arena_handle {
    ($name:ident) => {
        #[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
        #[repr(transparent)]
        pub struct $name(pub std::num::NonZeroU32);
        impl $crate::arena::ArenaHandle for $name {
            fn from_raw(raw: std::num::NonZeroU32) -> Self {
                Self(raw)
            }
            fn raw(self) -> std::num::NonZeroU32 {
                self.0
            }
        }
    };
}

#[cfg(test)]
#[path = "arena_test.rs"]
mod arena_test;
