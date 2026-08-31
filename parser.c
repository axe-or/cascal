#include "base.h"
#include "lang.h"

typedef struct {
    Scanner scanner;
} Parser;

typedef struct {
    u32 v;
} Node_ID;

#define NODE_AST_POOL_BITS 5ull
#define NODE_AST_POOL_OFFSET_BITS (32ull - NODE_AST_POOL_BITS)

typedef struct {
    Node_ID left;
    Node_ID right;
    Token_Type operator;
} Node_Binary;

typedef struct {
    union {
        Node_Binary binary;
    } as;
} Node;

#define NODE_POOL_SIZE 1024ull

_Static_assert(NODE_POOL_SIZE < (1ull << NODE_AST_POOL_OFFSET_BITS), "node pool size is too big");

typedef struct {
    Node nodes[NODE_POOL_SIZE];
    i32 usage;
} Node_Pool;

#define AST_NODE_POOL_COUNT (1 << NODE_AST_POOL_BITS)

typedef struct {
    Node_Pool* pools[AST_NODE_POOL_COUNT];
    u32 active_pool_count;
    Arena* arena;
} AST;

void ast_init(AST* ast, Arena* arena){
    mem_zero(ast, sizeof(*ast));
    ast->arena = arena;
}

static inline
i32 ast_pool_put(Node_Pool* p, Node node){
    if(p->usage >= NODE_POOL_SIZE){
        return -1;
    }

    p->nodes[p->usage] = node;
    p->usage += 1;
    return p->usage - 1;
}

static inline
Node_Pool* ast_push_pool(AST* ast){
    if(ast->active_pool_count >= AST_NODE_POOL_COUNT){
        return NULL;
    }

    Node_Pool* np = arena_make(ast->arena, Node_Pool, 1);
    if(np == NULL){
        return NULL;
    }

    ast->pools[ast->active_pool_count - 1] = np;
    ast->active_pool_count += 1;
    return np;
}

Node_ID ast_put(AST* ast, Node node){
    Node_Pool* pool = ast->pools[ast->active_pool_count - 1];
    if(!pool){
        pool = ast_push_pool(ast);
        ensure(pool != NULL, "Exhausted AST pools");
    }

    i32 off = ast_pool_put(pool, node);
    if(off < 0){
        pool = ast_push_pool(ast);
        ensure(pool != NULL, "Exhausted AST pools");
        off = ast_pool_put(pool, node);
    }

    return (Node_ID){ off + (NODE_POOL_SIZE * ast->active_pool_count) };
}

Node* ast_get(AST* ast, Node_ID id){
    u32 pool_idx = id.v >> NODE_AST_POOL_OFFSET_BITS;
    ensure(pool_idx < ast->active_pool_count && ast->pools[pool_idx] , "pool not active or out of bounds");

    Node_Pool* pool = ast->pools[pool_idx];

    u32 offset = (id.v << NODE_AST_POOL_BITS) >> NODE_AST_POOL_OFFSET_BITS;
    ensure(offset < pool->usage, "out of bounds node offset");

    return &pool->nodes[offset];
}

_Static_assert((NODE_POOL_SIZE & (NODE_POOL_SIZE - 1)) == 0, "Node pool size must be a power of 2");

