use super::*;

#[test]
fn real_rounding_and_extremes() {
    for (text, expected) in [
        ("0x1.00000000000008p0", 1.0),
        (
            "0x1.00000000000008001p0",
            f64::from_bits(1.0f64.to_bits() + 1),
        ),
        ("0x1.00000000000018p0", f64::from_bits(1.0f64.to_bits() + 2)),
        ("0x1p-1074", f64::from_bits(1)),
        ("0x1.8p-1074", f64::from_bits(2)),
        ("0x1.00000000001p-1075", f64::from_bits(1)),
        ("0x0.fffffffffffff8p-1022", f64::MIN_POSITIVE),
        ("0x1p-1022", f64::MIN_POSITIVE),
        ("0x1.fffffffffffffp1023", f64::MAX),
        ("1e-310", 1e-310),
        ("0.0e99999", 0.0),
    ] {
        let result = Scanner::new(text.as_bytes()).next_token();
        assert_eq!(result.error, None, "{text}");
        assert_eq!(
            result.token.value_real.to_bits(),
            expected.to_bits(),
            "{text}"
        );
    }
    for text in ["0x0.ep-9999", "0x1p-1075", "0x1p1024", "1e-9999"] {
        assert_eq!(
            Scanner::new(text.as_bytes())
                .next_token()
                .error
                .unwrap()
                .kind,
            ErrorType::InvalidNumber,
            "{text}"
        );
    }
}

#[test]
fn numbers_and_suffix_boundaries() {
    let mut sc = Scanner::new(b"0b101 0o17 0xff 1_000 1.25 2e3 0x1.8p2 1.foo 2e+");
    for expected in [5, 15, 255, 1000] {
        let r = sc.next_token();
        assert_eq!(r.error, None);
        assert_eq!(r.token.value_int, expected);
    }
    for expected in [1.25, 2000.0, 6.0] {
        let r = sc.next_token();
        assert_eq!(r.error, None);
        assert_eq!(r.token.kind, TokenType::Real);
        assert_eq!(r.token.value_real, expected);
    }
    for expected in [
        TokenType::Integer,
        TokenType::Dot,
        TokenType::Identifier,
        TokenType::Integer,
        TokenType::Identifier,
        TokenType::Plus,
        TokenType::EndOfFile,
    ] {
        assert_eq!(sc.next_token().token.kind, expected);
    }
}

#[test]
fn comments_keywords_and_peeking() {
    let mut sc = Scanner::new(b"/* outer /* inner */ */ // hi\r\nproc true and struct variant");
    assert_eq!(sc.peek_token().token.kind, TokenType::Proc);
    assert_eq!(sc.current, 0);
    for kind in [
        TokenType::Proc,
        TokenType::True,
        TokenType::LogicAnd,
        TokenType::Record,
        TokenType::Variant,
        TokenType::EndOfFile,
    ] {
        let r = sc.next_token();
        assert_eq!(r.error, None);
        assert_eq!(r.token.kind, kind);
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
        let e = sc.next_token().error.unwrap();
        assert_eq!((e.kind, e.offset), (kind, offset), "{text}");
        assert!(sc.current > 0);
    }
    let mut sc = Scanner::new(b"\xff\0");
    assert!(sc.next_token().error.is_some());
    assert!(sc.next_token().error.is_some());
    assert_eq!(sc.next_token().token.kind, TokenType::EndOfFile);
}
