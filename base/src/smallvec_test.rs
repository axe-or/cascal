use crate::{smallvec, SmallVec};

#[test]
fn exported_vector_spills_and_returns_inline() {
    let mut values: SmallVec<[String; 2]> = smallvec!["first".into(), "second".into()];
    assert!(!values.spilled());
    values.push("third".into());
    assert!(values.spilled());
    assert_eq!(values.as_slice(), ["first", "second", "third"]);
    values.pop();
    values.shrink_to_fit();
    assert!(!values.spilled());
    assert_eq!(values.as_slice(), ["first", "second"]);
}

#[test]
fn arbitrary_inline_capacity() {
    let mut values = SmallVec::<[u8; 7]>::new();
    values.extend(0..7);
    assert!(!values.spilled());
    values.push(7);
    assert!(values.spilled());
}
