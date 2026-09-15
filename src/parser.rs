use crate::errors::{Error, ErrorDetail, ErrorType};
use crate::lang::*;
use crate::scanner::Scanner;

pub struct Parser<'a> {
    pub scanner: Scanner<'a>,
    pub ast: AST,
}

pub type ParserResult = Result<NodeID, Error>;

pub fn parser_unexpected(got: Token, expected: TokenType) -> Error {
    Error {
        expected: Some(ErrorDetail::TokenType(expected)),
        got: Some(ErrorDetail::TokenType(got.kind)),
        ..Error::new(ErrorType::UnexpectedToken, got.start)
    }
}

pub fn prefix_binding_power(op: TokenType) -> Option<u8> {
    use TokenType::*;
    match op {
        Plus | Minus | Tilde | LogicNot => Some(80),
        _ => None,
    }
}

pub fn infix_binding_power(op: TokenType) -> Option<(u8, u8)> {
    use TokenType::*;
    let left = match op {
        ParenOpen | SquareOpen => 100,
        Dot => 90,
        Star | Tilde | Slash | Modulo | And | ShiftLeft | ShiftRight => 70,
        Plus | Minus | Or | Caret => 60,
        Eq | Neq | Gt | GtEq | Lt | LtEq => 50,
        LogicAnd => 40,
        LogicOr => 30,
        _ => return None,
    };
    Some((left, left + 1))
}

pub fn unescape_sequences_in_string(s: &str) -> String {
    let mut chars = s.chars();
    let mut result = String::with_capacity(s.len());
    while let Some(c) = chars.next() {
        result.push(if c == '\\' {
            escape_sequence(chars.next().expect("incomplete escape")).expect("invalid escape")
        } else {
            c
        });
    }
    result
}

pub fn parse(source: &[u8]) -> Result<AST, Error> {
    let mut parser = Parser::new(source);
    let mut definitions = NodeList::default();
    while parser.peek()?.kind != TokenType::EndOfFile {
        let node = parser.parse_proc_definition()?;
        parser.ast.push_to_list(&mut definitions, node);
    }
    parser.ast.root = definitions.first;
    Ok(parser.ast)
}

impl<'a> Parser<'a> {
    pub fn new(source: &'a [u8]) -> Self {
        Self {
            scanner: Scanner::new(source),
            ast: AST::default(),
        }
    }

    pub fn peek(&self) -> Result<Token, Error> {
        let r = self.scanner.peek_token();
        match r.error {
            Some(e) => Err(e),
            None => Ok(r.token),
        }
    }

    pub fn next_token(&mut self) -> Result<Token, Error> {
        let r = self.scanner.next_token();
        match r.error {
            Some(e) => Err(e),
            None => Ok(r.token),
        }
    }

    pub fn take_if(&mut self, kind: TokenType) -> Result<bool, Error> {
        if self.peek()?.kind != kind {
            return Ok(false);
        }
        self.next_token()?;
        Ok(true)
    }

    pub fn expect(&mut self, kind: TokenType) -> Result<Token, Error> {
        let token = self.peek()?;
        if token.kind != kind {
            return Err(parser_unexpected(token, kind));
        }
        self.next_token()
    }

    pub fn token_string(&self, token: Token) -> String {
        String::from_utf8_lossy(&self.scanner.source[token.start..token.end]).into_owned()
    }

    pub fn parse_prefix(&mut self) -> ParserResult {
        use TokenType::*;
        let token = self.next_token()?;
        let value = match token.kind {
            Integer => NodeValue::Integer(token.value_int),
            Real => NodeValue::Real(token.value_real),
            True | False => NodeValue::Boolean(token.kind == True),
            Identifier => NodeValue::Identifier(self.token_string(token)),
            String => {
                let text = self.token_string(token);
                NodeValue::String(unescape_sequences_in_string(&text[1..text.len() - 1]))
            }
            ParenOpen => {
                let expression = self.parse_expression_bp(0)?;
                self.expect(ParenClose)?;
                return Ok(expression);
            }
            op => {
                let bp =
                    prefix_binding_power(op).ok_or_else(|| parser_unexpected(token, Unknown))?;
                let operand = self.parse_expression_bp(bp)?;
                NodeValue::Unary { op, operand }
            }
        };
        Ok(self.ast.make_node(value))
    }

