use crate::base::rune_decode;
use crate::errors::{Error, ErrorType};
use crate::lang::{escape_sequence, keyword, Token, TokenType};

#[derive(Clone, Copy, Debug)]
pub struct Scanner<'a> { pub source: &'a [u8], pub current: usize }

#[derive(Clone, Debug)]
pub struct ScannerResult { pub token: Token, pub error: Option<Error> }

impl<'a> Scanner<'a> {
    pub fn new(source: &'a [u8]) -> Self { Self { source, current: 0 } }
}

pub fn scan_peek(sc: &Scanner<'_>, delta: usize) -> char {
    sc.source.get(sc.current.saturating_add(delta)..).map(rune_decode)
        .map_or('\0', |r| r.codepoint)
}
pub fn scan_next(sc: &mut Scanner<'_>) -> char {
    let decoded = rune_decode(&sc.source[sc.current..]);
    sc.current += decoded.size;
    decoded.codepoint
}
pub fn scan_take_if(sc: &mut Scanner<'_>, expected: char) -> bool {
    if scan_peek(sc, 0) != expected { return false; }
    scan_next(sc);
    true
}
pub fn scanner_result(kind: TokenType, start: usize, end: usize) -> ScannerResult {
    ScannerResult { token: Token { kind, start, end, ..Token::default() }, error: None }
}
pub fn is_identifier_start(c: char) -> bool { c.is_ascii_alphabetic() || c == '_' }
pub fn is_identifier_continue(c: char) -> bool { is_identifier_start(c) || c.is_ascii_digit() }

pub fn scan_string(sc: &mut Scanner<'_>, start: usize) -> ScannerResult {
    let mut error = None;
    loop {
        let offset = sc.current;
        let c = scan_next(sc);
        if c == '\0' {
            error = Some(Error::new(ErrorType::UnclosedString, offset));
            break;
        }
        if c == '"' { break; }
        if c == '\\' {
            let offset = sc.current;
            let escaped = scan_next(sc);
            if escaped == '\0' {
                error = Some(Error::new(ErrorType::UnclosedString, offset));
                break;
            }
            if escape_sequence(escaped).is_none() && error.is_none() {
                error = Some(Error::character(ErrorType::InvalidEscapeSequence, offset, escaped));
            }
        } else if matches!(c, '\n' | '\r' | '\t') && error.is_none() {
            error = Some(Error::character(ErrorType::InvalidStringChar, offset, c));
        }
    }
    ScannerResult { error, ..scanner_result(TokenType::String, start, sc.current) }
}

pub fn scan_comment(sc: &mut Scanner<'_>) -> Result<(), Error> {
    if scan_next(sc) == '/' {
        while sc.current < sc.source.len() {
            match scan_next(sc) {
                '\n' => break,
                '\r' => { scan_take_if(sc, '\n'); break; }
                _ => {}
            }
        }
        return Ok(());
    }
    let mut depth = 1usize;
    while sc.current < sc.source.len() {
        let c = scan_next(sc);
        if c == '/' && scan_take_if(sc, '*') { depth += 1; }
        else if c == '*' && scan_take_if(sc, '/') {
            depth -= 1;
            if depth == 0 { return Ok(()); }
        }
    }
    Err(Error::new(ErrorType::UnclosedComment, sc.current))
}

pub fn base_of(c: char) -> Option<u32> {
    match c { 'b' | 'B' => Some(2), 'o' | 'O' => Some(8), 'x' | 'X' => Some(16), _ => None }
}
pub fn digit_of(c: char) -> Option<u32> { c.to_digit(16) }
pub fn scan_digit_sequence(sc: &mut Scanner<'_>, base: u32) -> bool {
    let mut has_digit = false;
    loop {
        let c = scan_peek(sc, 0);
        if c == '_' { scan_next(sc); continue; }
        if !digit_of(c).is_some_and(|d| d < base) { break; }
        has_digit = true;
        scan_next(sc);
    }
    has_digit
}
pub fn scan_exponent(sc: &mut Scanner<'_>, lower: char, upper: char) -> bool {
    if scan_peek(sc, 0) != lower && scan_peek(sc, 0) != upper { return false; }
    let mut parsed = *sc;
    scan_next(&mut parsed);
    if matches!(scan_peek(&parsed, 0), '+' | '-') { scan_next(&mut parsed); }
    if !scan_digit_sequence(&mut parsed, 10) { return false; }
    *sc = parsed;
    true
}
pub fn scan_real_suffix(sc: &mut Scanner<'_>, base: u32) -> bool {
    let mut parsed = *sc;
    let mut fraction = false;
    if scan_take_if(&mut parsed, '.') {
        fraction = scan_digit_sequence(&mut parsed, base);
        if !fraction { parsed = *sc; }
    }
    let exponent = if base == 16 { scan_exponent(&mut parsed, 'p', 'P') }
        else { scan_exponent(&mut parsed, 'e', 'E') };
    if (base == 16 && !exponent) || (!fraction && !exponent) { return false; }
    *sc = parsed;
    true
}

/// Convert a hexadecimal float without libc or a numeric parsing dependency.
pub fn parse_hex_real(text: &str) -> Option<f64> {
    let (mantissa, exponent) = text[2..].split_once(['p', 'P'])?;
    let exponent: i64 = exponent.parse().unwrap_or_else(|_| if exponent.starts_with('-') { i64::MIN } else { i64::MAX });
    let mut value = 0.0;
    let mut fractional = false;
    let mut shift = 0i64;
    // Keep the significant prefix in range, and account for remaining digits in the exponent.
    for c in mantissa.chars() {
        if c == '.' { fractional = true; continue; }
        let digit = c.to_digit(16)?;
        if value < 2f64.powi(1000) {
            value = value * 16.0 + digit as f64;
            if fractional { shift -= 4; }
        } else if !fractional { shift += 4; }
    }
    if value == 0.0 { return Some(0.0); }
    let mut exponent = exponent.saturating_add(shift);
    // Scale in chunks so subnormal powers don't disappear before multiplication.
    while exponent > 0 && value.is_finite() {
        let n = exponent.min(512) as i32;
        value *= 2f64.powi(n);
        exponent -= n as i64;
    }
    while exponent < 0 && value != 0.0 {
        let n = exponent.max(-512) as i32;
        value *= 2f64.powi(n);
        exponent -= n as i64;
    }
    Some(value)
}

pub fn scan_real(sc: &Scanner<'_>, start: usize) -> ScannerResult {
    let text: String = sc.source[start..sc.current].iter().filter(|&&b| b != b'_').map(|&b| b as char).collect();
    let value = if text.starts_with("0x") || text.starts_with("0X") { parse_hex_real(&text) } else { text.parse::<f64>().ok() };
    let mut result = scanner_result(TokenType::Real, start, sc.current);
    // strtod reports ERANGE for nonzero underflows and subnormal results as well.
    let significant = text.split(['e', 'E', 'p', 'P']).next().unwrap_or("");
    let nonzero = significant.chars().any(|c| matches!(c, '1'..='9' | 'a'..='f' | 'A'..='F'));
    match value {
        Some(v) if v.is_finite() && !(nonzero && v.abs() < f64::MIN_POSITIVE) => result.token.value_real = v,
        _ => result.error = Some(Error::new(ErrorType::InvalidNumber, sc.current)),
    }
    result
}

pub fn scan_integer(sc: &mut Scanner<'_>, start: usize, first: char) -> ScannerResult {
    let mut base = 10;
    let mut has_body = true;
    let mut has_digit = true;
    let mut value = first.to_digit(10).unwrap() as i64;
    let mut error = None;
    if first == '0' {
        if let Some(b) = base_of(scan_peek(sc, 0)) {
            base = b; has_body = false; has_digit = false; value = 0; scan_next(sc);
        }
    }
    loop {
        let c = scan_peek(sc, 0);
        if c == '_' { has_body = true; scan_next(sc); continue; }
        let Some(digit) = digit_of(c).filter(|&d| d < base) else { break; };
        has_body = true; has_digit = true;
        let offset = sc.current;
        scan_next(sc);
        if error.is_none() {
            match value.checked_mul(base as i64).and_then(|v| v.checked_add(digit as i64)) {
                Some(v) => value = v,
                None => error = Some(Error::character(ErrorType::InvalidNumber, offset, c)),
            }
        }
    }
    if (base == 10 || base == 16) && has_digit && scan_real_suffix(sc, base) { return scan_real(sc, start); }
    if !has_body { error = Some(Error::character(ErrorType::InvalidNumber, sc.current, scan_peek(sc, 0))); }
    let mut result = scanner_result(TokenType::Integer, start, sc.current);
    result.token.value_int = value;
    result.error = error;
    result
}

pub fn scan_next_token(sc: &mut Scanner<'_>) -> ScannerResult {
    use TokenType::*;
    let (start, c) = loop {
        while matches!(scan_peek(sc, 0), ' ' | '\t' | '\n' | '\r' | '\x0c' | '\x0b') { scan_next(sc); }
        let start = sc.current;
        let c = scan_next(sc);
        if c != '/' || !matches!(scan_peek(sc, 0), '/' | '*') { break (start, c); }
        if let Err(error) = scan_comment(sc) {
            return ScannerResult { error: Some(error), ..scanner_result(Unknown, start, sc.current) };
        }
    };
    if c.is_ascii_digit() { return scan_integer(sc, start, c); }
    if c == '"' { return scan_string(sc, start); }
    if is_identifier_start(c) {
        while is_identifier_continue(scan_peek(sc, 0)) { scan_next(sc); }
        let text = std::str::from_utf8(&sc.source[start..sc.current]).unwrap();
        return scanner_result(keyword(text).unwrap_or(Identifier), start, sc.current);
    }
    let kind = match c {
        '\0' if start == sc.source.len() => EndOfFile,
        '{' => CurlyOpen, '}' => CurlyClose, '[' => SquareOpen, ']' => SquareClose,
        '(' => ParenOpen, ')' => ParenClose, ':' => Colon, ',' => Comma,
        '.' => Dot, ';' => Semicolon, '+' => Plus,
        '-' => if scan_take_if(sc, '>') { Arrow } else { Minus },
        '*' => Star, '/' => Slash, '%' => Modulo, '&' => And, '|' => Or, '~' => Tilde, '^' => Caret,
        '=' => if scan_take_if(sc, '=') { Eq } else { Assign },
        '!' => if scan_take_if(sc, '=') { Neq } else { Unknown },
        '>' => if scan_take_if(sc, '=') { GtEq } else if scan_take_if(sc, '>') { ShiftRight } else { Gt },
        '<' => if scan_take_if(sc, '=') { LtEq } else if scan_take_if(sc, '<') { ShiftLeft } else { Lt },
        _ => Unknown,
    };
    let mut result = scanner_result(kind, start, sc.current);
    if kind == Unknown { result.error = Some(Error::character(ErrorType::UnknownChar, start, c)); }
    result
}

pub fn scan_peek_token(sc: &Scanner<'_>) -> ScannerResult {
    let mut copy = *sc;
    scan_next_token(&mut copy)
}

#[cfg(test)]
#[path = "scanner_test.rs"]
mod scanner_test;
