use crate::base::Arena;
use crate::types::TypeID;
use std::collections::HashMap;

crate::arena_id!(SymbolID);

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum SymbolKind {
    Var,
    Const,
    Proc,
    Type,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Symbol {
    pub name: String,
    pub kind: SymbolKind,
    pub ty: Option<TypeID>,
}

#[derive(Debug, Default)]
pub struct SymbolTable {
    pub symbols: Arena<Symbol, SymbolID>,
    pub syms: HashMap<String, SymbolID>,
}

pub fn symbol_table_get<'a>(table: &'a SymbolTable, name: &str) -> Option<&'a Symbol> {
    table.syms.get(name).and_then(|id| table.symbols.get(*id))
}

/// Inserting an existing name replaces its definition while retaining its ID.
pub fn symbol_table_insert(table: &mut SymbolTable, symbol: Symbol) -> SymbolID {
    if let Some(&id) = table.syms.get(&symbol.name) {
        table.symbols[id] = symbol;
        return id;
    }
    let name = symbol.name.clone();
    let id = table.symbols.alloc(symbol);
    table.syms.insert(name, id);
    id
}

#[cfg(test)]
#[path = "symbol_table_test.rs"]
mod symbol_table_test;
