use super::*;

#[test]
fn inline_to_heap_and_reserve() {
    let mut arr = SmallArray::new();
    arr.push(10);
    arr.push(20);
    assert!(matches!(arr, SmallArray::Inline { .. }));
    assert_eq!(arr.as_slice(), [10, 20]);
    arr.try_reserve_capacity(2).unwrap();
    assert!(matches!(arr, SmallArray::Inline { .. }));
    arr.push(30);
    assert!(matches!(arr, SmallArray::Heap(_)));
    arr.try_reserve_capacity(1024).unwrap();
    arr.try_reserve_capacity(1).unwrap();
    assert_eq!(arr.as_slice(), [10, 20, 30]);
    assert!(arr.try_reserve_capacity(usize::MAX).is_err());
    assert_eq!(arr.as_slice(), [10, 20, 30]);
}
