use std::cell::{Ref, RefCell, RefMut};
use std::fmt;
use std::num::NonZeroU32;

#[derive(Debug, PartialEq)]
enum Token {
    Symbol(String),
    String(String),
    Number(f64),
    ParenOpen,
    ParenClose,

    Fn,
    Do,
    If,
    While,
    /// `(let (name value ...) body...)`, with an implicit `do` body.
    Let,
}

struct Scanner {
    source: Vec<char>,
    current: usize,
}

impl Scanner {
    pub fn advance(&mut self) -> Option<Token> {
        while self
            .source
            .get(self.current)
            .is_some_and(|c| c.is_whitespace())
        {
            self.current += 1;
        }

        let c = *self.source.get(self.current)?;
        match c {
            '(' => {
                self.current += 1;
                Some(Token::ParenOpen)
            }
            ')' => {
                self.current += 1;
                Some(Token::ParenClose)
            }
            '"' => self.scan_string(),
            '0'..='9' | '+' | '-' | '.' => self.scan_number(),
            _ => self.scan_symbol(),
        }
    }

    fn scan_string(&mut self) -> Option<Token> {
        self.current += 1;
        let mut value = String::new();

        while let Some(&c) = self.source.get(self.current) {
            self.current += 1;
            match c {
                '"' => return Some(Token::String(value)),
                '\\' => {
                    let escaped = *self.source.get(self.current)?;
                    self.current += 1;
                    match escaped {
                        'n' => value.push('\n'),
                        'r' => value.push('\r'),
                        't' => value.push('\t'),
                        '"' => value.push('"'),
                        '\\' => value.push('\\'),
                        _ => {
                            value.push('\\');
                            value.push(escaped);
                        }
                    }
                }
                _ => value.push(c),
            }
        }

        // The Option API cannot distinguish an unterminated string from EOF.
        None
    }

    fn scan_number(&mut self) -> Option<Token> {
        let atom = self.scan_atom();
        Some(match atom.parse::<f64>() {
            Ok(value) => Token::Number(value),
            Err(_) => Token::Symbol(atom),
        })
    }

    fn scan_symbol(&mut self) -> Option<Token> {
        let atom = self.scan_atom();
        Some(match atom.as_str() {
            "fn" => Token::Fn,
            "do" => Token::Do,
            "if" => Token::If,
            "while" => Token::While,
            "let" => Token::Let,
            _ => Token::Symbol(atom),
        })
    }

    fn scan_atom(&mut self) -> String {
        let start = self.current;
        while let Some(&c) = self.source.get(self.current) {
            if c.is_whitespace() || matches!(c, '(' | ')' | '"') {
                break;
            }
            self.current += 1;
        }
        self.source[start..self.current].iter().collect()
    }
}

/// One-based index into its owning AST. Handles remain valid as the arena grows.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(transparent)]
struct NodeID(NonZeroU32);

#[derive(Debug, PartialEq)]
enum Node {
    Symbol(String),
    String(String),
    Number(f64),
    Call {
        callee: NodeID,
        args: Vec<NodeID>,
    },
    Fn {
        params: Vec<String>,
        body: NodeID,
    },
    Do(Vec<NodeID>),
    If {
        condition: NodeID,
        then_branch: NodeID,
        else_branch: Option<NodeID>,
    },
    While {
        condition: NodeID,
        body: NodeID,
    },
    Let {
        bindings: Vec<(String, NodeID)>,
        body: NodeID,
    },
}

/// Append-only arena. IDs belong to this AST and must not be used in another.
#[derive(Debug, Default)]
struct AST {
    nodes: Vec<RefCell<Node>>,
    roots: Vec<NodeID>,
}

impl AST {
    fn alloc(&mut self, node: Node) -> NodeID {
        let index = u32::try_from(self.nodes.len())
            .ok()
            .and_then(|n| n.checked_add(1))
            .and_then(NonZeroU32::new)
            .expect("AST arena exhausted its NodeID space");
        self.nodes.push(RefCell::new(node));
        NodeID(index)
    }

