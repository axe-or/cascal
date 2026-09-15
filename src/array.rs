//! Rust's growable array supplies the C array operations with bounds checks,
//! typed elements, and automatic destruction. Arena storage uses this same type.
pub use std::vec::Vec as Array;
