local io_size = 1024
local preload = "preload.lua"
local dev = "/dev/luadrv"

local function get_contents(filename)
	local f = assert(io.open(filename, "r"))
	local s = f:read("a")
	f:close()
	return s
end

local program = get_contents(preload) .. get_contents(arg[1])
local f = assert(io.open(dev, "w"))
local n = math.ceil(#program / io_size)
for i = 1, n do
	f:write(program:sub((i - 1) * io_size, i * io_size))
	f:flush()
end
f:close()
