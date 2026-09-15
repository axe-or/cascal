use super::*;
use crate::types::*;

#[test]
fn lookup_replace_and_grow() {
    let mut table = SymbolTable::default();
    let mut types = TypeArena::default();
    let ty = types.intern(Type::Primitive(PrimitiveType::Int));
    let id = table.insert(Symbol {
        name: "x".into(),
        kind: SymbolKind::Var,
        ty: Some(ty),
    });
    for n in 0..1000 {
        table.insert(Symbol {
            name: format!("s{n}"),
            kind: SymbolKind::Proc,
            ty: None,
        });
    }
    assert_eq!(table.get("x").unwrap().ty, Some(ty));
    assert!(table.get("absent").is_none());
    let new_id = table.insert(Symbol {
        name: "x".into(),
        kind: SymbolKind::Const,
        ty: Some(ty),
    });
    assert_eq!(id, new_id);
    assert_eq!(table.symbols[id].kind, SymbolKind::Const);
    assert_eq!(table.symbols.len(), 1001);
    assert_eq!(size_of::<Option<SymbolID>>(), 4);
}
