
bot.chatcommands["$luahello"] = function(c, ui, msg)
	c:say("Hello " .. c:get_nickname(ui) .. " in " .. c:get_name() .. "!")
	return true
end

bot.chatcommands["$sell"] = function(c, ui, msg)
	if #msg < 2 then
		return false, "Sell what?"
	end

	local n = math.random() * 100
	if n > 22 then
		return "Sold " .. msg .. " for $".. math.ceil(math.random() * 100) * 10
	elseif n > 18 then
		return "FBI open up! " .. c:get_nickname(ui) .. " tried to sell the illegal " .. msg
	elseif n > 12 then
		return text .. " cannot be sold. " .. c:get_nickname(ui) .. " lost $" ..
			math.ceil(math.random() * 20) * 5 .. " to this nonsense effort."
	elseif n > 6 then
		return msg .. " caused an exception in the matrix. Trynfiw ts94 s:fe sa " ..
			"*BEEEEP* Segmentation fault (Core dumped)"
	else
		return msg .. " was thrown into the fire. Please wait..."
	end
end
