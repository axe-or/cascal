use crate::errors::{Error, ErrorDetail, ErrorType};
use crate::lang::*;
use crate::scanner::{scan_next_token, scan_peek_token, Scanner};
use std::io::{self, Write};

pub struct Parser<'a> { pub scanner: Scanner<'a>, pub ast: AST }
pub type ParserResult = Result<NodeID, Error>;

pub fn parser_make(source: &[u8]) -> Parser<'_> { Parser { scanner: Scanner::new(source), ast: AST::default() } }

pub fn parser_peek(parser: &Parser<'_>) -> Result<Token, Error> {
    let r = scan_peek_token(&parser.scanner);
    match r.error { Some(e) => Err(e), None => Ok(r.token) }
}
pub fn parser_next(parser: &mut Parser<'_>) -> Result<Token, Error> {
    let r = scan_next_token(&mut parser.scanner);
    match r.error { Some(e) => Err(e), None => Ok(r.token) }
}
pub fn parser_unexpected(got: Token, expected: TokenType) -> Error {
    Error { expected: Some(ErrorDetail::TokenType(expected)), got: Some(ErrorDetail::TokenType(got.kind)), ..Error::new(ErrorType::UnexpectedToken, got.start) }
}
pub fn parser_take_if(parser: &mut Parser<'_>, kind: TokenType) -> Result<bool, Error> {
    if parser_peek(parser)?.kind != kind { return Ok(false); }
    parser_next(parser)?;
    Ok(true)
}
pub fn parser_expect(parser: &mut Parser<'_>, kind: TokenType) -> Result<Token, Error> {
    let token = parser_peek(parser)?;
    if token.kind != kind { return Err(parser_unexpected(token, kind)); }
    parser_next(parser)
}
pub fn parser_token_string(parser: &Parser<'_>, token: Token) -> String {
    String::from_utf8_lossy(&parser.scanner.source[token.start..token.end]).into_owned()
}

pub fn node_list_push(ast: &mut AST, list: &mut NodeList, node: NodeID) {
    ast.arena[node].next = None;
    if let Some(last) = list.last { ast.arena[last].next = Some(node); }
    else { list.first = Some(node); }
    list.last = Some(node);
}
pub fn node_list_cardinality(ast: &AST, list: NodeList) -> usize {
    let mut count = 0;
    let mut current = list.first;
    while let Some(id) = current { count += 1; current = ast.arena[id].next; }
    count
}
pub fn node_list_set_parent(ast: &mut AST, list: NodeList, parent: NodeID) {
    let mut current = list.first;
    while let Some(id) = current { ast.arena[id].parent = Some(parent); current = ast.arena[id].next; }
}

pub fn ast_make_node(ast: &mut AST, value: NodeValue) -> NodeID {
    use NodeValue::*;
    // Fixed-size child collections keep construction allocation-free apart from the arena.
    let mut children = [None; 3];
    let mut lists = [NodeList::default(); 2];
    match &value {
        Unary { operand, .. } => children[0] = Some(*operand),
        Binary { left, right, .. } => { children[0] = Some(*left); children[1] = Some(*right); }
        Index { object, idx } => { children[0] = Some(*object); children[1] = Some(*idx); }
        Call { callable, args } => { children[0] = Some(*callable); lists[0] = *args; }
        // Parameter groups share their type node, so it has no single parent.
        Field { .. } => {}
        ParserType(crate::lang::ParserType::Slice(element) | crate::lang::ParserType::Pointer(element) | crate::lang::ParserType::Array { element, .. }) => children[0] = Some(*element),
        VarDefinition { idents, ty, values } => { children[0] = Some(*ty); lists = [*idents, *values]; }
        Assignment { left, right } => lists = [*left, *right],
        Block { statements } => lists[0] = *statements,
        Return { values } => lists[0] = *values,
        If { condition, then_block, else_branch } => children = [Some(*condition), Some(*then_block), *else_branch],
        While { condition, body } => { children[0] = Some(*condition); children[1] = Some(*body); }
        ProcDefinition { args, returns, body, .. } => { lists = [*args, *returns]; children[0] = Some(*body); }
        _ => {}
    }
    let id = ast.arena.alloc(Node { parent: None, next: None, value });
    for child in children.into_iter().flatten() { ast.arena[child].parent = Some(id); }
    for list in lists { node_list_set_parent(ast, list, id); }
    id
}

