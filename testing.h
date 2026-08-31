#pragma once

extern int printf(char const* fmt, ...);

struct Test {
    char const* name;
    int total;
    int fails;
};

static inline
bool test_ex(struct Test* t, bool predicate, char const* msg, char const* file, int line){
    t->total += 1;
    if(!predicate){
        t->fails += 1;
        printf("%s:%d fail: %s\n", file, line, msg);
    }
    return predicate;
}

#define t_pred(t, pred) test_ex((t), (pred), #pred, __FILE__, __LINE__)

#define t_ensure(t, pred, msg) test_ex((t), (pred), (msg), __FILE__, __LINE__)

typedef void (TestFunc)(struct Test* t);

static inline
bool test_run(TestFunc* f){
    Test t = {0};
    f(&t);
    printf("%s %s: ok in %d/%d\n", t.fails ? "FAIL" : "PASS", t.name ? "<unnamed>" : t.name, t.total - t.fails, t.total);
    return t.fails == 0;
}
