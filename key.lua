local small_set = false

-- invoke 'key' strokes for each line in the inlut file
function keyfile (path)
	local file = io.open(path, "rb") 
	if not file then return nil end

	local lines = {}
	
	for line in io.lines(path) do
		key ( line )
	end

	file:close()
	return lines
end
	
-- send key strokes from the input string
function key(input_str)

	local function key_press(input_tag, input_mask)
		local machine = manager.machine
		local input_port = machine.ioport.ports[input_tag]
		if input_port then
			for k,field in pairs ( input_port.fields ) do
				if field.mask == input_mask then
					-- Emulate key press
					field:set_value(1)
					emu.wait(0.1)  -- Wait for the specified duration
					-- Emulate key release
					field:set_value(0)
					break
				end
			end
			emu.wait(0.1)
		else
			print("Input port not found.")
		end
	end

	local function switch(value)
		-- Handing `cases` to the returned function allows the `switch()` function to be used with a syntax closer to c code (see the example below).
		-- This is because lua allows the parentheses around a table type argument to be omitted if it is the only argument.
		return function(cases)
			
			-- The default case is achieved through the metatable mechanism of lua tables (the `__index` operation).
			setmetatable(cases, cases)
			
			local f = cases[value]
			if f then
				f()
			end
		end
	end
	
	local function is_uppercase(char)
		return string.match(char, "%u") ~= nil
	end
	
	local function is_lowercase(char)
		return string.match(char, "%l") ~= nil
	end

	for i = 1, #input_str do
        local char = input_str:sub(i, i)	
		
		if is_uppercase(char) then
			if small_set then
				key_press ( ":KEY1" , 0x80)
				small_set = false
			end
		end
		if is_lowercase(char) then
			if not small_set then
				key_press ( ":KEY1" , 0x80)
				small_set = true
			end
		end
		
		if char == "#" then -- reserved for BASIC toggling
			key_press (":KEY11", 0x80)
			emu.wait(0.2)
		end 
			
		switch (char) {
		
			-- List of port-mask codes from mame\artwork\pc1403\pc1403.lay
			
			--	 sml   ":KEY1"  0x80
			--   shift ":KEY1"  0x20
			--   basic ":KEY11" 0x80
			--   cal   ":KEY12" 0x80
			--   c-ce  ":KEY7"  0x02
			
			-- Left Row 2  
			["Q"] = function()  key_press ( ":KEY2" , 0x20) end ,
			["W"] = function()  key_press ( ":KEY3" , 0x20) end ,
			["E"] = function()  key_press ( ":KEY4" , 0x20) end ,
			["R"] = function()  key_press ( ":KEY5" , 0x20) end ,
			["T"] = function()  key_press ( ":KEY6" , 0x20) end ,
			["Y"] = function()  key_press ( ":KEY7" , 0x20) end ,
			["U"] = function()  key_press ( ":KEY8" , 0x20) end ,
			["I"] = function()  key_press ( ":KEY9" , 0x20) end ,
			["O"] = function()  key_press ( ":KEY10", 0x20) end ,
			["P"] = function()  key_press ( ":KEY11", 0x20) end ,
			
			-- Left Row 2 with SHIFT
			["!"] = function()  key_press (":KEY1", 0x20) key_press (":KEY2", 0x20) end ,
			["\""] = function() key_press (":KEY1", 0x20) key_press (":KEY3" ,0x20)  end ,
			
			-- # reserved for BASIC toggling
			-- ["#"] = function()  key_press (":KEY1", 0x20) key_press (":KEY4"  ,0x20) end ,
			
			["$"] = function()  key_press (":KEY1", 0x20) key_press (":KEY5"  ,0x20) end ,
			["%"] = function()  key_press (":KEY1", 0x20) key_press (":KEY6"  ,0x20) end ,
			["&"] = function()  key_press (":KEY1", 0x20) key_press (":KEY7"  ,0x20) end ,
			["?"] = function()  key_press (":KEY1", 0x20) key_press (":KEY8"  ,0x20) end ,
			["@"] = function()  key_press (":KEY1", 0x20) key_press (":KEY9"  ,0x20) end ,
			[":"] = function()  key_press (":KEY1", 0x20) key_press (":KEY10" ,0x20) end ,
			[";"] = function()  key_press (":KEY1", 0x20) key_press (":KEY11" ,0x20) end ,
			
			-- -- Left Row 3 : A S D F G H J K L , 
			["A"] = function() key_press ( ":KEY2" , 0x40) end ,
			["S"] = function() key_press ( ":KEY3" , 0x40) end ,
			["D"] = function() key_press ( ":KEY4" , 0x40) end ,
			["F"] = function() key_press ( ":KEY5" , 0x40) end ,
			["G"] = function() key_press ( ":KEY6" , 0x40) end ,
			["H"] = function() key_press ( ":KEY7" , 0x40) end ,
			["J"] = function() key_press ( ":KEY8" , 0x40) end ,
			["K"] = function() key_press ( ":KEY9" , 0x40) end ,
			["L"] = function() key_press ( ":KEY10", 0x40) end ,
			[","] = function() key_press ( ":KEY11", 0x40) end ,

			-- -- Left Row 4 : Z X C V B N M SPC ENTER
			["Z"] = function() key_press ( ":KEY2"      ,0x80 )  end ,
			["X"] = function() key_press ( ":KEY3"      ,0x80 )  end ,
			["C"] = function() key_press ( ":KEY4"      ,0x80 )  end ,
			["V"] = function() key_press ( ":KEY5"      ,0x80 )  end ,
			["B"] = function() key_press ( ":KEY6"      ,0x80 )  end ,
			["N"] = function() key_press ( ":KEY7"      ,0x80 )  end ,
			["M"] = function() key_press ( ":KEY8"      ,0x80 )  end ,
			[" "] = function() key_press ( ":KEY9"      ,0x80 )  end ,
			[string.char(0x0D)] = function() key_press ( ":KEY10" ,0x80 ) end ,
			
			-- LOWER case letters (same as upper case, but SML was issued first)
			["q"] = function() key_press ( ":KEY2" , 0x20) end ,
			["w"] = function() key_press ( ":KEY3" , 0x20) end ,
			["e"] = function() key_press ( ":KEY4" , 0x20) end ,
			["r"] = function() key_press ( ":KEY5" , 0x20) end ,
			["t"] = function() key_press ( ":KEY6" , 0x20) end ,
			["y"] = function() key_press ( ":KEY7" , 0x20) end ,
			["u"] = function() key_press ( ":KEY8" , 0x20) end ,
			["i"] = function() key_press ( ":KEY9" , 0x20) end ,
			["o"] = function() key_press ( ":KEY10", 0x20) end ,
			["p"] = function() key_press ( ":KEY11", 0x20) end ,

			["a"] = function() key_press ( ":KEY2" , 0x40) end ,
			["s"] = function() key_press ( ":KEY3" , 0x40) end ,
			["d"] = function() key_press ( ":KEY4" , 0x40) end ,
			["f"] = function() key_press ( ":KEY5" , 0x40) end ,
			["g"] = function() key_press ( ":KEY6" , 0x40) end ,
			["h"] = function() key_press ( ":KEY7" , 0x40) end ,
			["j"] = function() key_press ( ":KEY8" , 0x40) end ,
			["k"] = function() key_press ( ":KEY9" , 0x40) end ,
			["l"] = function() key_press ( ":KEY10", 0x40) end ,

			["z"] = function() key_press ( ":KEY2" ,0x80 )  end ,
			["x"] = function() key_press ( ":KEY3" ,0x80 )  end ,
			["c"] = function() key_press ( ":KEY4" ,0x80 )  end ,
			["v"] = function() key_press ( ":KEY5" ,0x80 )  end ,
			["b"] = function() key_press ( ":KEY6" ,0x80 )  end ,
			["n"] = function() key_press ( ":KEY7" ,0x80 )  end ,
			["m"] = function() key_press ( ":KEY8" ,0x80 )  end ,
			
			-- Right Row 3 : ... ( ) 
			["("] = function() key_press (  ":KEY9" ,0x08 ) end ,
			[")"] = function() key_press (  ":KEY8" ,0x04 ) end ,
			
			-- Right Row 4 : 7 8 9 /  
			["7"] = function() key_press ( ":KEY0"  ,0x01 ) end ,
			["8"] = function() key_press ( ":KEY0"  ,0x02 ) end ,
			["9"] = function() key_press ( ":KEY0"  ,0x04 ) end ,
			["/"] = function() key_press ( ":KEY0"  ,0x08 ) end ,

			-- Right Row 5 : 4 5 6 *  
			["5"] = function() key_press (  ":KEY1" ,0x01 ) end ,
			["6"] = function() key_press (  ":KEY1" ,0x02 ) end ,
			["7"] = function() key_press (  ":KEY1" ,0x04 ) end ,
			["*"] = function() key_press (  ":KEY1" ,0x08 ) end ,

			-- Right Row 6 : 1 2 3 -
			["1"] = function() key_press (  ":KEY2" ,0x01 ) end ,
			["2"] = function() key_press (  ":KEY2" ,0x02 ) end ,
			["3"] = function() key_press (  ":KEY2" ,0x04 ) end ,
			["-"] = function() key_press (  ":KEY2" ,0x08 ) end ,

			-- Right,Row 7  : 0 +- . + =  
			["0"] = function() key_press (  ":KEY3" ,0x01 ) end ,
			["."] = function() key_press (  ":KEY3" ,0x04 ) end ,
			["+"] = function() key_press (  ":KEY3" ,0x08 ) end ,
			["="] = function() key_press (  ":KEY3" ,0x10 ) end ,
			
		}
		
	end -- for
	
	-- always terminate with ENTER and wait a bit
	emu.wait(0.1)
	key_press ( ":KEY10" ,0x80 ) 
	emu.wait(0.2)

end 