use super::*;

fn formatted(ast: &AST, id: NodeID) -> String {
    let mut output = Vec::new();
    node_format(&mut output, ast, id).unwrap();
    String::from_utf8(output).unwrap()
}

#[test]
fn expression_precedence_and_postfix() {
    for (source, expected) in [
        ("1 + 2 * -value", "(+ 1 (* 2 (- value)))"),
        ("10 - 3 - 2", "(- (- 10 3) 2)"),
        ("1 + 2 << 3 & 4", "(+ 1 (& (<< 2 3) 4))"),
        ("-value.field * (2 + 3)", "(* (- (. value field)) (+ 2 3))"),
        ("not false or true", "(or (not false) true)"),
        ("items(1, value,)[2]", "([] (call items 1 value) 2)"),
        ("\"line\\n\\\"quoted\\\"\\\\😀\"", "\"line\\n\\\"quoted\\\"\\\\😀\""),
    ] {
        let mut p = parser_make(source.as_bytes());
        let id = parse_expression(&mut p).unwrap();
        assert_eq!(formatted(&p.ast, id), expected);
        assert_eq!(parser_peek(&p).unwrap().kind, TokenType::EndOfFile);
        if let NodeValue::Binary { left, right, .. } = p.ast.arena[id].value {
            assert_eq!(p.ast.arena[left].parent, Some(id));
            assert_eq!(p.ast.arena[right].parent, Some(id));
        }
    }
}

#[test]
fn compound_types_and_bounds() {
    let mut p = parser_make(b"[32][]^Item");
    let id = parse_type(&mut p).unwrap();
    assert_eq!(formatted(&p.ast, id), "[32][]^Item");
    let mut p = parser_make(b"[2147483648]Int");
    let error = parse_type(&mut p).unwrap_err();
    assert_eq!((error.kind, error.offset), (ErrorType::InvalidNumber, 1));
    assert_eq!(size_of::<Option<NodeID>>(), 4);
}

#[test]
fn complete_program() {
    let source = b"proc foo(a,b: []^int, c: int, d: bool,) -> (int, bool) {
        var x,y: int = 1,2;
        x,y = y,x;
        while x < 10 { if d { break outer; } else if false { continue; } else { foo(); } }
        return x,d;
        proc nested(,) { return; }
    } proc bar() -> ^int {}";
    let ast = parse(source).unwrap();
    let first = ast.root.unwrap();
    assert_eq!(formatted(&ast, first), "(proc foo ((field a []^int) (field b []^int) (field c int) (field d bool)) (int bool) (block (var (x y) int (1 2)) (= (x y) (y x)) (while (< x 10) (block (if d (block (break outer)) (if false (block (continue)) (block (call foo)))))) (return x d) (proc nested () () (block (return)))))");
    let second = ast.arena[first].next.unwrap();
    assert_eq!(formatted(&ast, second), "(proc bar () (^int) (block))");
    assert!(ast.arena[second].next.is_none());
}

#[test]
fn rejects_bad_programs() {
    for (source, kind) in [
        ("var value: int = 1;", ErrorType::UnexpectedToken),
        ("proc p(){ var a,b: int = 1; }", ErrorType::MismatchedListCardinality),
        ("proc p(){ a,b = 1; }", ErrorType::MismatchedListCardinality),
        ("proc p(){ a = 1,; }", ErrorType::UnexpectedToken),
        ("proc p(){ return 1 }", ErrorType::UnexpectedToken),
        ("proc p(){", ErrorType::UnexpectedToken),
        ("proc p(){ 1 + ; }", ErrorType::UnexpectedToken),
        ("proc p(){ \"\\q\"; }", ErrorType::InvalidEscapeSequence),
        ("proc p(){} /*", ErrorType::UnclosedComment),
    ] { assert_eq!(parse(source.as_bytes()).unwrap_err().kind, kind, "{source}"); }
    assert!(parse(b"").unwrap().root.is_none());
}

#[test]
fn writer_errors_propagate() {
    let ast = parse(b"proc p(){}").unwrap();
    let mut output = [0u8; 3];
    assert_eq!(node_format(&mut output.as_mut_slice(), &ast, ast.root.unwrap()).unwrap_err().kind(), io::ErrorKind::WriteZero);
}

#[test]
fn arena_growth_and_lists() {
    let source = format!("proc p(){{ {} }}", "a = 1 + 2;".repeat(5000));
    let ast = parse(source.as_bytes()).unwrap();
    let NodeValue::ProcDefinition { body, .. } = ast.arena[ast.root.unwrap()].value else { panic!() };
    let NodeValue::Block { statements } = ast.arena[body].value else { panic!() };
    assert_eq!(node_list_cardinality(&ast, statements), 5000);
    assert_eq!(ast.arena[statements.last.unwrap()].parent, Some(body));
}
