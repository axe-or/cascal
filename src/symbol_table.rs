use crate::base::{Arena, Str};
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
    pub name: Str,
    pub kind: SymbolKind,
    pub ty: Option<TypeID>,
}

#[derive(Debug, Default)]
pub struct SymbolTable {
    pub symbols: Arena<Symbol, SymbolID>,
    pub syms: HashMap<Str, SymbolID>,
}

impl SymbolTable {
    pub fn get(&self, name: &str) -> Option<&Symbol> {
        self.syms.get(name).and_then(|id| self.symbols.get(*id))
    }

    /// Inserting an existing name replaces its definition while retaining its ID.
    pub fn insert(&mut self, symbol: Symbol) -> SymbolID {
        if let Some(&id) = self.syms.get(&symbol.name) {
            self.symbols[id] = symbol;
            return id;
        }
        let name = symbol.name.clone();
        let id = self.symbols.alloc(symbol);
        self.syms.insert(name, id);
        id
    }
}

#[cfg(test)]
#[path = "symbol_table_test.rs"]
mod symbol_table_test;
