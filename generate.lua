local templ = require 'templ'

local output = templ.hash_table {
    key = 'u32',
    value = 'Type_ID',
    hash_func = '(u32)', -- Key is already a hash, so just cast
    name = 'Type_Interning_Table',
	prefix = 'type_intern_',
	eq_func = 'u32_eq',
}

templ.write_file('hash_node_id_table.h', templ.header_prelude .. '#include "lang.h"\n' .. output)