    fn get(&self, id: NodeID) -> Ref<'_, Node> {
        self.nodes[id.0.get() as usize - 1].borrow()
    }

    /// Mutates through a shared arena reference. Conflicting live borrows panic.
    fn get_mut(&self, id: NodeID) -> RefMut<'_, Node> {
        self.nodes[id.0.get() as usize - 1].borrow_mut()
    }

    fn parse(source: &str) -> Result<Self, ParseError> {
        let mut scanner = Scanner {
            source: source.chars().collect(),
            current: 0,
        };
        let mut tokens = Vec::new();
        loop {
            let has_input = scanner.source[scanner.current..]
                .iter()
                .any(|c| !c.is_whitespace());
            match scanner.advance() {
                Some(token) => tokens.push(token),
                None if has_input => return Err(ParseError("unterminated string".into())),
                None => break,
            }
        }
        Parser {
            tokens: tokens.into_iter().peekable(),
            ast: AST::default(),
        }
        .parse()
    }
}

impl fmt::Display for AST {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for (index, &root) in self.roots.iter().enumerate() {
            if index != 0 {
                f.write_str("\n")?;
            }
            self.fmt_node(root, f)?;
        }
        Ok(())
    }
}

impl AST {
    fn fmt_node(&self, id: NodeID, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &*self.get(id) {
            Node::Symbol(value) => f.write_str(value),
            Node::String(value) => {
                f.write_str("\"")?;
                for c in value.chars() {
                    match c {
                        '"' => f.write_str("\\\"")?,
                        '\\' => f.write_str("\\\\")?,
                        '\n' => f.write_str("\\n")?,
                        '\r' => f.write_str("\\r")?,
                        '\t' => f.write_str("\\t")?,
                        _ => write!(f, "{c}")?,
                    }
                }
                f.write_str("\"")
            }
            // A leading sign routes these spellings through the number scanner.
            Node::Number(value) if value.is_nan() => f.write_str("+NaN"),
            Node::Number(value) if *value == f64::INFINITY => f.write_str("+inf"),
            Node::Number(value) => write!(f, "{value}"),
            Node::Call { callee, args } => {
                f.write_str("(")?;
                self.fmt_node(*callee, f)?;
                self.fmt_tail(args, f)?;
                f.write_str(")")
            }
            Node::Fn { params, body } => {
                f.write_str("(fn (")?;
                for (index, param) in params.iter().enumerate() {
                    if index != 0 {
                        f.write_str(" ")?;
                    }
                    f.write_str(param)?;
                }
                f.write_str(")")?;
                self.fmt_body(*body, f)?;
                f.write_str(")")
            }
            Node::Do(nodes) => {
                f.write_str("(do")?;
                self.fmt_tail(nodes, f)?;
                f.write_str(")")
            }
            Node::If {
                condition,
                then_branch,
                else_branch,
            } => {
                f.write_str("(if")?;
                self.fmt_tail(&[*condition, *then_branch], f)?;
                if let Some(branch) = else_branch {
                    self.fmt_tail(&[*branch], f)?;
                }
                f.write_str(")")
            }
            Node::While { condition, body } => {
                f.write_str("(while ")?;
                self.fmt_node(*condition, f)?;
                self.fmt_body(*body, f)?;
                f.write_str(")")
            }
            Node::Let { bindings, body } => {
                f.write_str("(let (")?;
                for (index, (name, value)) in bindings.iter().enumerate() {
                    if index != 0 {
                        f.write_str(" ")?;
                    }
                    write!(f, "{name} ")?;
                    self.fmt_node(*value, f)?;
                }
                f.write_str(")")?;
                self.fmt_body(*body, f)?;
                f.write_str(")")
            }
        }
    }

    fn fmt_tail(&self, nodes: &[NodeID], f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for &node in nodes {
            f.write_str(" ")?;
            self.fmt_node(node, f)?;
        }
        Ok(())
    }

    fn fmt_body(&self, body: NodeID, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        // Omit only the implicit body wrapper, preserving explicit nested `do`s.
        match &*self.get(body) {
            Node::Do(nodes) => self.fmt_tail(nodes, f),
            _ => self.fmt_tail(&[body], f),
        }
    }
}

#[derive(Debug, PartialEq)]
struct ParseError(String);

impl std::fmt::Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}

impl std::error::Error for ParseError {}

struct Parser {
    tokens: std::iter::Peekable<std::vec::IntoIter<Token>>,
    ast: AST,
}

impl Parser {
    fn parse(mut self) -> Result<AST, ParseError> {
        while self.tokens.peek().is_some() {
            let root = self.expression()?;
            self.ast.roots.push(root);
        }
        Ok(self.ast)
    }

