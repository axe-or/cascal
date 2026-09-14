local templ = require 'templ'

local function extend(tbl, keys)
	local copy = {}
	for k, v in pairs(tbl) do
		copy[k] = v
	end

	for k, v in pairs(keys) do
		copy[k] = v
	end
	return copy
end

local function type_id_by_hash()
	local tbl = templ.hash_table {
		key = 'u32',
		value = 'Type_ID',
		hash_func = '(u32)', -- Key is already a hash, so just cast
		name = 'Type_ID_By_Hash',
		prefix = 'type_id_hash_',
		eq_func = 'u32_eq',
	}
	templ.write_file('gen/type_id_by_hash.c', templ.header_prelude .. tbl)
end

local function symbol_by_name()
	local tbl = templ.hash_table {
		key = 'String',
		value = 'Symbol',
		hash_func = 'str_hash',
		name = 'Symbol_By_Name',
		prefix = 'symbol_name_',
		eq_func = 'str_equal',
	}
	templ.write_file('gen/symbol_by_name.c', templ.header_prelude .. tbl)
end

local arithmetic_prelude = [=[/* Well defined arithmetic that does not follow C's retarded rules */
#pragma once

#include <stdint.h>

#if defined(_MSC_VER)
	#define arith_func __forceinline static
#elif defined(__GCC__) || defined(__clang__)
	#define arith_func __attribute__((always_inline, artificial)) static inline
#else
	#define arith_func static inline
#endif
]=]

local function arith_operator(sign, width, name, op)
	local src = [=[arith_func @type @name(@type a, @type b){
	return (@type)((@utype)a @op (@utype)b);
}]=]
	local exp = templ.expand_template(src, {
		op = op,
		name = ('%s_%s%d'):format(name, sign and 'i' or 'u' ,width),
		type = ('%s%d_t'):format(sign and 'int' or 'uint', width),
		utype = ('uint%d_t'):format(width >= 32 and width or 32), -- NOTE: This is to avoid integer promotion bullshit in C
	})
	return exp
end

local function shift_op(sign, width)
	local left_src = [=[arith_func @type @name(@type a, @utype b){
	if((@utype)b >= @width){ return 0; }
	return (@type)((@utype)a @op (@utype)b);
}]=]

	local signed_right_src = [=[arith_func @type @name(@type a, @utype b){
	if((@utype)b >= (@width - 1)){
		return 0;
	}
	return a >> (@type)b;
}]=]

	local conf = {
		type = ('%s%d_t'):format(sign and 'int' or 'uint', width),
		utype = ('uint%d_t'):format(width >= 32 and width or 32), -- NOTE: This is to avoid integer promotion bullshit in C
		width = width,
	}

	local left = templ.expand_template(left_src,  extend(conf, {
		op = '<<',
		name = ('%s_shl%d'):format(sign and 'i' or 'u' ,width),
	}))

	local right = templ.expand_template(left_src,  extend(conf, {
		op = '>>',
		name = ('%s_shr%d'):format(sign and 'i' or 'u' ,width),
	}))

	if sign then
        right = templ.expand_template(signed_right_src, extend(conf, {
	        name = ('%s_shr%d'):format(sign and 'i' or 'u' ,width),
		}))
	end

	return left .. '\n' .. right
end

local function generate_arithmetic()
	local WIDTHS = {8, 16, 32, 64}

	local OPERATORS = { { 'add', '+' }, { 'sub', '-' }, { 'mul', '*' } }
	local src = {arithmetic_prelude}

	for _, sign in ipairs{false, true} do
		for _, width in ipairs(WIDTHS) do
			for _, operator in ipairs(OPERATORS) do
				src[#src+1] = arith_operator(sign, width, operator[1], operator[2])
			end

			src[#src+1] = shift_op(sign, width)
		end
	end

	templ.write_file('gen/arith.h', table.concat(src, '\n'))
end


-- Main
type_id_by_hash()
symbol_by_name()
generate_arithmetic()
