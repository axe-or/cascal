use crate::arena::Arena;
use crate::base::Str;
use std::io::{self, Write};

crate::def_arena_handle!(NodeID);

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Token {
    pub kind: TokenType,
    pub start: usize,
    pub end: usize,
    pub value_int: i64,
    pub value_real: f64,
}

pub fn escape_sequence(c: char) -> Option<char> {
    Some(match c {
        't' => '\t',
        'r' => '\r',
        'n' => '\n',
        '"' => '"',
        '\'' => '\'',
        '\\' => '\\',
        _ => return None,
    })
}

#[derive(Clone, Debug, PartialEq)]
pub enum ParserType {
    Named(Str),
    Slice(NodeID),
    Array { element: NodeID, length: i32 },
    Pointer(NodeID),
}

#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct NodeList {
    pub first: Option<NodeID>,
    pub last: Option<NodeID>,
}

#[derive(Clone, Debug, PartialEq)]
pub enum NodeValue {
    Integer(i64),
    Real(f64),
    Boolean(bool),
    String(Str),
    Identifier(Str),
    Unary {
        op: TokenType,
        operand: NodeID,
    },
    Binary {
        op: TokenType,
        left: NodeID,
        right: NodeID,
    },
    Index {
        object: NodeID,
        idx: NodeID,
    },
    Call {
        callable: NodeID,
        args: NodeList,
    },
    Field {
        identifier: Str,
        ty: NodeID,
    },
    ParserType(ParserType),
    VarDefinition {
        idents: NodeList,
        ty: NodeID,
        values: NodeList,
    },
    Assignment {
        left: NodeList,
        right: NodeList,
    },
    Block {
        statements: NodeList,
    },
    Return {
        values: NodeList,
    },
    Break(Str),
    Continue(Str),
    If {
        condition: NodeID,
        then_block: NodeID,
        else_branch: Option<NodeID>,
    },
    While {
        condition: NodeID,
        body: NodeID,
    },
    ProcDefinition {
        name: Str,
        args: NodeList,
        returns: NodeList,
        body: NodeID,
    },
}

#[derive(Clone, Debug, PartialEq)]
pub struct Node {
    pub parent: Option<NodeID>,
    pub next: Option<NodeID>,
    pub value: NodeValue,
}

#[derive(Debug, Default)]
pub struct AST {
    pub root: Option<NodeID>,
    pub arena: Arena<Node, NodeID>,
}

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

fn write_quoted_string(writer: &mut impl Write, value: &str) -> io::Result<()> {
    writer.write_all(b"\"")?;
    for c in value.chars() {
        match c {
            '\t' => writer.write_all(b"\\t")?,
            '\r' => writer.write_all(b"\\r")?,
            '\n' => writer.write_all(b"\\n")?,
            '"' => writer.write_all(b"\\\"")?,
            '\\' => writer.write_all(b"\\\\")?,
            c => {
                let mut buf = [0; 4];
                writer.write_all(c.encode_utf8(&mut buf).as_bytes())?;
            }
        }
    }
    writer.write_all(b"\"")
}

impl AST {
    pub fn push_to_list(&mut self, list: &mut NodeList, node: NodeID) {
        self.arena[node].next = None;
        if let Some(last) = list.last {
            self.arena[last].next = Some(node);
        } else {
            list.first = Some(node);
        }
        list.last = Some(node);
    }

    pub fn list_len(&self, list: NodeList) -> usize {
        let mut count = 0;
        let mut current = list.first;
        while let Some(id) = current {
            count += 1;
            current = self.arena[id].next;
        }
        count
    }

    pub fn set_list_parent(&mut self, list: NodeList, parent: NodeID) {
        let mut current = list.first;
        while let Some(id) = current {
            self.arena[id].parent = Some(parent);
            current = self.arena[id].next;
        }
    }

    pub fn make_node(&mut self, value: NodeValue) -> NodeID {
        use NodeValue as N;
        // Fixed-size child collections keep construction allocation-free apart from the arena.
        let mut children = [None; 3];
        let mut lists = [NodeList::default(); 2];
        match &value {
            N::Unary { operand, .. } => children[0] = Some(*operand),
            N::Binary { left, right, .. } => {
                children[0] = Some(*left);
                children[1] = Some(*right);
            }
            N::Index { object, idx } => {
                children[0] = Some(*object);
                children[1] = Some(*idx);
            }
            N::Call { callable, args } => {
                children[0] = Some(*callable);
                lists[0] = *args;
            }
            // Parameter groups share their type node, so it has no single parent.
            N::Field { .. } => {}
            N::ParserType(
                ParserType::Slice(element)
                | ParserType::Pointer(element)
                | ParserType::Array { element, .. },
            ) => children[0] = Some(*element),
            N::VarDefinition { idents, ty, values } => {
                children[0] = Some(*ty);
                lists = [*idents, *values];
            }
            N::Assignment { left, right } => lists = [*left, *right],
            N::Block { statements } => lists[0] = *statements,
            N::Return { values } => lists[0] = *values,
            N::If {
                condition,
                then_block,
                else_branch,
            } => children = [Some(*condition), Some(*then_block), *else_branch],
            N::While { condition, body } => {
                children[0] = Some(*condition);
                children[1] = Some(*body);
            }
            N::ProcDefinition {
                args,
                returns,
                body,
                ..
            } => {
                lists = [*args, *returns];
                children[0] = Some(*body);
            }
            _ => {}
        }
        let id = self.arena.alloc(Node {
            parent: None,
            next: None,
            value,
        });
        for child in children.into_iter().flatten() {
            self.arena[child].parent = Some(id);
        }
        for list in lists {
            self.set_list_parent(list, id);
        }
        id
    }

