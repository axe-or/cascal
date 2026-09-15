use super::*;

#[test]
fn inline_to_heap_and_reserve() {
    let mut arr = sm_arr_init();
    sm_arr_push(&mut arr, 10);
    sm_arr_push(&mut arr, 20);
    assert!(matches!(arr, SmallArray::Inline { .. }));
    assert_eq!(sm_arr_slice(&arr), [10, 20]);
    sm_arr_reserve(&mut arr, 2).unwrap();
    assert!(matches!(arr, SmallArray::Inline { .. }));
    sm_arr_push(&mut arr, 30);
    assert!(matches!(arr, SmallArray::Heap(_)));
    sm_arr_reserve(&mut arr, 1024).unwrap();
    sm_arr_reserve(&mut arr, 1).unwrap();
    assert_eq!(sm_arr_slice(&arr), [10, 20, 30]);
    assert!(sm_arr_reserve(&mut arr, usize::MAX).is_err());
    assert_eq!(sm_arr_slice(&arr), [10, 20, 30]);
}
