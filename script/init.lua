
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
		local val = { pcall(func, ...) }
		if not val[1] then
			print(name .. " callback failed: " .. val[2])
		elseif mode == 1 then
			-- Abort on true
			if val[2] then
				return true
			end
		else
			-- Execute all, no abort.
		end
	end
end

bot.register_on_client_ready(function()
	print("Client ready!")
end)

bot.register_on_user_join(function(c, ui)
	print("User " .. c:get_nickname(ui) .. " joined.")
end)

bot.register_on_user_say(function(c, ui, msg)
	if msg == "$luahello" then
		c:say("Hello " .. c:get_nickname(ui) .. "!")
		return true
    end
end)
