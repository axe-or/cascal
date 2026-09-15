use super::*;

#[test]
fn wrapping_and_shift_limits() {
    assert_eq!(add_i8(127, 1), -128);
    assert_eq!(sub_u64(0, 1), u64::MAX);
    assert_eq!(mul_i32(i32::MAX, 2), -2);
    assert_eq!(u_shl8(1, 7), 128);
    assert_eq!(u_shl8(1, 8), 0);
    assert_eq!(u_shr64(u64::MAX, 63), 1);
    assert_eq!(u_shr64(u64::MAX, u64::MAX), 0);
    assert_eq!(i_shr32(-4, 1), -2);
    assert_eq!(i_shr32(-4, 31), 0);
    assert_eq!(i_shl64(1, 63), i64::MIN);
    assert_eq!(i_shl64(1, 64), 0);
}
