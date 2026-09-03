#include "lang.h"

enum Symbol_Kind {
    Sym_Unknown = 0,

    Sym_Var,
    Sym_Const,
    Sym_Proc,
    Sym_Type,

    Symbol_Kind__COUNT,
} ;

typedef struct {
    String name;
    u8 kind;
    Type* type; // TODO: Replace with type ID
} Symbol;

typedef struct Sym_Table Sym_Table;

typedef struct {
    String key;
    u32 hash;
    Symbol sym;
} Sym_Table_Slot;

struct Sym_Table {
    Sym_Table_Slot* slots;
    isize slot_count;
    Arena* arena;
};

Symbol* symtbl_get_by_name(Sym_Table* s, String s);

// Symbol* sym_find(Sym_Table* s);