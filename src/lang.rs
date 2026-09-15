use crate::base::Arena;
use std::io::{self, Write};

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
    Named(String),
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
    String(String),
    Identifier(String),
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
        identifier: String,
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
    Break(String),
    Continue(String),
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
        name: String,
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
        use NodeValue::*;
        // Fixed-size child collections keep construction allocation-free apart from the arena.
        let mut children = [None; 3];
        let mut lists = [NodeList::default(); 2];
        match &value {
            Unary { operand, .. } => children[0] = Some(*operand),
            Binary { left, right, .. } => {
                children[0] = Some(*left);
                children[1] = Some(*right);
            }
            Index { object, idx } => {
                children[0] = Some(*object);
                children[1] = Some(*idx);
            }
            Call { callable, args } => {
                children[0] = Some(*callable);
                lists[0] = *args;
            }
            // Parameter groups share their type node, so it has no single parent.
            Field { .. } => {}
            ParserType(
                crate::lang::ParserType::Slice(element)
                | crate::lang::ParserType::Pointer(element)
                | crate::lang::ParserType::Array { element, .. },
            ) => children[0] = Some(*element),
            VarDefinition { idents, ty, values } => {
                children[0] = Some(*ty);
                lists = [*idents, *values];
            }
            Assignment { left, right } => lists = [*left, *right],
            Block { statements } => lists[0] = *statements,
            Return { values } => lists[0] = *values,
            If {
                condition,
                then_block,
                else_branch,
            } => children = [Some(*condition), Some(*then_block), *else_branch],
            While { condition, body } => {
                children[0] = Some(*condition);
                children[1] = Some(*body);
            }
            ProcDefinition {
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
        use NodeValue::*;
        let node = self
            .arena
            .get(id)
            .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "invalid node ID"))?;
        match &node.value {
            Integer(v) => write!(writer, "{v}"),
            Real(v) => write!(writer, "{v}"),
            Boolean(v) => write!(writer, "{v}"),
            String(v) => write_quoted_string(writer, v),
            Identifier(v) => write!(writer, "{v}"),
            ParserType(ty) => match ty {
                crate::lang::ParserType::Named(name) => write!(writer, "{name}"),
                crate::lang::ParserType::Slice(element) => {
                    write!(writer, "[]")?;
                    self.format_node(writer, *element)
                }
                crate::lang::ParserType::Pointer(element) => {
                    write!(writer, "^")?;
                    self.format_node(writer, *element)
                }
                crate::lang::ParserType::Array { element, length } => {
                    write!(writer, "[{length}]")?;
                    self.format_node(writer, *element)
                }
            },
            Unary { op, operand } => {
                write!(writer, "({} ", token_type_name(*op))?;
                self.format_node(writer, *operand)?;
                write!(writer, ")")
            }
            Binary { op, left, right } => {
                write!(writer, "({} ", token_type_name(*op))?;
                self.format_node(writer, *left)?;
                write!(writer, " ")?;
                self.format_node(writer, *right)?;
                write!(writer, ")")
            }
            Index { object, idx } => {
                write!(writer, "([] ")?;
                self.format_node(writer, *object)?;
                write!(writer, " ")?;
                self.format_node(writer, *idx)?;
                write!(writer, ")")
            }
            Call { callable, args } => {
                write!(writer, "(call ")?;
                self.format_node(writer, *callable)?;
                self.format_list(writer, *args, true)?;
                write!(writer, ")")
            }
            Field { identifier, ty } => {
                write!(writer, "(field {identifier} ")?;
                self.format_node(writer, *ty)?;
                write!(writer, ")")
            }
            VarDefinition { idents, ty, values } => {
                write!(writer, "(var (")?;
                self.format_list(writer, *idents, false)?;
                write!(writer, ") ")?;
                self.format_node(writer, *ty)?;
                write!(writer, " (")?;
                self.format_list(writer, *values, false)?;
                write!(writer, "))")
            }
            Assignment { left, right } => {
                write!(writer, "(= (")?;
                self.format_list(writer, *left, false)?;
                write!(writer, ") (")?;
                self.format_list(writer, *right, false)?;
                write!(writer, "))")
            }
            Block { statements } => {
                write!(writer, "(block")?;
                self.format_list(writer, *statements, true)?;
                write!(writer, ")")
            }
            Return { values } => {
                write!(writer, "(return")?;
                self.format_list(writer, *values, true)?;
                write!(writer, ")")
            }
            Break(label) | Continue(label) => {
                write!(
                    writer,
                    "({}",
                    if matches!(node.value, Break(_)) {
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
            If {
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
            While { condition, body } => {
                write!(writer, "(while ")?;
                self.format_node(writer, *condition)?;
                write!(writer, " ")?;
                self.format_node(writer, *body)?;
                write!(writer, ")")
            }
            ProcDefinition {
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
