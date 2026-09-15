use crate::base::Arena;

crate::arena_id!(NodeID);

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Token {
    pub kind: TokenType,
    pub start: usize,
    pub end: usize,
    pub value_int: i64,
    pub value_real: f64,
}

pub fn escape_sequence(c: char) -> Option<char> {
    Some(match c { 't' => '\t', 'r' => '\r', 'n' => '\n', '"' => '"', '\'' => '\'', '\\' => '\\', _ => return None })
}

#[derive(Clone, Debug, PartialEq)]
pub enum ParserType {
    Named(String),
    Slice(NodeID),
    Array { element: NodeID, length: i32 },
    Pointer(NodeID),
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct NodeList { pub first: Option<NodeID>, pub last: Option<NodeID> }

#[derive(Clone, Debug, PartialEq)]
pub enum NodeValue {
    Integer(i64),
    Real(f64),
    Boolean(bool),
    String(String),
    Identifier(String),
    Unary { op: TokenType, operand: NodeID },
    Binary { op: TokenType, left: NodeID, right: NodeID },
    Index { object: NodeID, idx: NodeID },
    Call { callable: NodeID, args: NodeList },
    Field { identifier: String, ty: NodeID },
    ParserType(ParserType),
    VarDefinition { idents: NodeList, ty: NodeID, values: NodeList },
    Assignment { left: NodeList, right: NodeList },
    Block { statements: NodeList },
    Return { values: NodeList },
    Break(String),
    Continue(String),
    If { condition: NodeID, then_block: NodeID, else_branch: Option<NodeID> },
    While { condition: NodeID, body: NodeID },
    ProcDefinition { name: String, args: NodeList, returns: NodeList, body: NodeID },
}

#[derive(Clone, Debug, PartialEq)]
pub struct Node {
    pub parent: Option<NodeID>,
    pub next: Option<NodeID>,
    pub value: NodeValue,
}

#[derive(Debug, Default)]
pub struct AST { pub root: Option<NodeID>, pub arena: Arena<Node, NodeID> }

macro_rules! tokens {
    ($($name:ident => $text:literal,)*) => {
        #[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
        pub enum TokenType { #[default] Unknown, $($name,)* }
        pub fn token_type_name(kind: TokenType) -> &'static str {
            match kind { TokenType::Unknown => "<Unknown>", $(TokenType::$name => $text,)* }
        }
        pub fn keyword(text: &str) -> Option<TokenType> {
            match text { $($text if $text.as_bytes()[0].is_ascii_lowercase() => Some(TokenType::$name),)* _ => None }
        }
    }
}
tokens! {
    True => "true",
    False => "false",
    If => "if",
    Else => "else",
    For => "for",
    While => "while",
    Proc => "proc",
    Break => "break",
    Continue => "continue",
    Return => "return",
    Var => "var",
    Const => "const",
    Type => "type",
    Record => "struct",
    Variant => "variant",
    LogicAnd => "and",
    LogicOr => "or",
    LogicNot => "not",
    Identifier => "Id",
    Real => "Real",
    Integer => "Int",
    String => "String",
    CurlyOpen => "{",
    CurlyClose => "}",
    SquareOpen => "[",
    SquareClose => "]",
    ParenOpen => "(",
    ParenClose => ")",
    Colon => ":",
    Comma => ",",
    Dot => ".",
    Arrow => "->",
    Assign => "=",
    Semicolon => ";",
    Plus => "+",
    Minus => "-",
    Star => "*",
    Slash => "/",
    Modulo => "%",
    And => "&",
    Or => "|",
    Tilde => "~",
    Caret => "^",
    ShiftLeft => "<<",
    ShiftRight => ">>",
    Eq => "==",
    Neq => "!=",
    Gt => ">",
    GtEq => ">=",
    Lt => "<",
    LtEq => "<=",
    EndOfFile => "<EOF>",
}
