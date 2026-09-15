use crate::base::rune_decode;
use crate::errors::{Error, ErrorType};
use crate::lang::{escape_sequence, keyword, Token, TokenType};

#[derive(Clone, Copy, Debug)]
pub struct Scanner<'a> {
    pub source: &'a [u8],
    pub current: usize,
}

#[derive(Clone, Debug)]
pub struct ScannerResult {
    pub token: Token,
    pub error: Option<Error>,
}

pub fn is_identifier_start(c: char) -> bool {
    c.is_ascii_alphabetic() || c == '_'
}
pub fn is_identifier_continue(c: char) -> bool {
    is_identifier_start(c) || c.is_ascii_digit()
}

pub fn base_of(c: char) -> Option<u32> {
    match c {
        'b' | 'B' => Some(2),
        'o' | 'O' => Some(8),
        'x' | 'X' => Some(16),
        _ => None,
    }
}
pub fn digit_of(c: char) -> Option<u32> {
    c.to_digit(16)
}

/// Convert a hexadecimal float without libc or a numeric parsing dependency.
pub fn parse_hex_real(text: &str) -> Option<f64> {
    let body = text
        .strip_prefix("0x")
        .or_else(|| text.strip_prefix("0X"))?;
    let (mantissa, exponent_text) = body.split_once(['p', 'P'])?;
    let exponent_digits = exponent_text.trim_start_matches(['+', '-']);
    if exponent_digits.is_empty() || !exponent_digits.bytes().all(|b| b.is_ascii_digit()) {
        return None;
    }
    let explicit_exponent: i64 = exponent_text.parse().unwrap_or_else(|_| {
        if exponent_text.starts_with('-') {
            i64::MIN
        } else {
            i64::MAX
        }
    });
    let integer_digits = mantissa.find('.').unwrap_or(mantissa.len()) as i64;
    let mut first = None;
    for (digits, c) in mantissa.chars().filter(|&c| c != '.').enumerate() {
        let digit = c.to_digit(16)?;
        if digit != 0 && first.is_none() {
            first = Some(
                (integer_digits - digits as i64 - 1) * 4 + (31 - digit.leading_zeros()) as i64,
            );
        }
    }
    let Some(highest_bit) = first else {
        return Some(0.0);
    };
    let mut exponent = explicit_exponent.saturating_add(highest_bit);
    if exponent > 1023 {
        return Some(f64::INFINITY);
    }
    if exponent < -1075 {
        return Some(0.0);
    }
    // Subnormal values retain fewer significant bits; round only once.
    let precision = (exponent + 1075).min(53) as usize;
    let mut significant = 0u64;
    let mut count = 0;
    let mut started = false;
    let mut guard = false;
    let mut sticky = false;
    for c in mantissa.chars().filter(|&c| c != '.') {
        let digit = c.to_digit(16)?;
        for bit in (0..4).rev() {
            let one = digit & (1 << bit) != 0;
            if !started && !one {
                continue;
            }
            started = true;
            if count < precision {
                significant = (significant << 1) | u64::from(one);
            } else if count == precision {
                guard = one;
            } else {
                sticky |= one;
            }
            count += 1;
        }
    }
    if count < precision {
        significant <<= precision - count;
    }
    if guard && (sticky || significant & 1 != 0) {
        significant += 1;
    }
    if precision < 53 {
        return Some(f64::from_bits(significant));
    }
    if significant == 1 << 53 {
        significant >>= 1;
        exponent += 1;
    }
    if exponent > 1023 {
        return Some(f64::INFINITY);
    }
    Some(f64::from_bits(
        ((exponent + 1023) as u64) << 52 | (significant & ((1 << 52) - 1)),
    ))
}

impl<'a> Scanner<'a> {
    pub fn new(source: &'a [u8]) -> Self {
        Self { source, current: 0 }
    }

    pub fn peek(&self, delta: usize) -> char {
        self.source
            .get(self.current.saturating_add(delta)..)
            .map(rune_decode)
            .map_or('\0', |r| r.codepoint)
    }

    pub fn advance(&mut self) -> char {
        let decoded = rune_decode(&self.source[self.current..]);
        self.current += decoded.size;
        decoded.codepoint
    }

    pub fn take_if(&mut self, expected: char) -> bool {
        if self.peek(0) != expected {
            return false;
        }
        self.advance();
        true
    }

