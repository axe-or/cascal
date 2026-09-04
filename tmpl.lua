-- Global template environment
local env = {
    K = 'String',
    V = 'f32',
    Hash_Table = "Earnings",
	hash_func = "str_hash";

    prefix = "earnings_",
}

local hash_table_funcs = {'init', 'insert', 'remove', 'find', 'get', 'probe_distance', 'key_hash'}
for _, fn in ipairs(hash_table_funcs) do
    env[fn] = env.prefix .. fn
end

env.Hash_Table_Slot = env.Hash_Table .. "_Slot"

local function expand_template(text, values)
    return (text:gsub("(\\*)@([%a_][%w_]*)", function(backslashes, key)
        local prefix = string.rep("\\", math.floor(#backslashes / 2))

        if #backslashes % 2 == 1 then
            return prefix .. "@" .. key
        end

        local value = values[key]
        if value == nil then
            error("no template value for @" .. key, 2)
        end

        return prefix .. tostring(value)
    end))
end

local function read_file(path)
    local f = io.open(path, "r")
    assert(f, "failed to open file")
    local data = f:read("a*")
    f:close()
    return data
end

local output = expand_template(read_file("hash_table.tmpl"), env)

print(output)