    pub fn parse_expression_bp(&mut self, minimum_bp: u8) -> ParserResult {
        let mut left = self.parse_prefix()?;
        loop {
            let op = self.peek()?.kind;
            let Some((lbp, rbp)) = infix_binding_power(op) else {
                break;
            };
            if lbp < minimum_bp {
                break;
            }
            self.next_token()?;
            let value = if op == TokenType::ParenOpen {
                let mut args = NodeList::default();
                if !self.take_if(TokenType::ParenClose)? {
                    loop {
                        let arg = self.parse_expression_bp(0)?;
                        self.ast.push_to_list(&mut args, arg);
                        if !self.take_if(TokenType::Comma)? {
                            self.expect(TokenType::ParenClose)?;
                            break;
                        }
                        if self.take_if(TokenType::ParenClose)? {
                            break;
                        }
                    }
                }
                NodeValue::Call {
                    callable: left,
                    args,
                }
            } else if op == TokenType::SquareOpen {
                let idx = self.parse_expression_bp(0)?;
                self.expect(TokenType::SquareClose)?;
                NodeValue::Index { object: left, idx }
            } else {
                let right = self.parse_expression_bp(rbp)?;
                NodeValue::Binary { op, left, right }
            };
            left = self.ast.make_node(value);
        }
        Ok(left)
    }

    pub fn parse_expression(&mut self) -> ParserResult {
        let node = self.parse_expression_bp(0)?;
        self.ast.root = Some(node);
        Ok(node)
    }

    pub fn parse_type(&mut self) -> ParserResult {
        use TokenType::*;
        let token = self.next_token()?;
        let ty = match token.kind {
            Identifier => ParserType::Named(self.token_string(token)),
            Caret => ParserType::Pointer(self.parse_type()?),
            SquareOpen => {
                if self.take_if(SquareClose)? {
                    ParserType::Slice(self.parse_type()?)
                } else {
                    let length = self.expect(Integer)?;
                    let length_value = i32::try_from(length.value_int)
                        .map_err(|_| Error::new(ErrorType::InvalidNumber, length.start))?;
                    self.expect(SquareClose)?;
                    ParserType::Array {
                        element: self.parse_type()?,
                        length: length_value,
                    }
                }
            }
            _ => return Err(parser_unexpected(token, Identifier)),
        };
        Ok(self.ast.make_node(NodeValue::ParserType(ty)))
    }

    pub fn parse_identifier_list(&mut self) -> Result<NodeList, Error> {
        let mut list = NodeList::default();
        loop {
            let token = self.expect(TokenType::Identifier)?;
            let name = self.token_string(token);
            let id = self.ast.make_node(NodeValue::Identifier(name));
            self.ast.push_to_list(&mut list, id);
            if !self.take_if(TokenType::Comma)? {
                break;
            }
        }
        Ok(list)
    }

    pub fn parse_expression_list(&mut self, end_delim: TokenType) -> Result<NodeList, Error> {
        let mut list = NodeList::default();
        loop {
            let node = self.parse_expression()?;
            self.ast.push_to_list(&mut list, node);
            let token = self.peek()?;
            if token.kind == end_delim || token.kind == TokenType::EndOfFile {
                break;
            }
            self.expect(TokenType::Comma)?;
        }
        Ok(list)
    }

    pub fn check_cardinality(&self, left: NodeList, right: NodeList) -> Result<(), Error> {
        let expected = self.ast.list_len(left);
        let got = self.ast.list_len(right);
        if expected == got {
            return Ok(());
        }
        Err(Error {
            expected: Some(ErrorDetail::Cardinality(expected)),
            got: Some(ErrorDetail::Cardinality(got)),
            ..Error::new(ErrorType::MismatchedListCardinality, self.peek()?.start)
        })
    }

    pub fn parse_var_declaration(&mut self) -> ParserResult {
        self.expect(TokenType::Var)?;
        let idents = self.parse_identifier_list()?;
        self.expect(TokenType::Colon)?;
        let ty = self.parse_type()?;
        self.expect(TokenType::Assign)?;
        let values = self.parse_expression_list(TokenType::Semicolon)?;
        self.check_cardinality(idents, values)?;
        Ok(self
            .ast
            .make_node(NodeValue::VarDefinition { idents, ty, values }))
    }

    pub fn parse_field_list(&mut self) -> Result<NodeList, Error> {
        let mut fields = NodeList::default();
        loop {
            let idents = self.parse_identifier_list()?;
            self.expect(TokenType::Colon)?;
            let ty = self.parse_type()?;
            let mut current = idents.first;
            while let Some(id) = current {
                let NodeValue::Identifier(identifier) = &self.ast.arena[id].value else {
                    unreachable!()
                };
                let value = NodeValue::Field {
                    identifier: identifier.clone(),
                    ty,
                };
                current = self.ast.arena[id].next;
                // Reuse the identifier allocation for the field.
                self.ast.arena[id].value = value;
                self.ast.push_to_list(&mut fields, id);
            }
            if !self.take_if(TokenType::Comma)? || self.peek()?.kind == TokenType::ParenClose {
                break;
            }
        }
        Ok(fields)
    }

