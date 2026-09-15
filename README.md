# Cascal

A small language front end written in Rust, with no external dependencies.

```
cargo run --offline                     # reads source.txt
cargo run --offline -- path/to/file     # reads another source file
cargo test --offline
cargo clippy --offline --all-targets -- -D warnings
```

The executable echoes the source and prints each procedure as an S-expression.
File and parse errors go to stderr and return a failing exit status. Empty input
is valid. The parser implements expressions, typed variable declarations,
assignments, procedures, calls, indexing, if/else, while, return, break, and
continue. Other reserved keywords are not yet implemented. `syntax.txt` and
`grammar.txt` also contain proposed syntax beyond the implemented parser.

## Layout

The Rust modules roughly follow the original C files: `base`, `lang`, `scanner`,
`parser`, `errors`, `types`, and `symbol_table`. `arith` replaces generated
fixed-width arithmetic. Tests live in separate `_test.rs` files.

AST nodes, interned types, and symbols use append-only typed arenas backed by
vectors. Links use distinct `NodeID`, `TypeID`, and `SymbolID` wrappers over
`NonZeroU32`; both an ID and its `Option` occupy four bytes. IDs are local to
their arena. Arena growth preserves IDs, and dropping an arena releases its
contents. Names and strings are owned Rust strings. The old fixed byte-buffer
capacity and peak-allocation diagnostic are no longer used.

Rust vectors replace raw arrays, and `std::io::Write` and standard formatting
replace the C stream and string-builder plumbing. Real values use Rust's
round-trip decimal formatting. The small-array helper retains its two inline
elements. Type interning retains Murmur3 hashes and explicit collision chains;
standard-library hash maps replace generated hash tables. Type checking and
execution are not implemented, matching the original front end's scope.
