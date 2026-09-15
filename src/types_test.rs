use super::*;

#[test]
fn interning_all_kinds_and_growth() {
    let mut arena = TypeArena::with_capacity(1);
    let int = arena.intern(Type::Primitive(PrimitiveType::Int));
    assert_eq!(int, arena.intern(Type::Primitive(PrimitiveType::Int)));
    for ty in [
        Type::Pointer { inner: int },
        Type::Slice { inner: int },
        Type::Array {
            inner: int,
            size: 4,
        },
        Type::Distinct {
            inner: int,
            name: "Index".to_owned(),
        },
    ] {
        let id = arena.intern(ty.clone());
        assert_eq!(id, arena.intern(ty.clone()));
        assert_eq!(arena.get(id), Some(&ty));
    }
    let mut previous = int;
    for _ in 0..10000 {
        previous = arena.intern(Type::Pointer { inner: previous });
    }
    assert_eq!(arena.get(int), Some(&Type::Primitive(PrimitiveType::Int)));
    assert_eq!(size_of::<TypeID>(), 4);
    assert_eq!(size_of::<Option<TypeID>>(), 4);
}

#[test]
fn collisions_are_resolved_by_equality() {
    let mut arena = TypeArena::with_capacity(1);
    let int = arena.intern(Type::Primitive(PrimitiveType::Int));
    // Find a real 32-bit hash collision rather than replace the hashing implementation.
    let mut seen = HashMap::new();
    let (a, b) = (0..300_000)
        .find_map(|n| {
            let ty = Type::Distinct {
                inner: int,
                name: format!("name{n}"),
            };
            let h = ty.hash();
            seen.insert(h, ty.clone()).map(|other| (other, ty))
        })
        .expect("collision in deterministic fixture");
    assert_ne!(a, b);
    let a_id = arena.intern(a.clone());
    let b_id = arena.intern(b.clone());
    assert_ne!(a_id, b_id);
    assert_eq!(arena.intern(a), a_id);
    assert_eq!(arena.intern(b), b_id);
}

#[test]
fn names_are_owned_and_lengths_matter() {
    let mut arena = TypeArena::default();
    let int = arena.intern(Type::Primitive(PrimitiveType::Int));
    let name = String::from("Index");
    let id = arena.intern(Type::Distinct {
        inner: int,
        name: name.clone(),
    });
    drop(name);
    assert_eq!(
        arena.types[id],
        Type::Distinct {
            inner: int,
            name: "Index".into()
        }
    );
    let other = arena.intern(Type::Distinct {
        inner: int,
        name: "Index2".into(),
    });
    assert_ne!(id, other);
}