    pub fn parse_proc_return_types(&mut self) -> Result<NodeList, Error> {
        let mut list = NodeList::default();
        if !self.take_if(TokenType::Arrow)? {
            return Ok(list);
        }
        let grouped = self.take_if(TokenType::ParenOpen)?;
        loop {
            let ty = self.parse_type()?;
            self.ast.push_to_list(&mut list, ty);
            if !grouped || !self.take_if(TokenType::Comma)? {
                break;
            }
        }
        if grouped {
            self.expect(TokenType::ParenClose)?;
        }
        Ok(list)
    }

    pub fn parse_proc_definition(&mut self) -> ParserResult {
        self.expect(TokenType::Proc)?;
        let token = self.expect(TokenType::Identifier)?;
        let name = self.token_string(token);
        self.expect(TokenType::ParenOpen)?;
        let args =
            if self.peek()?.kind == TokenType::ParenClose || self.take_if(TokenType::Comma)? {
                NodeList::default()
            } else {
                self.parse_field_list()?
            };
        self.expect(TokenType::ParenClose)?;
        let returns = self.parse_proc_return_types()?;
        let body = self.parse_block()?;
        Ok(self.ast.make_node(NodeValue::ProcDefinition {
            name,
            args,
            returns,
            body,
        }))
    }

    pub fn parse_expression_or_assignment(&mut self) -> ParserResult {
        let mut left = NodeList::default();
        loop {
            let node = self.parse_expression()?;
            self.ast.push_to_list(&mut left, node);
            if !self.take_if(TokenType::Comma)? {
                break;
            }
        }
        if !self.take_if(TokenType::Assign)? {
            if left.first != left.last {
                return Err(parser_unexpected(self.peek()?, TokenType::Assign));
            }
            return Ok(left.first.unwrap());
        }
        let right = self.parse_expression_list(TokenType::Semicolon)?;
        self.check_cardinality(left, right)?;
        Ok(self.ast.make_node(NodeValue::Assignment { left, right }))
    }

    pub fn parse_return_statement(&mut self) -> ParserResult {
        self.expect(TokenType::Return)?;
        let values = if self.peek()?.kind == TokenType::Semicolon {
            NodeList::default()
        } else {
            self.parse_expression_list(TokenType::Semicolon)?
        };
        Ok(self.ast.make_node(NodeValue::Return { values }))
    }

    pub fn parse_branch_control(&mut self, kind: TokenType) -> ParserResult {
        self.expect(kind)?;
        let label = if self.peek()?.kind == TokenType::Identifier {
            let token = self.next_token()?;
            self.token_string(token)
        } else {
            String::new()
        };
        let value = if kind == TokenType::Break {
            NodeValue::Break(label)
        } else {
            NodeValue::Continue(label)
        };
        Ok(self.ast.make_node(value))
    }

    pub fn parse_if_statement(&mut self) -> ParserResult {
        self.expect(TokenType::If)?;
        let condition = self.parse_expression()?;
        let then_block = self.parse_block()?;
        let else_branch = if self.take_if(TokenType::Else)? {
            Some(if self.peek()?.kind == TokenType::If {
                self.parse_if_statement()?
            } else {
                self.parse_block()?
            })
        } else {
            None
        };
        Ok(self.ast.make_node(NodeValue::If {
            condition,
            then_block,
            else_branch,
        }))
    }

    pub fn parse_while_statement(&mut self) -> ParserResult {
        self.expect(TokenType::While)?;
        let condition = self.parse_expression()?;
        let body = self.parse_block()?;
        Ok(self.ast.make_node(NodeValue::While { condition, body }))
    }

    pub fn parse_statement(&mut self) -> ParserResult {
        use TokenType::*;
        let kind = self.peek()?.kind;
        let node = match kind {
            Var => self.parse_var_declaration(),
            Return => self.parse_return_statement(),
            Break | Continue => self.parse_branch_control(kind),
            If => self.parse_if_statement(),
            While => self.parse_while_statement(),
            Proc => self.parse_proc_definition(),
            _ => self.parse_expression_or_assignment(),
        }?;
        if !matches!(kind, If | While | Proc) {
            self.expect(Semicolon)?;
        }
        Ok(node)
    }

    pub fn parse_block(&mut self) -> ParserResult {
        self.expect(TokenType::CurlyOpen)?;
        let mut statements = NodeList::default();
        while self.peek()?.kind != TokenType::CurlyClose {
            let token = self.peek()?;
            if token.kind == TokenType::EndOfFile {
                return Err(parser_unexpected(token, TokenType::CurlyClose));
            }
            let node = self.parse_statement()?;
            self.ast.push_to_list(&mut statements, node);
        }
        self.expect(TokenType::CurlyClose)?;
        Ok(self.ast.make_node(NodeValue::Block { statements }))
    }
}

#[cfg(test)]
#[path = "parser_test.rs"]
mod parser_test;