    pub fn scan_string(&mut self, start: usize) -> ScannerResult {
        let mut error = None;
        loop {
            let offset = self.current;
            let c = self.advance();
            if c == '\0' {
                error = Some(Error::new(ErrorType::UnclosedString, offset));
                break;
            }
            if c == '"' {
                break;
            }
            if c == '\\' {
                let offset = self.current;
                let escaped = self.advance();
                if escaped == '\0' {
                    error = Some(Error::new(ErrorType::UnclosedString, offset));
                    break;
                }
                if escape_sequence(escaped).is_none() && error.is_none() {
                    error = Some(Error::character(
                        ErrorType::InvalidEscapeSequence,
                        offset,
                        escaped,
                    ));
                }
            } else if matches!(c, '\n' | '\r' | '\t') && error.is_none() {
                error = Some(Error::character(ErrorType::InvalidStringChar, offset, c));
            }
        }
        ScannerResult {
            error,
            ..ScannerResult::new(TokenType::String, start, self.current)
        }
    }

    pub fn scan_comment(&mut self) -> Result<(), Error> {
        if self.advance() == '/' {
            while self.current < self.source.len() {
                match self.advance() {
                    '\n' => break,
                    '\r' => {
                        self.take_if('\n');
                        break;
                    }
                    _ => {}
                }
            }
            return Ok(());
        }
        let mut depth = 1usize;
        while self.current < self.source.len() {
            let c = self.advance();
            if c == '/' && self.take_if('*') {
                depth += 1;
            } else if c == '*' && self.take_if('/') {
                depth -= 1;
                if depth == 0 {
                    return Ok(());
                }
            }
        }
        Err(Error::new(ErrorType::UnclosedComment, self.current))
    }

    pub fn scan_digit_sequence(&mut self, base: u32) -> bool {
        let mut has_digit = false;
        loop {
            let c = self.peek(0);
            if c == '_' {
                self.advance();
                continue;
            }
            if digit_of(c).is_none_or(|d| d >= base) {
                break;
            }
            has_digit = true;
            self.advance();
        }
        has_digit
    }

    pub fn scan_exponent(&mut self, lower: char, upper: char) -> bool {
        if self.peek(0) != lower && self.peek(0) != upper {
            return false;
        }
        let mut parsed = *self;
        parsed.advance();
        if matches!(parsed.peek(0), '+' | '-') {
            parsed.advance();
        }
        if !parsed.scan_digit_sequence(10) {
            return false;
        }
        *self = parsed;
        true
    }

    pub fn scan_real_suffix(&mut self, base: u32) -> bool {
        let mut parsed = *self;
        let mut fraction = false;
        if parsed.take_if('.') {
            fraction = parsed.scan_digit_sequence(base);
            if !fraction {
                parsed = *self;
            }
        }
        let exponent = if base == 16 {
            parsed.scan_exponent('p', 'P')
        } else {
            parsed.scan_exponent('e', 'E')
        };
        if (base == 16 || !fraction) && !exponent {
            return false;
        }
        *self = parsed;
        true
    }

    pub fn scan_real(&self, start: usize) -> ScannerResult {
        let text: String = self.source[start..self.current]
            .iter()
            .filter(|&&b| b != b'_')
            .map(|&b| b as char)
            .collect();
        let value = if text.starts_with("0x") || text.starts_with("0X") {
            parse_hex_real(&text)
        } else {
            text.parse::<f64>().ok()
        };
        let mut result = ScannerResult::new(TokenType::Real, start, self.current);
        let significant = if text.starts_with("0x") || text.starts_with("0X") {
            text[2..].split(['p', 'P']).next().unwrap_or("")
        } else {
            text.split(['e', 'E']).next().unwrap_or("")
        };
        let nonzero = significant
            .chars()
            .any(|c| matches!(c, '1'..='9' | 'a'..='f' | 'A'..='F'));
        match value {
            Some(v) if v.is_finite() && !(nonzero && v == 0.0) => result.token.value_real = v,
            _ => result.error = Some(Error::new(ErrorType::InvalidNumber, self.current)),
        }
        result
    }

