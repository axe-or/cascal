# Base

Shared foundation crate for Cascal. Exposes `SmallVec` and `smallvec!` from the
local, dependency-free copy of smallvec 1.16.1. The same exports are available
through `cascal::base`.

```rust
use base::{smallvec, SmallVec};

let values: SmallVec<[u32; 2]> = smallvec![1, 2];
```

`const_generics` is enabled, so any inline array capacity is supported. No registry
downloads are needed. Existing Cascal base utilities and `SmallArray` are unchanged.

See [vendoring notes](vendor/smallvec/VENDOR.md) for provenance, licenses, local
changes, and verification commands.
