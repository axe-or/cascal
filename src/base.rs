use std::rc::Rc;

/// Shared string storage; cloning a `Str` only increments its reference count.
pub type Str = Rc<str>;

pub fn murmur3_hash32(data: &[u8], seed: u32) -> u32 {
    let mut hash = seed;
    let mut chunks = data.chunks_exact(4);
    for bytes in &mut chunks {
        let block = u32::from_le_bytes(bytes.try_into().unwrap())
            .wrapping_mul(0xcc9e2d51)
            .rotate_left(15)
            .wrapping_mul(0x1b873593);
        hash ^= block;
        hash = hash
            .rotate_left(13)
            .wrapping_mul(5)
            .wrapping_add(0xe6546b64);
    }
    let mut tail = 0;
    for (i, byte) in chunks.remainder().iter().enumerate() {
        tail |= (*byte as u32) << (i * 8);
    }
    if !chunks.remainder().is_empty() {
        hash ^= tail
            .wrapping_mul(0xcc9e2d51)
            .rotate_left(15)
            .wrapping_mul(0x1b873593);
    }
    hash ^= data.len() as u32;
    hash ^= hash >> 16;
    hash = hash.wrapping_mul(0x85ebca6b);
    hash ^= hash >> 13;
    hash = hash.wrapping_mul(0xc2b2ae35);
    hash ^ (hash >> 16)
}

pub fn str_hash(s: &str) -> u32 {
    murmur3_hash32(s.as_bytes(), 0)
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RuneDecoded {
    pub codepoint: char,
    pub size: usize,
}

pub fn rune_decode(bytes: &[u8]) -> RuneDecoded {
    if bytes.is_empty() {
        return RuneDecoded {
            codepoint: '\0',
            size: 0,
        };
    }

    let size = match bytes[0] {
        0..=0x7f => 1,
        0xc2..=0xdf => 2,
        0xe0..=0xef => 3,
        0xf0..=0xf4 => 4,
        _ => 1,
    };

    if let Some(Ok(s)) = bytes.get(..size).map(std::str::from_utf8) {
        return RuneDecoded {
            codepoint: s.chars().next().unwrap(),
            size,
        };
    }
    RuneDecoded {
        codepoint: char::REPLACEMENT_CHARACTER,
        size: 1,
    }
}

#[cfg(test)]
#[path = "base_test.rs"]
mod base_test;
