#include "base.h"
#include "lang.h"
#include "testing.h"

void type_tests(Test* t){
	t->name = "types";

	Type int_a = {.primitive = Prim_Int, .kind = Type_Primitive};
	Type int_b = {.primitive = Prim_Int, .kind = Type_Primitive};
	Type real = {.primitive = Prim_Real, .kind = Type_Primitive};

	t_pred(t, type_eq(&int_a, &int_b));
	t_pred(t, type_hash(&int_a) == type_hash(&int_b));
	t_pred(t, !type_eq(&int_a, &real));
	t_pred(t, type_hash(&int_a) != type_hash(&real));

	Type int_pointer_a = {
		.pointer.inner = {1},
		.kind = Type_Pointer,
	};
	Type int_pointer_b = {
		.pointer.inner = {1},
		.kind = Type_Pointer,
	};
	Type int_slice = {
		.slice.inner = {1},
		.kind = Type_Slice,
	};

	t_pred(t, type_eq(&int_pointer_a, &int_pointer_b));
	t_pred(t, type_hash(&int_pointer_a) == type_hash(&int_pointer_b));
	t_pred(t, !type_eq(&int_pointer_a, &int_slice));
	t_pred(t, type_hash(&int_pointer_a) != type_hash(&int_slice));

	Type array_8_a = {
		.array = {.inner = {2}, .size = 8},
		.kind = Type_Array,
	};
	Type array_8_b = {
		.array = {.inner = {2}, .size = 8},
		.kind = Type_Array,
	};
	Type array_9 = {
		.array = {.inner = {2}, .size = 9},
		.kind = Type_Array,
	};

	t_pred(t, type_eq(&array_8_a, &array_8_b));
	t_pred(t, type_hash(&array_8_a) == type_hash(&array_8_b));
	t_pred(t, !type_eq(&array_8_a, &array_9));
	t_pred(t, type_hash(&array_8_a) != type_hash(&array_9));

	char meters_a[] = "Meters";
	char meters_b[] = "Meters";
	Type meters_type_a = {
		.distinct = {
			.name = {.v = meters_a, .len = sizeof(meters_a) - 1},
			.inner = {3},
		},
		.kind = Type_Distinct,
	};
	Type meters_type_b = {
		.distinct = {
			.name = {.v = meters_b, .len = sizeof(meters_b) - 1},
			.inner = {3},
		},
		.kind = Type_Distinct,
	};
	Type seconds_type = {
		.distinct = {
			.name = strlit("Seconds"),
			.inner = {3},
		},
		.kind = Type_Distinct,
	};

	t_pred(t, type_eq(&meters_type_a, &meters_type_b));
	t_pred(t, type_hash(&meters_type_a) == type_hash(&meters_type_b));
	t_pred(t, !type_eq(&meters_type_a, &seconds_type));
	t_pred(t, type_hash(&meters_type_a) != type_hash(&seconds_type));
	t_pred(t, type_hash(&meters_type_a) == type_hash(&meters_type_a));

	// Changing only the child ID must change type identity for each wrapper.
	Type originals[] = {int_pointer_a, int_slice, array_8_a, meters_type_a};
	Type changed[] = {int_pointer_a, int_slice, array_8_a, meters_type_a};
	changed[0].pointer.inner.v = 99;
	changed[1].slice.inner.v = 99;
	changed[2].array.inner.v = 99;
	changed[3].distinct.inner.v = 99;
	for(usize i = 0; i < sizeof(originals) / sizeof(originals[0]); i += 1){
		t_pred(t, !type_eq(&originals[i], &changed[i]));
	}
}
