local function read_file(path)
    local f = io.open(path, "r")
    assert(f, "failed to open file")
    local data = f:read("a*")
    f:close()
    return data
end

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

local function ensure_strings(tbl, keys)
    for _, required in ipairs(keys) do
        if type(tbl[required]) ~= "string" then
            error(("missing required string argument: `%s`"):format(required), 2)
        end
    end
end

local ht_template = read_file('hash_table.tmpl')

local function hash_table(opts)
    ensure_strings(opts, {'key', 'value', 'name', 'hash_func'})

    local env = {
        K = opts.key,
        V = opts.value,
        Hash_Table = opts.name,
        hash_func = opts.hash_func;
        prefix = opts.prefix or opts.name:lower() .. '_',
    }

    local hash_table_funcs = {'init', 'insert', 'remove', 'find', 'get', 'probe_distance', 'key_hash'} for _, fn in ipairs(hash_table_funcs) do
        env[fn] = env.prefix .. fn
    end
    env.Hash_Table_Slot = env.Hash_Table .. "_Slot"

    return expand_template(ht_template, env)
end


local output = hash_table {
    key = 'String',
    value = 'f32',
    hash_func = 'str_hash',
    name = 'Earnings'
}

print(output)