    fn expect(&mut self, expected: Token) -> Result<(), ParseError> {
        let actual = self.tokens.next();
        if actual == Some(expected) {
            Ok(())
        } else {
            Err(ParseError(format!("unexpected token: {actual:?}")))
        }
    }

    fn symbol(&mut self) -> Result<String, ParseError> {
        match self.tokens.next() {
            Some(Token::Symbol(name)) => Ok(name),
            actual => Err(ParseError(format!("expected a name, got {actual:?}"))),
        }
    }

    fn expression(&mut self) -> Result<NodeID, ParseError> {
        let node = match self.tokens.next() {
            Some(Token::Symbol(value)) => Node::Symbol(value),
            Some(Token::String(value)) => Node::String(value),
            Some(Token::Number(value)) => Node::Number(value),
            Some(Token::ParenOpen) => return self.list(),
            actual => {
                return Err(ParseError(format!(
                    "expected an expression, got {actual:?}"
                )));
            }
        };
        Ok(self.ast.alloc(node))
    }

    /// Reads expressions through the closing parenthesis, consuming it.
    fn expressions(&mut self) -> Result<Vec<NodeID>, ParseError> {
        let mut nodes = Vec::new();
        loop {
            match self.tokens.peek() {
                Some(Token::ParenClose) => {
                    self.tokens.next();
                    return Ok(nodes);
                }
                None => return Err(ParseError("expected ')' before end of input".into())),
                _ => nodes.push(self.expression()?),
            }
        }
    }

    fn body(&mut self) -> Result<NodeID, ParseError> {
        let expressions = self.expressions()?;
        Ok(self.ast.alloc(Node::Do(expressions)))
    }

    fn list(&mut self) -> Result<NodeID, ParseError> {
        let node = match self.tokens.peek() {
            Some(Token::Fn) => {
                self.tokens.next();
                self.expect(Token::ParenOpen)?;
                let mut params = Vec::new();
                while self.tokens.peek() != Some(&Token::ParenClose) {
                    params.push(self.symbol()?);
                }
                self.expect(Token::ParenClose)?;
                let body = self.body()?;
                Node::Fn { params, body }
            }
            Some(Token::Do) => {
                self.tokens.next();
                return self.body();
            }
            Some(Token::If) => {
                self.tokens.next();
                let condition = self.expression()?;
                let then_branch = self.expression()?;
                let else_branch = if self.tokens.peek() == Some(&Token::ParenClose) {
                    None
                } else {
                    Some(self.expression()?)
                };
                self.expect(Token::ParenClose)?;
                Node::If {
                    condition,
                    then_branch,
                    else_branch,
                }
            }
            Some(Token::While) => {
                self.tokens.next();
                let condition = self.expression()?;
                let body = self.body()?;
                Node::While { condition, body }
            }
            Some(Token::Let) => {
                self.tokens.next();
                self.expect(Token::ParenOpen)?;
                let mut bindings = Vec::new();
                while self.tokens.peek() != Some(&Token::ParenClose) {
                    let name = self.symbol()?;
                    let value = self.expression()?;
                    bindings.push((name, value));
                }
                self.expect(Token::ParenClose)?;
                let body = self.body()?;
                Node::Let { bindings, body }
            }
            _ => {
                let callee = self.expression()?;
                let args = self.expressions()?;
                Node::Call { callee, args }
            }
        };
        Ok(self.ast.alloc(node))
    }
}

