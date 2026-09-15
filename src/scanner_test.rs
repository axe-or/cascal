use super::*;

#[test]
fn numbers_and_suffix_boundaries() {
    let mut sc = Scanner::new(b"0b101 0o17 0xff 1_000 1.25 2e3 0x1.8p2 1.foo 2e+");
    for expected in [5, 15, 255, 1000] {
        let r = scan_next_token(&mut sc); assert_eq!(r.error, None); assert_eq!(r.token.value_int, expected);
    }
    for expected in [1.25, 2000.0, 6.0] {
        let r = scan_next_token(&mut sc); assert_eq!(r.error, None); assert_eq!(r.token.kind, TokenType::Real); assert_eq!(r.token.value_real, expected);
    }
    for expected in [TokenType::Integer, TokenType::Dot, TokenType::Identifier, TokenType::Integer, TokenType::Identifier, TokenType::Plus, TokenType::EndOfFile] {
        assert_eq!(scan_next_token(&mut sc).token.kind, expected);
    }
}

#[test]
fn comments_keywords_and_peeking() {
    let mut sc = Scanner::new(b"/* outer /* inner */ */ // hi\r\nproc true and struct variant");
    assert_eq!(scan_peek_token(&sc).token.kind, TokenType::Proc);
    assert_eq!(sc.current, 0);
    for kind in [TokenType::Proc, TokenType::True, TokenType::LogicAnd, TokenType::Record, TokenType::Variant, TokenType::EndOfFile] {
        let r = scan_next_token(&mut sc); assert_eq!(r.error, None); assert_eq!(r.token.kind, kind);
    }
}

#[test]
fn errors_and_progress() {
    for (text, kind, offset) in [
        ("0x", ErrorType::InvalidNumber, 2),
        ("9223372036854775808", ErrorType::InvalidNumber, 18),
        ("\"a\\q\"", ErrorType::InvalidEscapeSequence, 3),
        ("\"a\n\"", ErrorType::InvalidStringChar, 2),
        ("\"abc", ErrorType::UnclosedString, 4),
        ("/*", ErrorType::UnclosedComment, 2),
        ("é", ErrorType::UnknownChar, 0),
        ("1e9999", ErrorType::InvalidNumber, 6),
    ] {
        let mut sc = Scanner::new(text.as_bytes());
        let e = scan_next_token(&mut sc).error.unwrap();
        assert_eq!((e.kind, e.offset), (kind, offset), "{text}");
        assert!(sc.current > 0);
    }
    let mut sc = Scanner::new(b"\xff\0");
    assert!(scan_next_token(&mut sc).error.is_some());
    assert!(scan_next_token(&mut sc).error.is_some());
    assert_eq!(scan_next_token(&mut sc).token.kind, TokenType::EndOfFile);
}
