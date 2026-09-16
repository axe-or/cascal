use super::*;

#[test]
fn procedure_types_preserve_signature_and_ignore_argument_storage() {
    let mut arena = TypeArena::default();
    let int = arena.intern(Type::Primitive(PrimitiveType::Int));
    let real = arena.intern(Type::Primitive(PrimitiveType::Real));
    let mut args = SmallArray::new();
    args.push(int);
    args.push(real);
    let ty = Type::Proc {
        args: args.clone(),
        returns: Some(int),
    };
    let id = arena.intern(ty.clone());
    args.try_reserve_capacity(8).unwrap();
    let heap = Type::Proc {
        args,
        returns: Some(int),
    };
    assert_eq!(ty.hash(), heap.hash());
    assert_eq!(id, arena.intern(heap));
    let mut reversed = SmallArray::new();
    reversed.push(real);
    reversed.push(int);
    assert_ne!(
        id,
        arena.intern(Type::Proc {
            args: reversed,
            returns: Some(int)
        })
    );
    let Type::Proc { args, .. } = ty else {
        unreachable!()
    };
    assert_ne!(
        id,
        arena.intern(Type::Proc {
            args,
            returns: None
        })
    );
    let empty = Type::Proc {
        args: SmallArray::new(),
        returns: None,
    };
    let empty_id = arena.intern(empty.clone());
    assert_eq!(empty_id, arena.intern(empty));
}

#[test]
#[should_panic(expected = "invalid child type ID")]
fn procedure_types_reject_invalid_arguments() {
    let mut args = SmallArray::new();
    args.push(TypeID(std::num::NonZeroU32::new(1).unwrap()));
    TypeArena::default().intern(Type::Proc {
        args,
        returns: None,
    });
}

#[test]
#[should_panic(expected = "invalid child type ID")]
fn procedure_types_reject_invalid_returns() {
    TypeArena::default().intern(Type::Proc {
        args: SmallArray::new(),
        returns: Some(TypeID(std::num::NonZeroU32::new(1).unwrap())),
    });
}

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
            name: "Index".into(),
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
                name: format!("name{n}").into(),
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
    let name = Str::from("Index");
    let id = arena.intern(Type::Distinct {
        inner: int,
        name: name.clone(),
    });
    let Type::Distinct { name: stored, .. } = &arena.types[id] else {
        panic!()
    };
    assert!(Str::ptr_eq(&name, stored));
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