fn main() {
    println!("Hello, world!");
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scanner(source: &str) -> Scanner {
        Scanner {
            source: source.chars().collect(),
            current: 0,
        }
    }

    fn scan(source: &str) -> Vec<Token> {
        let mut scanner = scanner(source);
        std::iter::from_fn(|| scanner.advance()).collect()
    }

    #[test]
    fn scans_nested_expressions_and_whitespace() {
        assert_eq!(
            scan(" \t(define π(+ 1 2))\n\u{2003}"),
            vec![
                Token::ParenOpen,
                Token::Symbol("define".into()),
                Token::Symbol("π".into()),
                Token::ParenOpen,
                Token::Symbol("+".into()),
                Token::Number(1.0),
                Token::Number(2.0),
                Token::ParenClose,
                Token::ParenClose,
            ]
        );
    }

    #[test]
    fn scans_numbers_and_preserves_non_numeric_atoms() {
        assert_eq!(
            scan("0 -12 +3.5 .25 1e-3 - + 12abc 1.2.3"),
            vec![
                Token::Number(0.0),
                Token::Number(-12.0),
                Token::Number(3.5),
                Token::Number(0.25),
                Token::Number(0.001),
                Token::Symbol("-".into()),
                Token::Symbol("+".into()),
                Token::Symbol("12abc".into()),
                Token::Symbol("1.2.3".into()),
            ]
        );
    }

    #[test]
    fn scans_strings_and_escapes() {
        assert_eq!(
            scan(r#""""hé (there)\n\r\t\"\\\q"next"#),
            vec![
                Token::String(String::new()),
                Token::String("hé (there)\n\r\t\"\\\\q".into()),
                Token::Symbol("next".into()),
            ]
        );
    }

    #[test]
    fn handles_empty_input_and_repeated_eof() {
        for input in ["", " \n\t", "\"unterminated", "\"trailing\\"] {
            let mut scanner = scanner(input);
            assert_eq!(scanner.advance(), None);
            assert_eq!(scanner.current, scanner.source.len());
            assert_eq!(scanner.advance(), None);
        }
    }
}

#[cfg(test)]
mod parser_tests {
    use super::*;

    #[test]
    fn parses_literals_calls_and_multiple_roots() {
        let ast = AST::parse("42 \"hello\" ((fn (x) x) 3)").unwrap();
        assert_eq!(ast.roots.len(), 3);
        assert_eq!(*ast.get(ast.roots[0]), Node::Number(42.0));
        assert_eq!(*ast.get(ast.roots[1]), Node::String("hello".into()));
        let call = ast.get(ast.roots[2]);
        let Node::Call { callee, args } = &*call else {
            panic!("expected call")
        };
        assert_eq!(args.len(), 1);
        assert_eq!(*ast.get(args[0]), Node::Number(3.0));
        let function = ast.get(*callee);
        let Node::Fn { params, body } = &*function else {
            panic!("expected function")
        };
        assert_eq!(params, &["x"]);
        let body = ast.get(*body);
        let Node::Do(expressions) = &*body else {
            panic!("expected body")
        };
        assert_eq!(expressions.len(), 1);
        assert_eq!(*ast.get(expressions[0]), Node::Symbol("x".into()));
    }

    #[test]
    fn parses_flat_bindings_and_control_flow() {
        let ast = AST::parse("(let (x 1 b 2 y 9) (while x (do (f b) y)) (if b x y))").unwrap();
        let root = ast.get(ast.roots[0]);
        let Node::Let { bindings, body } = &*root else {
            panic!("expected let")
        };
        assert_eq!(
            bindings
                .iter()
                .map(|(name, _)| name.as_str())
                .collect::<Vec<_>>(),
            ["x", "b", "y"]
        );
        for ((_, value), expected) in bindings.iter().zip([1.0, 2.0, 9.0]) {
            assert_eq!(*ast.get(*value), Node::Number(expected));
        }
        let body = ast.get(*body);
        let Node::Do(expressions) = &*body else {
            panic!("expected body")
        };
        assert_eq!(expressions.len(), 2);
        let loop_node = ast.get(expressions[0]);
        let Node::While { condition, body } = &*loop_node else {
            panic!("expected while")
        };
        assert_eq!(*ast.get(*condition), Node::Symbol("x".into()));
        let loop_body = ast.get(*body);
        let Node::Do(loop_expressions) = &*loop_body else {
            panic!("expected loop body")
        };
        assert_eq!(loop_expressions.len(), 1);
        assert!(matches!(&*ast.get(loop_expressions[0]), Node::Do(nodes) if nodes.len() == 2));
        let conditional = ast.get(expressions[1]);
        let Node::If {
            condition,
            then_branch,
            else_branch,
        } = &*conditional
        else {
            panic!("expected if")
        };
        assert_eq!(*ast.get(*condition), Node::Symbol("b".into()));
        assert_eq!(*ast.get(*then_branch), Node::Symbol("x".into()));
        assert_eq!(*ast.get(else_branch.unwrap()), Node::Symbol("y".into()));
    }

    #[test]
    fn handles_empty_bodies_and_optional_else() {
        assert!(AST::parse("  ").unwrap().roots.is_empty());
        let ast = AST::parse("(do) (fn ()) (let ()) (while 1) (if 1 2)").unwrap();
        assert_eq!(ast.roots.len(), 5);
        assert_eq!(*ast.get(ast.roots[0]), Node::Do(vec![]));
        assert!(matches!(
            *ast.get(ast.roots[4]),
            Node::If {
                else_branch: None,
                ..
            }
        ));
    }

    #[test]
    fn rejects_malformed_input() {
        for source in [
            "(",
            ")",
            "()",
            "(f",
            "(do",
            "fn",
            "(fn x x)",
            "(fn (1) x)",
            "(fn (x",
            "(let (x) x)",
            "(let (1 2) x)",
            "(let x x)",
            "(let (x 1",
            "(if)",
            "(if 1)",
            "(if 1 2 3 4)",
            "(while)",
            "\"unterminated",
            "1 \"unterminated",
            "(f \"trailing\\",
        ] {
            assert!(AST::parse(source).is_err(), "accepted {source:?}");
        }
    }

    #[test]
    fn handles_aliasing_mutation_and_arena_growth() {
        assert_eq!(std::mem::size_of::<NodeID>(), 4);
        assert_eq!(std::mem::size_of::<Option<NodeID>>(), 4);
        let mut ast = AST::default();
        let id = ast.alloc(Node::Number(1.0));
        let parent = ast.alloc(Node::Do(vec![id, id]));
        for _ in 0..1000 {
            ast.alloc(Node::Number(0.0));
        }
        let shared = &ast;
        let parent = shared.get(parent);
        let Node::Do(children) = &*parent else {
            panic!("expected do")
        };
        *shared.get_mut(children[0]) = Node::Number(2.0);
        assert_eq!(*shared.get(children[1]), Node::Number(2.0));
        assert_eq!(*shared.get(id), Node::Number(2.0));
    }
}

#[cfg(test)]
mod display_tests {
    use super::*;

    #[test]
    fn canonical_forms_round_trip_without_extra_body_wrappers() {
        for (source, expected) in [
            (" \n ", ""),
            ("+3.00  .25 -0", "3\n0.25\n-0"),
            (
                "( let (x 1 b 2 y 9)\n(f x) y )",
                "(let (x 1 b 2 y 9) (f x) y)",
            ),
            ("((fn (x y) (do x y) y) 1 2)", "((fn (x y) (do x y) y) 1 2)"),
            (
                "(while x (if y (f) (do (g) x)) (if x y))",
                "(while x (if y (f) (do (g) x)) (if x y))",
            ),
            (
                "(do) (fn ()) (let ()) (while 1)",
                "(do)\n(fn ())\n(let ())\n(while 1)",
            ),
            (r#""hé\n\r\t\"\\\q""#, r#""hé\n\r\t\"\\\\q""#),
        ] {
            let ast = AST::parse(source).unwrap();
            let printed = ast.to_string();
            assert_eq!(printed, expected);
            let reparsed = AST::parse(&printed).unwrap();
            assert_eq!(ast.roots, reparsed.roots);
            assert_eq!(ast.nodes, reparsed.nodes);
            assert_eq!(reparsed.to_string(), printed);
        }
    }

    #[test]
    fn prints_shared_nodes_after_mutation() {
        let mut ast = AST::default();
        let child = ast.alloc(Node::Number(1.0));
        let root = ast.alloc(Node::Do(vec![child, child]));
        ast.roots.push(root);
        *ast.get_mut(child) = Node::String("new".into());
        assert_eq!(ast.to_string(), r#"(do "new" "new")"#);
    }

    #[test]
    fn non_finite_numbers_remain_numbers() {
        let ast = AST::parse("1e999 -1e999 +NaN").unwrap();
        assert_eq!(ast.to_string(), "+inf\n-inf\n+NaN");
        let reparsed = AST::parse(&ast.to_string()).unwrap();
        for (id, expected) in
            reparsed
                .roots
                .iter()
                .zip([f64::INFINITY, f64::NEG_INFINITY, f64::NAN])
        {
            let node = reparsed.get(*id);
            let Node::Number(value) = *node else {
                panic!("expected number")
            };
            assert!(value == expected || (value.is_nan() && expected.is_nan()));
        }
    }
}
