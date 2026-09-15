.PHONY: all run test clean

all:
	cargo build --offline

run:
	cargo run --offline

test:
	cargo test --offline

clean:
	cargo clean
