// beeper.js - Arcaqjs/QuickJS port

class Beeper {
    constructor() {
        this.notes = [];
        this.sources = {};
    }

    beep(freq, duration, vol, timbre, pan) {
        const rate = 44100;
        timbre = timbre ?? 0.5;
        pan = pan ?? 0;
        const key = `${Math.floor(freq)}_${Math.floor(duration * 1000)}_${Math.floor(timbre * 100)}`;

        let source = this.sources[key];
        if (!source) {
            const sampleCount = Math.floor(duration * rate);
            const samples = new Float32Array(sampleCount);
            const p = Math.floor(rate / freq);
            timbre = Math.floor(timbre * p);
            for (let i = 0; i < sampleCount; i++)
                samples[i] = (i % p < timbre) ? 1 : -1;
            source = resource.createAudio(samples, 1);
            this.sources[key] = source;
        }
        audio.replay(source, vol, pan);
    }

    update(deltaT) {
        for (let i = this.notes.length - 1; i >= 0; i--) {
            const note = this.notes[i];
            note.deltaT -= deltaT;
            if (note.deltaT <= 0) {
                this.beep(note.freq, note.duration, note.vol, note.timbre, note.pan);
                this.notes.splice(i, 1);
            }
        }
    }

    play(...notes) {
        let freq = 0, duration = 0, vol = 0, timbre = 0.5, pan = 0, deltaT = 0;
        for (const note of notes) {
            freq = note.freq ?? freq;
            duration = note.duration ?? duration;
            vol = note.vol ?? vol;
            timbre = note.timbre ?? timbre;
            pan = note.pan ?? pan;
            deltaT = (note.deltaT ?? 0) + deltaT;
            if (freq > 0 && duration > 0 && vol > 0) {
                this.notes.push({ freq, duration, vol, timbre, pan, deltaT });
            } else {
                console.log("ignoring", note);
            }
            deltaT += duration;
        }
    }
}

module.exports = new Beeper();
