pub const INLINE_COUNT: usize = 2;

/// Only initialized elements are stored, without requiring `T: Default` or unsafe code.
#[derive(Clone, Debug)]
pub enum InlineArray<T> {
    Empty,
    One([T; 1]),
    Two([T; INLINE_COUNT]),
}

#[derive(Clone, Debug)]
pub enum SmallArray<T> {
    Inline { data: InlineArray<T> },
    Heap(Vec<T>),
}

impl<T: PartialEq> PartialEq for SmallArray<T> {
    fn eq(&self, other: &Self) -> bool {
        self.as_slice() == other.as_slice()
    }
}

impl<T: Eq> Eq for SmallArray<T> {}

impl<T> Default for SmallArray<T> {
    fn default() -> Self {
        Self::new()
    }
}

impl<T> SmallArray<T> {
    pub fn new() -> Self {
        Self::Inline {
            data: InlineArray::Empty,
        }
    }

    pub fn as_slice(&self) -> &[T] {
        match self {
            Self::Inline {
                data: InlineArray::Empty,
            } => &[],
            Self::Inline {
                data: InlineArray::One(data),
            } => data,
            Self::Inline {
                data: InlineArray::Two(data),
            } => data,
            Self::Heap(data) => data,
        }
    }

    /// Ensures room for at least `new_cap` elements in total.
    pub fn try_reserve_capacity(
        &mut self,
        new_cap: usize,
    ) -> Result<(), std::collections::TryReserveError> {
        match self {
            Self::Inline { data } if new_cap > INLINE_COUNT => {
                let mut heap = Vec::new();
                heap.try_reserve(new_cap)?;
                match std::mem::replace(data, InlineArray::Empty) {
                    InlineArray::Empty => {}
                    InlineArray::One(values) => heap.extend(values),
                    InlineArray::Two(values) => heap.extend(values),
                }
                *self = Self::Heap(heap);
            }
            Self::Heap(data) if new_cap > data.capacity() => {
                data.try_reserve(new_cap - data.len())?;
            }
            _ => {}
        }
        Ok(())
    }

    pub fn push(&mut self, value: T) {
        if matches!(
            self,
            Self::Inline {
                data: InlineArray::Two(_),
            }
        ) {
            self.try_reserve_capacity(16)
                .expect("small array exhausted");
        }
        match self {
            Self::Inline { data } => {
                *data = match std::mem::replace(data, InlineArray::Empty) {
                    InlineArray::Empty => InlineArray::One([value]),
                    InlineArray::One([first]) => InlineArray::Two([first, value]),
                    InlineArray::Two(_) => unreachable!("full inline array was moved to heap"),
                };
            }
            Self::Heap(data) => data.push(value),
        }
    }
}

#[cfg(test)]
#[path = "small_array_test.rs"]
mod small_array_test;
