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

    pub fn scan_digit_sequence(&mut self) -> bool {
        let mut has_digit = false;
        loop {
            let c = self.peek(0);
            if c == '_' {
                self.advance();
                continue;
            }
            if !c.is_ascii_digit() {
                break;
            }
            has_digit = true;
            self.advance();
        }
        has_digit
    }

    pub fn scan_exponent(&mut self) -> bool {
        if !matches!(self.peek(0), 'e' | 'E') {
            return false;
        }
        let mut parsed = *self;
        parsed.advance();
        if matches!(parsed.peek(0), '+' | '-') {
            parsed.advance();
        }
        if !parsed.scan_digit_sequence() {
            return false;
        }
        *self = parsed;
        true
    }

    pub fn scan_real_suffix(&mut self) -> bool {
        let mut parsed = *self;
        let mut fraction = false;
        if parsed.take_if('.') {
            fraction = parsed.scan_digit_sequence();
            if !fraction {
                parsed = *self;
            }
        }
        let exponent = parsed.scan_exponent();
        if !fraction && !exponent {
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

        let value = text.parse::<f64>().ok();
        let mut result = ScannerResult::new(TokenType::Real, start, self.current);
        let significant = text.split(['e', 'E']).next().unwrap_or("");
        let nonzero = significant.chars().any(|c| matches!(c, '1'..='9'));

        match value {
            Some(v) if v.is_finite() && !(nonzero && v == 0.0) => result.token.value_real = v,
            _ => result.error = Some(Error::new(ErrorType::InvalidNumber, self.current)),
        }
        result
    }

    pub fn scan_integer(&mut self, start: usize, first: char) -> ScannerResult {
        let mut base = 10;
        let mut has_body = true;
        let mut value = first.to_digit(10).unwrap() as i64;
        let mut error = None;
        if first == '0' {
            if let Some(b) = base_of(self.peek(0)) {
                base = b;
                has_body = false;
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

        if base == 10 && self.scan_real_suffix() {
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
