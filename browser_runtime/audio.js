arcamini.audio = (function() {

let audioCtx = window.AudioContext ? new AudioContext() : new webkitAudioContext();
const numTracksMax = 16;
let samples = [], tracks=[];
for(let i=0; i<numTracksMax; ++i)
	tracks.push(null);

let masterVolume = audioCtx.createGain();
masterVolume.gain.value = 1.0;
masterVolume.connect(audioCtx.destination);

/// loads sample asynchronously
function loadSample(url, id, callback) {
	let request = new XMLHttpRequest();
	request.open("GET", url, true);
	request.responseType = "arraybuffer";

	request.onload = ()=>{
		// asynchronously decode the audio file data in request.response
		audioCtx.decodeAudioData(
			request.response,
			(buffer)=>{
				if(!buffer) {
					console.error('error decoding file data:', id);
					if(callback)
						callback();
					return;
				}
				let sample = samples[id-1];
				sample.buffer = buffer;
				sample.ready = true;
				const arr = buffer.getChannelData(0);
				for(let i=0; i<arr.length; ++i)
					if(arr[i]>0.002 || arr[i]<-0.002) {
						sample.offset = i/buffer.sampleRate;
						break;
					}
				if(callback)
					callback(sample);
			},
			(error)=>{
				console.error('decodeAudioData error', id, error);
				if(callback)
					callback();
			}
		);
	}
	request.onerror = ()=>{
		console.error('loadSample: XHR error', url);
		if(callback)
			callback();
	}
	request.send();
}

function findAvailableTrack() {
	let trackId=0;
	for( ; trackId<numTracksMax; ++trackId)
		if(tracks[trackId]===null)
			return trackId;
	return numTracksMax;
 }

function connectSource(source, gain, pan) {
	let gainNode = audioCtx.createGain();
	gainNode.gain.value = gain;
	source.connect(gainNode);
	let pred = gainNode;

	if(pan) {
		let panNode;
		if (audioCtx.createStereoPanner) {
			panNode = audioCtx.createStereoPanner();
			panNode.pan.value = pan;
		}
		else {
			panNode = audioCtx.createPanner();
			panNode.panningModel = 'equalpower';
			panNode.setPosition(pan, 0, 1 - Math.abs(pan));
		}
		pred.connect(panNode);
		pred = panNode;
	}
	pred.connect(masterVolume);
	return gainNode;
}

return {
	resume: function() {
		if(audioCtx.state !== 'running')
			audioCtx.resume();
	},
	load: function(url, params, callback) {
		if(Array.isArray(url)) {
			let samples = [];
			for(let i=0; i<url.length; ++i)
				samples.push(this.load(url[i], params, callback));
			return samples;
		}
		let sample = { id:samples.length+1, ready:false, url:url, buffer:null, offset:0 };
		samples.push(sample);
		loadSample(url, sample.id, callback);
		return sample.id;
	},
	replay: function(id, gain=1.0, pan=0, detune=0) {
		if(id===0 || id>samples.length)
			return;
		const sample = samples[id-1];
		if(!sample.ready || !sample.buffer)
			return;
		let trackId = findAvailableTrack();
		if(trackId === numTracksMax)
			return 0xffffffff;

		let source = audioCtx.createBufferSource();
		if(detune!==0)
			source.playbackRate.value = Math.pow(2, detune/12);
		source.buffer = sample.buffer;
		tracks[trackId] = { src:source, gain:connectSource(source, gain, pan) };
		source.start(audioCtx.currentTime, sample.offset || 0);
		source.addEventListener('ended', ()=>{ tracks[trackId]=null; })
		return trackId;
	},
	stop: function(track) {
		const tr = tracks[track];
		if(!tr)
			return;
		tr.src.stop();
		tracks[track] = null;
	},
	volume: function(track, gain) {
		const tr = tracks[track];
		if(!tr)
			return;
		tr.gain.gain.value = gain;
	},
	fadeOut: function(track, duration) {
		const tr = tracks[track];
		if(!tr)
			return;
		tr.gain.gain.linearRampToValueAtTime(0, audioCtx.currentTime + duration);
		tr.src.stop(audioCtx.currentTime + duration);
	},
	uploadPCM: function(data, numChannels=1) {
		const buffer = audioCtx.createBuffer(numChannels, data.length/numChannels, audioCtx.sampleRate);
		for(let j=0; j<numChannels; ++j) {
			const channel = buffer.getChannelData(j);
			for(let i=0, end=data.length/numChannels; i<end; ++i)
				channel[i] = data[i*numChannels+j];
		}
		samples.push({ id:samples.length+1, ready:true, url:'', buffer:buffer, offset:0 });
		return samples.length;
	},
	sampleBuffer: function(id) {
		if(id>0 && id<=samples.length)
			return samples[id-1].buffer;
	}
}

})();
