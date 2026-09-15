pub const INLINE_COUNT: usize = 2;

#[derive(Debug)]
pub enum SmallArray {
    Inline {
        data: [i32; INLINE_COUNT],
        len: usize,
    },
    Heap(Vec<i32>),
}

impl Default for SmallArray {
    fn default() -> Self {
        Self::new()
    }
}

impl SmallArray {
    pub fn new() -> Self {
        Self::Inline {
            data: [0; INLINE_COUNT],
            len: 0,
        }
    }

    pub fn as_slice(&self) -> &[i32] {
        match self {
            Self::Inline { data, len } => &data[..*len],
            Self::Heap(data) => data,
        }
    }

    /// Ensures room for at least `new_cap` elements in total.
    pub fn try_reserve_capacity(
        &mut self,
        new_cap: usize,
    ) -> Result<(), std::collections::TryReserveError> {
        match self {
            Self::Inline { data, len } if new_cap > INLINE_COUNT => {
                let mut heap = Vec::new();
                heap.try_reserve(new_cap)?;
                heap.extend_from_slice(&data[..*len]);
                *self = Self::Heap(heap);
            }
            Self::Heap(data) if new_cap > data.capacity() => {
                data.try_reserve(new_cap - data.len())?;
            }
            _ => {}
        }
        Ok(())
    }

    pub fn push(&mut self, value: i32) {
        if matches!(
            self,
            Self::Inline {
                len: INLINE_COUNT,
                ..
            }
        ) {
            self.try_reserve_capacity(16)
                .expect("small array exhausted");
        }
        match self {
            Self::Inline { data, len } => {
                data[*len] = value;
                *len += 1;
            }
            Self::Heap(data) => data.push(value),
        }
    }
}

#[cfg(test)]
#[path = "small_array_test.rs"]
mod small_array_test;
