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

} Symbol;

typedef struct {
} Sym_Table;