    pub fn scan_integer(&mut self, start: usize, first: char) -> ScannerResult {
        let mut base = 10;
        let mut has_body = true;
        let mut has_digit = true;
        let mut value = first.to_digit(10).unwrap() as i64;
        let mut error = None;
        if first == '0' {
            if let Some(b) = base_of(self.peek(0)) {
                base = b;
                has_body = false;
                has_digit = false;
                value = 0;
                self.advance();
            }
        }
        loop {
            let c = self.peek(0);
            if c == '_' {
                has_body = true;
                self.advance();
                continue;
            }
            let Some(digit) = digit_of(c).filter(|&d| d < base) else {
                break;
            };
            has_body = true;
            has_digit = true;
            let offset = self.current;
            self.advance();
            if error.is_none() {
                match value
                    .checked_mul(base as i64)
                    .and_then(|v| v.checked_add(digit as i64))
                {
                    Some(v) => value = v,
                    None => error = Some(Error::character(ErrorType::InvalidNumber, offset, c)),
                }
            }
        }
        if (base == 10 || base == 16) && has_digit && self.scan_real_suffix(base) {
            return self.scan_real(start);
        }
        if !has_body {
            error = Some(Error::character(
                ErrorType::InvalidNumber,
                self.current,
                self.peek(0),
            ));
        }
        let mut result = ScannerResult::new(TokenType::Integer, start, self.current);
        result.token.value_int = value;
        result.error = error;
        result
    }

    pub fn next_token(&mut self) -> ScannerResult {
        use TokenType::*;
        let (start, c) = loop {
            while matches!(self.peek(0), ' ' | '\t' | '\n' | '\r' | '\x0c' | '\x0b') {
                self.advance();
            }
            let start = self.current;
            let c = self.advance();
            if c != '/' || !matches!(self.peek(0), '/' | '*') {
                break (start, c);
            }
            if let Err(error) = self.scan_comment() {
                return ScannerResult {
                    error: Some(error),
                    ..ScannerResult::new(Unknown, start, self.current)
                };
            }
        };
        if c.is_ascii_digit() {
            return self.scan_integer(start, c);
        }
        if c == '"' {
            return self.scan_string(start);
        }
        if is_identifier_start(c) {
            while is_identifier_continue(self.peek(0)) {
                self.advance();
            }
            let text = std::str::from_utf8(&self.source[start..self.current]).unwrap();
            return ScannerResult::new(keyword(text).unwrap_or(Identifier), start, self.current);
        }
        let kind = match c {
            '\0' if start == self.source.len() => EndOfFile,
            '{' => CurlyOpen,
            '}' => CurlyClose,
            '[' => SquareOpen,
            ']' => SquareClose,
            '(' => ParenOpen,
            ')' => ParenClose,
            ':' => Colon,
            ',' => Comma,
            '.' => Dot,
            ';' => Semicolon,
            '+' => Plus,
            '-' => {
                if self.take_if('>') {
                    Arrow
                } else {
                    Minus
                }
            }
            '*' => Star,
            '/' => Slash,
            '%' => Modulo,
            '&' => And,
            '|' => Or,
            '~' => Tilde,
            '^' => Caret,
            '=' => {
                if self.take_if('=') {
                    Eq
                } else {
                    Assign
                }
            }
            '!' => {
                if self.take_if('=') {
                    Neq
                } else {
                    Unknown
                }
            }
            '>' => {
                if self.take_if('=') {
                    GtEq
                } else if self.take_if('>') {
                    ShiftRight
                } else {
                    Gt
                }
            }
            '<' => {
                if self.take_if('=') {
                    LtEq
                } else if self.take_if('<') {
                    ShiftLeft
                } else {
                    Lt
                }
            }
            _ => Unknown,
        };
        let mut result = ScannerResult::new(kind, start, self.current);
        if kind == Unknown {
            result.error = Some(Error::character(ErrorType::UnknownChar, start, c));
        }
        result
    }

    pub fn peek_token(&self) -> ScannerResult {
        let mut copy = *self;
        copy.next_token()
    }
}

impl ScannerResult {
    pub fn new(kind: TokenType, start: usize, end: usize) -> Self {
        Self {
            token: Token {
                kind,
                start,
                end,
                ..Token::default()
            },
            error: None,
        }
    }
}

#[cfg(test)]
#[path = "scanner_test.rs"]
mod scanner_test;
