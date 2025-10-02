# beeper.py - arcapy/PocketPy port
from arcamini import resource, audio

class Beeper:
    def __init__(self):
        self.notes = []
        self.sources = {}

    def beep(self, freq, duration, vol, timbre=0.5, pan=0):
        rate = 44100
        key = f"{int(freq)}_{int(duration * 1000)}_{int(timbre * 100)}"
        source = self.sources.get(key)
        if not source:
            sample_count = int(duration * rate)
            samples = [(1 if (i % int(rate / freq) < int(timbre * int(rate / freq))) else -1) for i in range(sample_count)]
            source = resource.createAudio(samples, 1)
            self.sources[key] = source
        audio.replay(source, vol, pan)

    def update(self, deltaT):
        for i in range(len(self.notes) - 1, -1, -1):
            note = self.notes[i]
            note['deltaT'] -= deltaT
            if note['deltaT'] <= 0:
                self.beep(note['freq'], note['duration'], note['vol'], note['timbre'], note['pan'])
                self.notes.pop(i)

    def play(self, *notes):
        freq = duration = vol = pan = 0
        timbre = 0.5
        deltaT = 0
        for note in notes:
            freq = note.get('freq', freq)
            duration = note.get('duration', duration)
            vol = note.get('vol', vol)
            timbre = note.get('timbre', timbre)
            pan = note.get('pan', pan)
            deltaT = note.get('deltaT', 0) + deltaT
            if freq > 0 and duration > 0 and vol > 0:
                self.notes.append({'freq': freq, 'duration': duration, 'vol': vol, 'timbre': timbre, 'pan': pan, 'deltaT': deltaT})
            else:
                print('ignoring', note)
            deltaT += duration

beeper = Beeper()
