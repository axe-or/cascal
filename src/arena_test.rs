use super::*;
crate::def_arena_handle!(TestID);

#[test]
fn arena_handles_survive_growth() {
    let mut arena = Arena::<_, TestID>::with_capacity(1);
    let first = arena.alloc(String::from("first"));
    for _ in 0..4096 {
        arena.alloc(String::from("later"));
    }
    assert_eq!(arena[first], "first");
    arena[first].push('!');
    assert_eq!(arena[first], "first!");
    assert_eq!(size_of::<TestID>(), 4);
    assert_eq!(size_of::<Option<TestID>>(), 4);
}
