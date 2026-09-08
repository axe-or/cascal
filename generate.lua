local templ = require 'templ'

local output = templ.hash_table {
    key = 'u32',
    value = 'Type_ID',
    hash_func = '(u32)', -- Key is already a hash, so just cast
    name = 'Type_ID_By_Hash',
	prefix = 'type_id_hash_',
	eq_func = 'u32_eq',
}

templ.write_file('gen/type_id_by_hash.c', templ.header_prelude .. output)

