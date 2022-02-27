function table.copy(t, seen)
	local n = {}
	seen = seen or {}
	seen[t] = n
	for k, v in pairs(t) do
		n[(type(k) == "table" and (seen[k] or table.copy(k, seen))) or k] =
			(type(v) == "table" and (seen[v] or table.copy(v, seen))) or v
	end
	return n
end

function string:split(delimiter)
	local result = {}
	local from = 1
	local delim_from, delim_to = string.find(self, delimiter, from)
	while delim_from do
		table.insert(result, string.sub(self, from, delim_from - 1))
		from = delim_to + 1
		delim_from, delim_to = string.find(self, delimiter, from)
	end
	table.insert(result, string.sub(self, from))
	return result
end

function math.round(n, digits)
	digits = digits or 0

	local multi = math.pow(10, digits)
	n = n * multi
	if n > 0 then
		return math.floor(n + 0.5) / multi
	else
		return math.ceil(n - 0.5) / multi
	end
end

function dump(t, layer)
	layer = layer or 0
	if layer > 10 then
		return "ERR_DUMP_TOO_DEEP"
	end
	if t == nil then
		return "nil value"
	end

	local txt = {}
	if type(t) == "table" then
		txt[#txt + 1] = "{"
		for k, v in pairs(t) do
			txt[#txt + 1] = (" [%s] = %s"):format(tostring(k), dump(v, layer + 1))
		end
		txt[#txt + 1] = "} "
	elseif type(t) == "string" then
		txt[#txt + 1] = '"' .. t:gsub('\n', "$LF"):gsub('"', '\\"') .. '" '
	else
		txt[#txt + 1] = tostring(t) .. " "
	end
	return table.concat(txt)
end

function percent(num, text)
	if not num or type(num) ~= "number" then
		error("Argument #1: Number expected.")
	end
	if num > 1 or num < -1 then
		error("Number '".. num .."' not accepted. Allowed range: -1 .. 1")
	end
	text = text or ""
	print(math.round(num * 100, 2).."% ".. text)
end

local tech_sizes = {
	{ "Y", 1E24 },
	{ "Z", 1E21 },
	{ "E", 1E18 },
	{ "P", 1E15 },
	{ "T", 1E12 },
	{ "G", 1E9 },
	{ "M", 1E6 },
	{ "k", 1E3 },
	{ "K", 1E3 },
	{ "%", 0.01, false },
	{ "m", 1E-3 },
	{ "\xb5", 1E-6, false },
	{ "u", 1E-6 },
	{ "n", 1E-9 },
	{ "p", 1E-12 },
	{ "f", 1E-15 }
}

function fromFancy(num, i)
	i = i or 1
	assert(type(num) == "string", "Argument #"..i..": String expected.")

	num = num:gsub(" ", "")
	assert(num ~= "", "Argument #"..i..": Invalid number formatting.")

	local last_char = string.sub(num, -1)
	for i, v in ipairs(tech_sizes) do
		if last_char == v[1] then
			return tonumber(string.sub(num, 1, -2)) * v[2]
		end
	end
	return tonumber(num) or error("Unknown factor: "..last_char)
end

function toFancy(num, perc, i)
	i = i or 1
	assert(type(num) == "number", "Argument #"..i..": Number expected.")
	assert(type(num) == "nil" or type(num) == "number",
			"Invalid type for 'perc': nil or number expected.")

	if num >= 1 and num < 1000 then
		if perc then
			num = math.round(num, perc)
		end
		return tostring(num)
	end

	for i, v in ipairs(tech_sizes) do
		if num >= v[2] and (#v < 3 or v[3])
				or i == #tech_sizes then
			num = num / v[2]
			if perc then
				num = math.round(num, perc)
				if num == 0 then
					return "0"
				end
			end
			return tostring(num)..v[1]
		end
	end
	error("Something went wrong :/")
end
