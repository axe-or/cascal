//! Shared foundation for Cascal, including the dependency-free vendored SmallVec.

pub use smallvec::{smallvec, Array, CollectionAllocErr, SmallVec};

#[cfg(test)]
#[path = "smallvec_test.rs"]
mod smallvec_test;
