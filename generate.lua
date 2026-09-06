local templ = require 'templ'

local output = templ.hash_table {
    key = 'String',
    value = 'f32',
    hash_func = 'str_hash',
    name = 'Earnings'
}

templ.write_file('hash_table.h', templ.header_prelude .. output)

