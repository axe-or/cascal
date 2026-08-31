#include "base.h"
#include "lang.h"

static inline
Node* ast_make_node(AST* ast){
    return arena_make(ast->arena, Node, 1);
}

static inline
Node* ast_make_unary(AST* ast, Token_Type op, Node* operand){
    Node* node = arena_make(ast->arena, Node, 1);
    *node = (Node){
        .type = Node_Unary,
        .value.unary = {
            .op = op,
            .operand = operand,
        }
    };

    operand->parent = node;

    return node;
}

static inline
Node* ast_make_binary(AST* ast, Token_Type op, Node* left, Node* right){
    Node* node = arena_make(ast->arena, Node, 1);
    *node = (Node){
        .type = Node_Binary,
        .value.binary = {
            .op = op,
            .left = left,
            .right = right,
        }
    };

    left->parent = node;
    right->parent = node;

    return node;
}

static inline
Node* ast_make_integer(AST* ast, i64 value){
    Node* node = arena_make(ast->arena, Node, 1);
    *node = (Node){
        .type = Node_Integer,
        .value.integer = value,
    };

    return node;
}

static inline
Node* ast_make_real(AST* ast, f64 value){
    Node* node = arena_make(ast->arena, Node, 1);
    *node = (Node){
        .type = Node_Real,
        .value.real = value,
    };

    return node;
}

static inline
String unescape_sequences_in_string(String s, Arena* a){
    String_Builder sb = sb_make(s.len + 1, a);

    for(usize i = 0; i < s.len; i += 1){
        if(s.v[i] == '\\'){
            i += 1;
            if(i >= s.len){ break; }

            rune r = escape_sequence(s.v[i]);
            if(r < 0){ continue; }
            sb_write_rune(&sb, r);
        }
        else {
            sb_write_byte(&sb, s.v[i]);
        }
    }

    return sb_get(&sb);
}

static inline
Node* ast_make_string(AST* ast, String escaped){
    panic("TODO");
}


//  7           *   /   %   %%   &   &~  <<   >>
//  6           +   -   |   ~    in  not_in
//  5           ==  !=  <   >    <=  >=
//  4           &&
//  3           ||
//  2           ..=    ..<
//  1           or_else     ?    if  when
 
static inline
bool infix_binding_power(Token_Type t, int* lbp, int* rbp){
    switch (t) {
    // case Tk_SquareOpen:
    // case Tk_ParenOpen:
    // case Tk_Dot:

    case Tk_Star:
    case Tk_Slash:
    case Tk_Modulo:
    case Tk_And:
    case Tk_ShiftLeft:
    case Tk_ShiftRight:
        *lbp = 70; *rbp = 71;

    case Tk_Plus:
    case Tk_Minus:
    case Tk_Or:
    case Tk_Tilde:
        *lbp = 60; *rbp = 61;

    case Tk_Eq:
    case Tk_Neq:
    case Tk_Gt:
    case Tk_GtEq:
    case Tk_Lt:
    case Tk_LtEq:
        *lbp = 50; *rbp = 51;


    case Tk_LogicAnd:
        *lbp = 40; *rbp = 41;

    case Tk_LogicOr:
        *lbp = 30; *rbp = 31;

    default: return false;
    }

    return true;
}

static inline
bool prefix_binding_power(Token_Type t, int* bp){
    switch (t) {
    case Tk_Plus:
    case Tk_Tilde:
    case Tk_Minus:
        *bp = 100;

    default: return false;
    }

    return true;
}
