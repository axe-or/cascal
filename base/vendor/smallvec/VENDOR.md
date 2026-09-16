# Vendored smallvec

- Version: **1.16.1**, latest non-yanked stable release verified on 2026-09-16.
- Source: https://static.crates.io/crates/smallvec/smallvec-1.16.1.crate
- SHA-256: `ba467056f1b547ed52077911161fc86985becbc60e8e1857c8a144dab0def891`
- Upstream commit: `700e1c3ff899cda7799b18e0804889105fc8beea`
- Repository: https://github.com/servo/rust-smallvec
- License: MIT OR Apache-2.0; both license files and source notices are retained.

## Local changes

The manifest has no registry, optional, development, or build dependencies.
Removed `arbitrary`, `serde`, `malloc_size_of`, `bincode`, and `unty`, their
feature switches and integration code, and the associated serialization tests.
The `bincode1` development dependency was also removed. The unused upstream
lockfile, original manifest, and `arbitrary.rs` are not included.

The remaining upstream implementation and dependency-free features are retained.
`specialization` and `may_dangle` still require nightly Rust, as upstream documents;
they are not enabled by `base`. The `base` crate enables `const_generics` so any
inline array capacity works. Upstream tests were renamed to `smallvec_test.rs`.

## Verification

From the project root:

```
cargo test --offline -p base -p smallvec
cargo test --offline -p smallvec --features const_new,union,write,drain_filter,drain_keep_rest
cargo clippy --offline -p base --all-targets --no-deps -- -D warnings
cargo tree --offline --workspace
```

Clippy excludes the vendored dependency to avoid changing upstream code merely
for newer style lints. Its unit tests and doc-tests are run explicitly above.

To update, download the new stable release and verify its archive checksum against
the crates.io API. Reapply the changes above and run the tests. Do not replace the
local manifest with upstream's dependency-bearing manifest.