pub fn prefix_binding_power(op: TokenType) -> Option<u8> {
    use TokenType::*;
    match op { Plus | Minus | Tilde | LogicNot => Some(80), _ => None }
}
pub fn infix_binding_power(op: TokenType) -> Option<(u8, u8)> {
    use TokenType::*;
    let left = match op {
        ParenOpen | SquareOpen => 100, Dot => 90,
        Star | Tilde | Slash | Modulo | And | ShiftLeft | ShiftRight => 70,
        Plus | Minus | Or | Caret => 60,
        Eq | Neq | Gt | GtEq | Lt | LtEq => 50,
        LogicAnd => 40, LogicOr => 30, _ => return None,
    };
    Some((left, left + 1))
}
pub fn unescape_sequences_in_string(s: &str) -> String {
    let mut chars = s.chars();
    let mut result = String::with_capacity(s.len());
    while let Some(c) = chars.next() {
        result.push(if c == '\\' { escape_sequence(chars.next().expect("incomplete escape")).expect("invalid escape") } else { c });
    }
    result
}
pub fn parse_prefix(parser: &mut Parser<'_>) -> ParserResult {
    use TokenType::*;
    let token = parser_next(parser)?;
    let value = match token.kind {
        Integer => NodeValue::Integer(token.value_int),
        Real => NodeValue::Real(token.value_real),
        True | False => NodeValue::Boolean(token.kind == True),
        Identifier => NodeValue::Identifier(parser_token_string(parser, token)),
        String => {
            let text = parser_token_string(parser, token);
            NodeValue::String(unescape_sequences_in_string(&text[1..text.len() - 1]))
        }
        ParenOpen => {
            let expression = parse_expression_bp(parser, 0)?;
            parser_expect(parser, ParenClose)?;
            return Ok(expression);
        }
        op => {
            let bp = prefix_binding_power(op).ok_or_else(|| parser_unexpected(token, Unknown))?;
            let operand = parse_expression_bp(parser, bp)?;
            NodeValue::Unary { op, operand }
        }
    };
    Ok(ast_make_node(&mut parser.ast, value))
}
pub fn parse_expression_bp(parser: &mut Parser<'_>, minimum_bp: u8) -> ParserResult {
    let mut left = parse_prefix(parser)?;
    loop {
        let op = parser_peek(parser)?.kind;
        let Some((lbp, rbp)) = infix_binding_power(op) else { break; };
        if lbp < minimum_bp { break; }
        parser_next(parser)?;
        let value = if op == TokenType::ParenOpen {
            let mut args = NodeList::default();
            if !parser_take_if(parser, TokenType::ParenClose)? {
                loop {
                    let arg = parse_expression_bp(parser, 0)?;
                    node_list_push(&mut parser.ast, &mut args, arg);
                    if !parser_take_if(parser, TokenType::Comma)? {
                        parser_expect(parser, TokenType::ParenClose)?;
                        break;
                    }
                    if parser_take_if(parser, TokenType::ParenClose)? { break; }
                }
            }
            NodeValue::Call { callable: left, args }
        } else if op == TokenType::SquareOpen {
            let idx = parse_expression_bp(parser, 0)?;
            parser_expect(parser, TokenType::SquareClose)?;
            NodeValue::Index { object: left, idx }
        } else {
            let right = parse_expression_bp(parser, rbp)?;
            NodeValue::Binary { op, left, right }
        };
        left = ast_make_node(&mut parser.ast, value);
    }
    Ok(left)
}
pub fn parse_expression(parser: &mut Parser<'_>) -> ParserResult {
    let node = parse_expression_bp(parser, 0)?;
    parser.ast.root = Some(node);
    Ok(node)
}
pub fn parse_type(parser: &mut Parser<'_>) -> ParserResult {
    use TokenType::*;
    let token = parser_next(parser)?;
    let ty = match token.kind {
        Identifier => ParserType::Named(parser_token_string(parser, token)),
        Caret => ParserType::Pointer(parse_type(parser)?),
        SquareOpen => {
            if parser_take_if(parser, SquareClose)? { ParserType::Slice(parse_type(parser)?) }
            else {
                let length = parser_expect(parser, Integer)?;
                let length_value = i32::try_from(length.value_int).map_err(|_| Error::new(ErrorType::InvalidNumber, length.start))?;
                parser_expect(parser, SquareClose)?;
                ParserType::Array { element: parse_type(parser)?, length: length_value }
            }
        }
        _ => return Err(parser_unexpected(token, Identifier)),
    };
    Ok(ast_make_node(&mut parser.ast, NodeValue::ParserType(ty)))
}
pub fn parse_identifier_list(parser: &mut Parser<'_>) -> Result<NodeList, Error> {
    let mut list = NodeList::default();
    loop {
        let token = parser_expect(parser, TokenType::Identifier)?;
        let name = parser_token_string(parser, token);
        let id = ast_make_node(&mut parser.ast, NodeValue::Identifier(name));
        node_list_push(&mut parser.ast, &mut list, id);
        if !parser_take_if(parser, TokenType::Comma)? { break; }
    }
    Ok(list)
}
pub fn parse_expression_list(parser: &mut Parser<'_>, end_delim: TokenType) -> Result<NodeList, Error> {
    let mut list = NodeList::default();
    loop {
        let node = parse_expression(parser)?;
        node_list_push(&mut parser.ast, &mut list, node);
        let token = parser_peek(parser)?;
        if token.kind == end_delim || token.kind == TokenType::EndOfFile { break; }
        parser_expect(parser, TokenType::Comma)?;
    }
    Ok(list)
}
pub fn parser_check_cardinality(parser: &Parser<'_>, left: NodeList, right: NodeList) -> Result<(), Error> {
    let expected = node_list_cardinality(&parser.ast, left);
    let got = node_list_cardinality(&parser.ast, right);
    if expected == got { return Ok(()); }
    Err(Error { expected: Some(ErrorDetail::Cardinality(expected)), got: Some(ErrorDetail::Cardinality(got)), ..Error::new(ErrorType::MismatchedListCardinality, parser_peek(parser)?.start) })
}
pub fn parse_var_declaration(parser: &mut Parser<'_>) -> ParserResult {
    parser_expect(parser, TokenType::Var)?;
    let idents = parse_identifier_list(parser)?;
    parser_expect(parser, TokenType::Colon)?;
    let ty = parse_type(parser)?;
    parser_expect(parser, TokenType::Assign)?;
    let values = parse_expression_list(parser, TokenType::Semicolon)?;
    parser_check_cardinality(parser, idents, values)?;
    Ok(ast_make_node(&mut parser.ast, NodeValue::VarDefinition { idents, ty, values }))
}
pub fn parse_field_list(parser: &mut Parser<'_>) -> Result<NodeList, Error> {
    let mut fields = NodeList::default();
    loop {
        let idents = parse_identifier_list(parser)?;
        parser_expect(parser, TokenType::Colon)?;
        let ty = parse_type(parser)?;
        let mut current = idents.first;
        while let Some(id) = current {
            let NodeValue::Identifier(identifier) = &parser.ast.arena[id].value else { unreachable!() };
            let value = NodeValue::Field { identifier: identifier.clone(), ty };
            current = parser.ast.arena[id].next;
            // Reuse the identifier allocation for the field.
            parser.ast.arena[id].value = value;
            node_list_push(&mut parser.ast, &mut fields, id);
        }
        if !parser_take_if(parser, TokenType::Comma)? || parser_peek(parser)?.kind == TokenType::ParenClose { break; }
    }
    Ok(fields)
}
pub fn parse_proc_return_types(parser: &mut Parser<'_>) -> Result<NodeList, Error> {
    let mut list = NodeList::default();
    if !parser_take_if(parser, TokenType::Arrow)? { return Ok(list); }
    let grouped = parser_take_if(parser, TokenType::ParenOpen)?;
    loop {
        let ty = parse_type(parser)?;
        node_list_push(&mut parser.ast, &mut list, ty);
        if !grouped || !parser_take_if(parser, TokenType::Comma)? { break; }
    }
    if grouped { parser_expect(parser, TokenType::ParenClose)?; }
    Ok(list)
}
pub fn parse_proc_definition(parser: &mut Parser<'_>) -> ParserResult {
    parser_expect(parser, TokenType::Proc)?;
    let token = parser_expect(parser, TokenType::Identifier)?;
    let name = parser_token_string(parser, token);
    parser_expect(parser, TokenType::ParenOpen)?;
    let args = if parser_peek(parser)?.kind == TokenType::ParenClose || parser_take_if(parser, TokenType::Comma)? { NodeList::default() }
        else { parse_field_list(parser)? };
    parser_expect(parser, TokenType::ParenClose)?;
    let returns = parse_proc_return_types(parser)?;
    let body = parse_block(parser)?;
    Ok(ast_make_node(&mut parser.ast, NodeValue::ProcDefinition { name, args, returns, body }))
}
pub fn parse_expression_or_assignment(parser: &mut Parser<'_>) -> ParserResult {
    let mut left = NodeList::default();
    loop {
        let node = parse_expression(parser)?;
        node_list_push(&mut parser.ast, &mut left, node);
        if !parser_take_if(parser, TokenType::Comma)? { break; }
    }
    if !parser_take_if(parser, TokenType::Assign)? {
        if left.first != left.last { return Err(parser_unexpected(parser_peek(parser)?, TokenType::Assign)); }
        return Ok(left.first.unwrap());
    }
    let right = parse_expression_list(parser, TokenType::Semicolon)?;
    parser_check_cardinality(parser, left, right)?;
    Ok(ast_make_node(&mut parser.ast, NodeValue::Assignment { left, right }))
}
pub fn parse_return_statement(parser: &mut Parser<'_>) -> ParserResult {
    parser_expect(parser, TokenType::Return)?;
    let values = if parser_peek(parser)?.kind == TokenType::Semicolon { NodeList::default() }
        else { parse_expression_list(parser, TokenType::Semicolon)? };
    Ok(ast_make_node(&mut parser.ast, NodeValue::Return { values }))
}
pub fn parse_branch_control(parser: &mut Parser<'_>, kind: TokenType) -> ParserResult {
    parser_expect(parser, kind)?;
    let label = if parser_peek(parser)?.kind == TokenType::Identifier {
        let token = parser_next(parser)?;
        parser_token_string(parser, token)
    } else { String::new() };
    let value = if kind == TokenType::Break { NodeValue::Break(label) } else { NodeValue::Continue(label) };
    Ok(ast_make_node(&mut parser.ast, value))
}
pub fn parse_if_statement(parser: &mut Parser<'_>) -> ParserResult {
    parser_expect(parser, TokenType::If)?;
    let condition = parse_expression(parser)?;
    let then_block = parse_block(parser)?;
    let else_branch = if parser_take_if(parser, TokenType::Else)? {
        Some(if parser_peek(parser)?.kind == TokenType::If { parse_if_statement(parser)? } else { parse_block(parser)? })
    } else { None };
    Ok(ast_make_node(&mut parser.ast, NodeValue::If { condition, then_block, else_branch }))
}
pub fn parse_while_statement(parser: &mut Parser<'_>) -> ParserResult {
    parser_expect(parser, TokenType::While)?;
    let condition = parse_expression(parser)?;
    let body = parse_block(parser)?;
    Ok(ast_make_node(&mut parser.ast, NodeValue::While { condition, body }))
}
pub fn parse_statement(parser: &mut Parser<'_>) -> ParserResult {
    use TokenType::*;
    let kind = parser_peek(parser)?.kind;
    let node = match kind {
        Var => parse_var_declaration(parser), Return => parse_return_statement(parser),
        Break | Continue => parse_branch_control(parser, kind), If => parse_if_statement(parser),
        While => parse_while_statement(parser), Proc => parse_proc_definition(parser),
        _ => parse_expression_or_assignment(parser),
    }?;
    if !matches!(kind, If | While | Proc) { parser_expect(parser, Semicolon)?; }
    Ok(node)
}
pub fn parse_block(parser: &mut Parser<'_>) -> ParserResult {
    parser_expect(parser, TokenType::CurlyOpen)?;
    let mut statements = NodeList::default();
    while parser_peek(parser)?.kind != TokenType::CurlyClose {
        let token = parser_peek(parser)?;
        if token.kind == TokenType::EndOfFile { return Err(parser_unexpected(token, TokenType::CurlyClose)); }
        let node = parse_statement(parser)?;
        node_list_push(&mut parser.ast, &mut statements, node);
    }
    parser_expect(parser, TokenType::CurlyClose)?;
    Ok(ast_make_node(&mut parser.ast, NodeValue::Block { statements }))
}
pub fn parse(source: &[u8]) -> Result<AST, Error> {
    let mut parser = parser_make(source);
    let mut definitions = NodeList::default();
    while parser_peek(&parser)?.kind != TokenType::EndOfFile {
        let node = parse_proc_definition(&mut parser)?;
        node_list_push(&mut parser.ast, &mut definitions, node);
    }
    parser.ast.root = definitions.first;
    Ok(parser.ast)
}

