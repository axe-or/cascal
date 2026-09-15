pub const INLINE_COUNT: usize = 2;

#[derive(Debug)]
pub enum SmallArray {
    Inline { data: [i32; INLINE_COUNT], len: usize },
    Heap(Vec<i32>),
}

impl Default for SmallArray {
    fn default() -> Self { sm_arr_init() }
}

pub fn sm_arr_init() -> SmallArray { SmallArray::Inline { data: [0; INLINE_COUNT], len: 0 } }

pub fn sm_arr_slice(arr: &SmallArray) -> &[i32] {
    match arr { SmallArray::Inline { data, len } => &data[..*len], SmallArray::Heap(data) => data }
}

pub fn sm_arr_reserve(arr: &mut SmallArray, new_cap: usize) -> Result<(), std::collections::TryReserveError> {
    match arr {
        SmallArray::Inline { data, len } if new_cap > INLINE_COUNT => {
            let mut heap = Vec::new();
            heap.try_reserve(new_cap)?;
            heap.extend_from_slice(&data[..*len]);
            *arr = SmallArray::Heap(heap);
        }
        SmallArray::Heap(data) if new_cap > data.capacity() => { data.try_reserve(new_cap - data.len())?; }
        _ => {}
    }
    Ok(())
}

pub fn sm_arr_push(arr: &mut SmallArray, value: i32) {
    if matches!(arr, SmallArray::Inline { len: INLINE_COUNT, .. }) {
        sm_arr_reserve(arr, 16).expect("small array exhausted");
    }
    match arr {
        SmallArray::Inline { data, len } => { data[*len] = value; *len += 1; }
        SmallArray::Heap(data) => data.push(value),
    }
}

#[cfg(test)]
#[path = "small_array_test.rs"]
mod small_array_test;
