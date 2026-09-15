//! Fixed-width arithmetic formerly generated in gen/arith.h.

macro_rules! arithmetic {
    ($ty:ty, $shift:ty, $right_limit:expr, $add:ident, $sub:ident, $mul:ident, $shl:ident, $shr:ident) => {
        pub fn $add(a: $ty, b: $ty) -> $ty { a.wrapping_add(b) }
        pub fn $sub(a: $ty, b: $ty) -> $ty { a.wrapping_sub(b) }
        pub fn $mul(a: $ty, b: $ty) -> $ty { a.wrapping_mul(b) }
        pub fn $shl(a: $ty, b: $shift) -> $ty {
            if b >= <$ty>::BITS as $shift { 0 } else { a << b }
        }
        pub fn $shr(a: $ty, b: $shift) -> $ty {
            if b >= $right_limit { 0 } else { a >> b }
        }
    };
}

arithmetic!(u8, u32, 8, add_u8, sub_u8, mul_u8, u_shl8, u_shr8);
arithmetic!(u16, u32, 16, add_u16, sub_u16, mul_u16, u_shl16, u_shr16);
arithmetic!(u32, u32, 32, add_u32, sub_u32, mul_u32, u_shl32, u_shr32);
arithmetic!(u64, u64, 64, add_u64, sub_u64, mul_u64, u_shl64, u_shr64);
// Preserve the C helpers' signed right-shift cutoff at width - 1.
arithmetic!(i8, u32, 7, add_i8, sub_i8, mul_i8, i_shl8, i_shr8);
arithmetic!(i16, u32, 15, add_i16, sub_i16, mul_i16, i_shl16, i_shr16);
arithmetic!(i32, u32, 31, add_i32, sub_i32, mul_i32, i_shl32, i_shr32);
arithmetic!(i64, u64, 63, add_i64, sub_i64, mul_i64, i_shl64, i_shr64);

#[cfg(test)]
#[path = "arith_test.rs"]
mod arith_test;
