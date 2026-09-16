use crate::base::Str;
use crate::lang::TokenType;
use std::fmt;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
#[repr(u8)]
pub enum ErrorType {
    UnexpectedChar = 1,
    UnknownChar,
    InvalidBase,
    InvalidNumber,
    InvalidEscapeSequence,
    InvalidStringChar,
    UnclosedString,
    UnclosedComment,
    UnexpectedToken,
    MismatchedListCardinality,
}

pub fn error_type_name(kind: ErrorType) -> &'static str {
    type E = ErrorType;
    match kind {
        E::UnexpectedChar => "unexpected character",
        E::UnknownChar => "unknown character",
        E::InvalidBase => "invalid base",
        E::InvalidNumber => "invalid number",
        E::InvalidEscapeSequence => "invalid escape sequence",
        E::InvalidStringChar => "invalid string character",
        E::UnclosedString => "unclosed string",
        E::UnclosedComment => "unclosed comment",
        E::UnexpectedToken => "unexpected token",
        E::MismatchedListCardinality => "mismatched cardinality",
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum ErrorDetail {
    Character(char),
    TokenType(TokenType),
    Cardinality(usize),
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Error {
    pub file: Str,
    pub offset: usize,
    pub kind: ErrorType,
    pub expected: Option<ErrorDetail>,
    pub got: Option<ErrorDetail>,
}

impl Error {
    pub fn new(kind: ErrorType, offset: usize) -> Self {
        Self {
            file: Str::default(),
            offset,
            kind,
            expected: None,
            got: None,
        }
    }
    pub fn character(kind: ErrorType, offset: usize, got: char) -> Self {
        Self {
            got: Some(ErrorDetail::Character(got)),
            ..Self::new(kind, offset)
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}:{} error[E{:04}]: {}",
            self.file,
            self.offset,
            self.kind as u8,
            error_type_name(self.kind)
        )
    }
}

impl std::error::Error for Error {}
