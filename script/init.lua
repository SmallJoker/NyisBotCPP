dofile("script/misc_helpers.lua")
math.randomseed(bot.clock())

local function register_callbacks(name)
	local cb = {}
	bot["callbacks_" .. name] = cb
	bot["register_" .. name] = function(func)
		cb[#cb + 1] = func
	end
end

register_callbacks("on_client_ready")
register_callbacks("on_step")
register_callbacks("on_channel_join")
register_callbacks("on_channel_leave")
register_callbacks("on_user_join")
register_callbacks("on_user_leave")
register_callbacks("on_user_rename")
register_callbacks("on_user_say")

function bot.run_callbacks(name, mode, ...)
	local cb = bot["callbacks_" .. name]
	for _, func in ipairs(cb) do
		local val = { func(...) }
		if mode == 1 then
			-- Abort on true
			if val[1] then
				return true
			end
		else
			-- Execute all, no abort.
		end
	end
end

bot.chatcommands = {}

bot.register_on_user_say(function(c, ui, msg)
	local ref = bot.chatcommands
	while true do
		local part, msg = msg:match("(%S+)%s*(.*)")
		if not ref[part] then
			return false
		end
		if type(ref[part]) == "function" then
			local good, msg = ref[part](c, ui, msg)
			-- (status) (text ...)
			-- (text ...)
			if type(good) ~= "boolean" then
				msg = good
				good = true
			end
			if type(msg) == "string" then
				c:say(good and msg or (" -!- " .. msg))
			end
			return true
		elseif type(ref[part]) == "table" then
			ref = ref[part]
		else
			print("Invalid type: " .. type(ref[part]))
			return false
		end
	end
	return false
end)

bot.register_on_client_ready(function()
	print("Client ready!")
end)

bot.register_on_user_join(function(c, ui)
	print("User " .. c:get_nickname(ui) .. " joined in " .. c:get_name())
end)

dofile("script/fun.lua")
