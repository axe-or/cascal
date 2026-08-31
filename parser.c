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
            rune r = escape_sequence(s.v[i]);
            if(r > 0){
                sb_write_rune(&sb, r);
            }
        }
        sb_write_byte(&sb, s.v[i]);
    }
}

static inline
Node* ast_make_string(AST* ast, String escaped){
    panic("TODO");
}
