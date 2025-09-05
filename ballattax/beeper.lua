local Beeper = {}
Beeper.__index = Beeper

function Beeper:create()
    local bp = { notes={}, sources={} }
    setmetatable(bp, Beeper)
    return bp
end

function Beeper:beep(freq, duration, vol, timbre, pan)
    --print("beep("..tostring(freq)..", "..tostring(duration)..", "..tostring(vol)..", "..tostring(timbre)..", "..tostring(pan)..")")
    local rate = 44100 -- samples per second
    timbre = timbre or 0.5
    pan = pan or 0
    local key = string.format("%i_%i_%i", math.floor(freq), math.floor(duration * 1000), math.floor(timbre*100))

    -- Retrieve from cache if available
    local source = self.sources[key]
    if not source then
        local sampleCount = math.floor(duration * rate)
        local audioData = {}

        -- Generate pulse wave
        local p = math.floor(rate / freq)
        timbre = math.floor(timbre * p)
        for i = 0, sampleCount - 1 do
            local sample = (i % p < timbre) and 1 or -1
            table.insert(audioData, sample)
        end

        -- Create audio buffer
        source = resource.createAudio(audioData, 1)
        self.sources[key] = source
    end

    -- Play audio
    audio.replay(source, vol, pan)
end

function Beeper:update(deltaT)
    for i = #self.notes,1,-1 do
        local note = self.notes[i]
        note.deltaT = note.deltaT - deltaT
        if note.deltaT <= 0 then -- beep and remove
            self:beep(note.freq, note.duration, note.vol, note.timbre, note.pan)
            table.remove(self.notes, i)
        end
    end
end

function Beeper:play(...)
    local freq, duration, vol, timbre, pan, deltaT = 0,0,0,0.5,0,0
    for i,note in ipairs({...}) do
        freq = note.freq or freq
        duration = note.duration or duration
        vol = note.vol or vol
        timbre = note.timbre or timbre
        pan = note.pan or pan
        deltaT = (note.deltaT or 0) + deltaT
        if freq>0 and duration>0 and vol>0 then
            table.insert(self.notes, {freq=freq, duration=duration, vol=vol, timbre=timbre, pan=pan, deltaT=deltaT})
        else
            print("ignoring "..dump(note))
        end
        deltaT = deltaT + duration
    end
end

local beeper = Beeper:create()
return beeper
