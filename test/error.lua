local function erroneous()
	error('This is an error for testing purposes.')
end

function load()
	window.color(0x0055aaff)
	print('load')
	if math.random() < 0.5 then
		erroneous()
	end
end

function update(deltaT)
	print('update', deltaT)
	if math.random() < 0.5 then
		if math.random() < 0.5 then
			erroneous()
		else
			window.color(nil)
		end
	end
	return true
end

function draw(gfx)
	-- no-op
end