pub fn write_quoted_string(writer: &mut impl Write, value: &str) -> io::Result<()> {
    writer.write_all(b"\"")?;
    for c in value.chars() {
        match c {
            '\t' => writer.write_all(b"\\t")?, '\r' => writer.write_all(b"\\r")?, '\n' => writer.write_all(b"\\n")?,
            '"' => writer.write_all(b"\\\"")?, '\\' => writer.write_all(b"\\\\")?,
            c => { let mut buf = [0; 4]; writer.write_all(c.encode_utf8(&mut buf).as_bytes())?; }
        }
    }
    writer.write_all(b"\"")
}
pub fn node_list_format(writer: &mut impl Write, ast: &AST, list: NodeList, mut leading_space: bool) -> io::Result<()> {
    let mut current = list.first;
    while let Some(id) = current {
        if leading_space { writer.write_all(b" ")?; }
        node_format(writer, ast, id)?;
        leading_space = true;
        current = ast.arena[id].next;
    }
    Ok(())
}
pub fn node_format(writer: &mut impl Write, ast: &AST, id: NodeID) -> io::Result<()> {
    use NodeValue::*;
    let node = ast.arena.get(id).ok_or_else(|| io::Error::new(io::ErrorKind::InvalidInput, "invalid node ID"))?;
    match &node.value {
        Integer(v) => write!(writer, "{v}"), Real(v) => write!(writer, "{v}"), Boolean(v) => write!(writer, "{v}"),
        String(v) => write_quoted_string(writer, v), Identifier(v) => write!(writer, "{v}"),
        ParserType(ty) => match ty {
            crate::lang::ParserType::Named(name) => write!(writer, "{name}"),
            crate::lang::ParserType::Slice(element) => { write!(writer, "[]")?; node_format(writer, ast, *element) }
            crate::lang::ParserType::Pointer(element) => { write!(writer, "^")?; node_format(writer, ast, *element) }
            crate::lang::ParserType::Array { element, length } => { write!(writer, "[{length}]")?; node_format(writer, ast, *element) }
        }
        Unary { op, operand } => {
            write!(writer, "({} ", token_type_name(*op))?; node_format(writer, ast, *operand)?; write!(writer, ")")
        }
        Binary { op, left, right } => {
            write!(writer, "({} ", token_type_name(*op))?; node_format(writer, ast, *left)?;
            write!(writer, " ")?; node_format(writer, ast, *right)?; write!(writer, ")")
        }
        Index { object, idx } => {
            write!(writer, "([] ")?; node_format(writer, ast, *object)?; write!(writer, " ")?;
            node_format(writer, ast, *idx)?; write!(writer, ")")
        }
        Call { callable, args } => {
            write!(writer, "(call ")?; node_format(writer, ast, *callable)?; node_list_format(writer, ast, *args, true)?; write!(writer, ")")
        }
        Field { identifier, ty } => { write!(writer, "(field {identifier} ")?; node_format(writer, ast, *ty)?; write!(writer, ")") }
        VarDefinition { idents, ty, values } => {
            write!(writer, "(var (")?; node_list_format(writer, ast, *idents, false)?; write!(writer, ") ")?;
            node_format(writer, ast, *ty)?; write!(writer, " (")?; node_list_format(writer, ast, *values, false)?; write!(writer, "))")
        }
        Assignment { left, right } => {
            write!(writer, "(= (")?; node_list_format(writer, ast, *left, false)?; write!(writer, ") (")?;
            node_list_format(writer, ast, *right, false)?; write!(writer, "))")
        }
        Block { statements } => { write!(writer, "(block")?; node_list_format(writer, ast, *statements, true)?; write!(writer, ")") }
        Return { values } => { write!(writer, "(return")?; node_list_format(writer, ast, *values, true)?; write!(writer, ")") }
        Break(label) | Continue(label) => {
            write!(writer, "({}", if matches!(node.value, Break(_)) { "break" } else { "continue" })?;
            if !label.is_empty() { write!(writer, " {label}")?; } write!(writer, ")")
        }
        If { condition, then_block, else_branch } => {
            write!(writer, "(if ")?; node_format(writer, ast, *condition)?; write!(writer, " ")?; node_format(writer, ast, *then_block)?;
            if let Some(id) = else_branch { write!(writer, " ")?; node_format(writer, ast, *id)?; } write!(writer, ")")
        }
        While { condition, body } => {
            write!(writer, "(while ")?; node_format(writer, ast, *condition)?; write!(writer, " ")?; node_format(writer, ast, *body)?; write!(writer, ")")
        }
        ProcDefinition { name, args, returns, body } => {
            write!(writer, "(proc {name} (")?; node_list_format(writer, ast, *args, false)?; write!(writer, ") (")?;
            node_list_format(writer, ast, *returns, false)?; write!(writer, ") ")?; node_format(writer, ast, *body)?; write!(writer, ")")
        }
    }
}

#[cfg(test)]
#[path = "parser_test.rs"]
mod parser_test;
