/* Well defined arithmetic that does not follow C's retarded rules */
#pragma once

#include <stdint.h>

#if defined(_MSC_VER)
	#define arith_func __forceinline static
#elif defined(__GCC__) || defined(__clang__)
	#define arith_func __attribute__((always_inline, artificial)) static inline
#else
	#define arith_func static inline
#endif

arith_func uint8_t add_u8(uint8_t a, uint8_t b){
	return (uint8_t)((uint32_t)a + (uint32_t)b);
}
arith_func uint8_t sub_u8(uint8_t a, uint8_t b){
	return (uint8_t)((uint32_t)a - (uint32_t)b);
}
arith_func uint8_t mul_u8(uint8_t a, uint8_t b){
	return (uint8_t)((uint32_t)a * (uint32_t)b);
}
arith_func uint8_t u_shl8(uint8_t a, uint32_t b){
	if((uint32_t)b >= 8){ return 0; }
	return (uint8_t)((uint32_t)a << (uint32_t)b);
}
arith_func uint8_t u_shr8(uint8_t a, uint32_t b){
	if((uint32_t)b >= 8){ return 0; }
	return (uint8_t)((uint32_t)a >> (uint32_t)b);
}
arith_func uint16_t add_u16(uint16_t a, uint16_t b){
	return (uint16_t)((uint32_t)a + (uint32_t)b);
}
arith_func uint16_t sub_u16(uint16_t a, uint16_t b){
	return (uint16_t)((uint32_t)a - (uint32_t)b);
}
arith_func uint16_t mul_u16(uint16_t a, uint16_t b){
	return (uint16_t)((uint32_t)a * (uint32_t)b);
}
arith_func uint16_t u_shl16(uint16_t a, uint32_t b){
	if((uint32_t)b >= 16){ return 0; }
	return (uint16_t)((uint32_t)a << (uint32_t)b);
}
arith_func uint16_t u_shr16(uint16_t a, uint32_t b){
	if((uint32_t)b >= 16){ return 0; }
	return (uint16_t)((uint32_t)a >> (uint32_t)b);
}
arith_func uint32_t add_u32(uint32_t a, uint32_t b){
	return (uint32_t)((uint32_t)a + (uint32_t)b);
}
arith_func uint32_t sub_u32(uint32_t a, uint32_t b){
	return (uint32_t)((uint32_t)a - (uint32_t)b);
}
arith_func uint32_t mul_u32(uint32_t a, uint32_t b){
	return (uint32_t)((uint32_t)a * (uint32_t)b);
}
arith_func uint32_t u_shl32(uint32_t a, uint32_t b){
	if((uint32_t)b >= 32){ return 0; }
	return (uint32_t)((uint32_t)a << (uint32_t)b);
}
arith_func uint32_t u_shr32(uint32_t a, uint32_t b){
	if((uint32_t)b >= 32){ return 0; }
	return (uint32_t)((uint32_t)a >> (uint32_t)b);
}
arith_func uint64_t add_u64(uint64_t a, uint64_t b){
	return (uint64_t)((uint64_t)a + (uint64_t)b);
}
arith_func uint64_t sub_u64(uint64_t a, uint64_t b){
	return (uint64_t)((uint64_t)a - (uint64_t)b);
}
arith_func uint64_t mul_u64(uint64_t a, uint64_t b){
	return (uint64_t)((uint64_t)a * (uint64_t)b);
}
arith_func uint64_t u_shl64(uint64_t a, uint64_t b){
	if((uint64_t)b >= 64){ return 0; }
	return (uint64_t)((uint64_t)a << (uint64_t)b);
}
arith_func uint64_t u_shr64(uint64_t a, uint64_t b){
	if((uint64_t)b >= 64){ return 0; }
	return (uint64_t)((uint64_t)a >> (uint64_t)b);
}
arith_func int8_t add_i8(int8_t a, int8_t b){
	return (int8_t)((uint32_t)a + (uint32_t)b);
}
arith_func int8_t sub_i8(int8_t a, int8_t b){
	return (int8_t)((uint32_t)a - (uint32_t)b);
}
arith_func int8_t mul_i8(int8_t a, int8_t b){
	return (int8_t)((uint32_t)a * (uint32_t)b);
}
arith_func int8_t i_shl8(int8_t a, uint32_t b){
	if((uint32_t)b >= 8){ return 0; }
	return (int8_t)((uint32_t)a << (uint32_t)b);
}
arith_func int8_t i_shr8(int8_t a, uint32_t b){
	if((uint32_t)b >= (8 - 1)){
		return 0;
	}
	return a >> (int8_t)b;
}
arith_func int16_t add_i16(int16_t a, int16_t b){
	return (int16_t)((uint32_t)a + (uint32_t)b);
}
arith_func int16_t sub_i16(int16_t a, int16_t b){
	return (int16_t)((uint32_t)a - (uint32_t)b);
}
arith_func int16_t mul_i16(int16_t a, int16_t b){
	return (int16_t)((uint32_t)a * (uint32_t)b);
}
arith_func int16_t i_shl16(int16_t a, uint32_t b){
	if((uint32_t)b >= 16){ return 0; }
	return (int16_t)((uint32_t)a << (uint32_t)b);
}
arith_func int16_t i_shr16(int16_t a, uint32_t b){
	if((uint32_t)b >= (16 - 1)){
		return 0;
	}
	return a >> (int16_t)b;
}
arith_func int32_t add_i32(int32_t a, int32_t b){
	return (int32_t)((uint32_t)a + (uint32_t)b);
}
arith_func int32_t sub_i32(int32_t a, int32_t b){
	return (int32_t)((uint32_t)a - (uint32_t)b);
}
arith_func int32_t mul_i32(int32_t a, int32_t b){
	return (int32_t)((uint32_t)a * (uint32_t)b);
}
arith_func int32_t i_shl32(int32_t a, uint32_t b){
	if((uint32_t)b >= 32){ return 0; }
	return (int32_t)((uint32_t)a << (uint32_t)b);
}
arith_func int32_t i_shr32(int32_t a, uint32_t b){
	if((uint32_t)b >= (32 - 1)){
		return 0;
	}
	return a >> (int32_t)b;
}
arith_func int64_t add_i64(int64_t a, int64_t b){
	return (int64_t)((uint64_t)a + (uint64_t)b);
}
arith_func int64_t sub_i64(int64_t a, int64_t b){
	return (int64_t)((uint64_t)a - (uint64_t)b);
}
arith_func int64_t mul_i64(int64_t a, int64_t b){
	return (int64_t)((uint64_t)a * (uint64_t)b);
}
arith_func int64_t i_shl64(int64_t a, uint64_t b){
	if((uint64_t)b >= 64){ return 0; }
	return (int64_t)((uint64_t)a << (uint64_t)b);
}
arith_func int64_t i_shr64(int64_t a, uint64_t b){
	if((uint64_t)b >= (64 - 1)){
		return 0;
	}
	return a >> (int64_t)b;
}
