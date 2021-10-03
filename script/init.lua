
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

function bot.run_callbacks(name, mode, ...)
	local cb = bot["callbacks_" .. name]
	for _, func in ipairs(cb) do
		local val = { func(...) }
		if mode == 1 then
			-- Abort on true
			if val[1] then
				return
			end
		else
			-- Execute all, no abort.
		end
	end
end

bot.register_on_client_ready(function(what)
	print("Ready: " .. tostring(what))
	test("foobar baz")
end)

print("Hello world", "test")
