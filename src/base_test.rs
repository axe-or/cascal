use super::*;

#[test]
fn hashes_match_murmur3_vectors() {
    assert_eq!(str_hash(""), 0);
    assert_eq!(str_hash("foo"), 0xf6a5c420);
    assert_eq!(str_hash("hello"), 0x248bfa47);
}

#[test]
fn decode_valid_and_invalid_utf8() {
    assert_eq!(
        rune_decode("😀".as_bytes()),
        RuneDecoded {
            codepoint: '😀',
            size: 4
        }
    );
    assert_eq!(rune_decode(&[0xc0, 0x80]).size, 1);
    assert_eq!(rune_decode(&[0xed, 0xa0, 0x80]).codepoint, '\u{fffd}');
    assert_eq!(rune_decode(&[]).size, 0);
}