    pub fn format_list(
        &self,
        writer: &mut impl Write,
        list: NodeList,
        mut leading_space: bool,
    ) -> io::Result<()> {
        let mut current = list.first;
        while let Some(id) = current {
            if leading_space {
                writer.write_all(b" ")?;
            }
            self.format_node(writer, id)?;
            leading_space = true;
            current = self.arena[id].next;
        }
        Ok(())
    }

    pub fn format_node(&self, writer: &mut impl Write, id: NodeID) -> io::Result<()> {
        use NodeValue as N;
        let node = self
            .arena
            .get(id)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "invalid node ID"))?;
        match &node.value {
            N::Integer(v) => write!(writer, "{v}"),
            N::Real(v) => write!(writer, "{v}"),
            N::Boolean(v) => write!(writer, "{v}"),
            N::String(v) => write_quoted_string(writer, v),
            N::Identifier(v) => write!(writer, "{v}"),
            N::ParserType(ty) => match ty {
                ParserType::Named(name) => write!(writer, "{name}"),
                ParserType::Slice(element) => {
                    write!(writer, "[]")?;
                    self.format_node(writer, *element)
                }
                ParserType::Pointer(element) => {
                    write!(writer, "^")?;
                    self.format_node(writer, *element)
                }
                ParserType::Array { element, length } => {
                    write!(writer, "[{length}]")?;
                    self.format_node(writer, *element)
                }
            },
            N::Unary { op, operand } => {
                write!(writer, "({} ", token_type_name(*op))?;
                self.format_node(writer, *operand)?;
                write!(writer, ")")
            }
            N::Binary { op, left, right } => {
                write!(writer, "({} ", token_type_name(*op))?;
                self.format_node(writer, *left)?;
                write!(writer, " ")?;
                self.format_node(writer, *right)?;
                write!(writer, ")")
            }
            N::Index { object, idx } => {
                write!(writer, "([] ")?;
                self.format_node(writer, *object)?;
                write!(writer, " ")?;
                self.format_node(writer, *idx)?;
                write!(writer, ")")
            }
            N::Call { callable, args } => {
                write!(writer, "(call ")?;
                self.format_node(writer, *callable)?;
                self.format_list(writer, *args, true)?;
                write!(writer, ")")
            }
            N::Field { identifier, ty } => {
                write!(writer, "(field {identifier} ")?;
                self.format_node(writer, *ty)?;
                write!(writer, ")")
            }
            N::VarDefinition { idents, ty, values } => {
                write!(writer, "(var (")?;
                self.format_list(writer, *idents, false)?;
                write!(writer, ") ")?;
                self.format_node(writer, *ty)?;
                write!(writer, " (")?;
                self.format_list(writer, *values, false)?;
                write!(writer, "))")
            }
            N::Assignment { left, right } => {
                write!(writer, "(= (")?;
                self.format_list(writer, *left, false)?;
                write!(writer, ") (")?;
                self.format_list(writer, *right, false)?;
                write!(writer, "))")
            }
            N::Block { statements } => {
                write!(writer, "(block")?;
                self.format_list(writer, *statements, true)?;
                write!(writer, ")")
            }
            N::Return { values } => {
                write!(writer, "(return")?;
                self.format_list(writer, *values, true)?;
                write!(writer, ")")
            }
            N::Break(label) | N::Continue(label) => {
                write!(
                    writer,
                    "({}",
                    if matches!(node.value, N::Break(_)) {
                        "break"
                    } else {
                        "continue"
                    }
                )?;
                if !label.is_empty() {
                    write!(writer, " {label}")?;
                }
                write!(writer, ")")
            }
            N::If {
                condition,
                then_block,
                else_branch,
            } => {
                write!(writer, "(if ")?;
                self.format_node(writer, *condition)?;
                write!(writer, " ")?;
                self.format_node(writer, *then_block)?;
                if let Some(id) = else_branch {
                    write!(writer, " ")?;
                    self.format_node(writer, *id)?;
                }
                write!(writer, ")")
            }
            N::While { condition, body } => {
                write!(writer, "(while ")?;
                self.format_node(writer, *condition)?;
                write!(writer, " ")?;
                self.format_node(writer, *body)?;
                write!(writer, ")")
            }
            N::ProcDefinition {
                name,
                args,
                returns,
                body,
            } => {
                write!(writer, "(proc {name} (")?;
                self.format_list(writer, *args, false)?;
                write!(writer, ") (")?;
                self.format_list(writer, *returns, false)?;
                write!(writer, ") ")?;
                self.format_node(writer, *body)?;
                write!(writer, ")")
            }
        }
    }
}
