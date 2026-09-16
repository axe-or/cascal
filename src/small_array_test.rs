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

#[test]
fn owned_values_and_storage_independent_equality() {
    let mut arr = SmallArray::<String>::default();
    assert!(arr.as_slice().is_empty());
    arr.push("first".into());
    arr.push("second".into());
    let mut heap = arr.clone();
    heap.try_reserve_capacity(16).unwrap();
    assert_eq!(arr, heap);
    arr.push("third".into());
    heap.push("third".into());
    assert_eq!(arr, heap);
    assert_eq!(arr.as_slice(), ["first", "second", "third"]);
}

#[test]
fn moves_and_drops_non_clone_non_default_values_once() {
    use std::{cell::Cell, rc::Rc};

    struct Tracked(Rc<Cell<usize>>);
    impl Drop for Tracked {
        fn drop(&mut self) {
            self.0.set(self.0.get() + 1);
        }
    }

    for count in 0..=4 {
        let drops = Rc::new(Cell::new(0));
        let mut arr = SmallArray::new();
        for _ in 0..count {
            arr.push(Tracked(drops.clone()));
        }
        assert_eq!(drops.get(), 0);
        assert_eq!(arr.as_slice().len(), count);
        assert!(arr.try_reserve_capacity(usize::MAX).is_err());
        assert_eq!(arr.as_slice().len(), count);
        assert_eq!(drops.get(), 0);
        drop(arr);
        assert_eq!(drops.get(), count);
    }
}

#[test]
fn explicit_reserve_moves_partial_inline_storage() {
    for count in 0..=2 {
        let mut arr = SmallArray::new();
        for n in 0..count {
            arr.push(Box::new(n));
        }
        arr.try_reserve_capacity(8).unwrap();
        assert!(matches!(arr, SmallArray::Heap(_)));
        assert_eq!(
            arr.as_slice().iter().map(|n| **n).collect::<Vec<_>>(),
            (0..count).collect::<Vec<_>>()
        );
    }
}
