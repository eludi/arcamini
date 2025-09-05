window.color(0x224466ff);
var numObj = 200;
const objCounts = [ 100, 200, 400, 500, 1000, 2000, 4000, 8000, 16000, 32000 ];

function randi(lo, hi) {
	if(hi===undefined)
		return Math.floor(Math.random()*lo);
	return lo + Math.floor(Math.random()*(hi-lo));
}

var counter = 0, frames=0;
var fps = '', now=0;
const sprites = resource.getTileGrid(resource.getImage('flags.png', 1, 0.5, 0.5), 6,5,2);

const LINE_SIZE = 24;

function Sprite(seed, winSzX, winSzY, szMin, szMax) {
	const type = seed%3;
	if(type==0) { // circle
		this.image = sprites + 29
		this.color = randi(63,0xFFffFFff);
		this.rot = this.velRot = 0;
	}
	else if(type==1) { // rect
		this.image = sprites + 28;
		this.color = randi(63,0xFFffFFff);
		this.rot = this.velRot = 0;
	}
	else if(type==2) { // img
		this.image = sprites + Math.floor(seed/3)%28;
		this.color = 0xFFffFFff;
		this.rot = Math.random()*Math.PI*2;
		this.velRot = Math.random()*Math.PI - Math.PI*0.5;
	}
	this.x = randi(winSzX);
	this.y = randi(winSzY);
	this.velX = randi(-2*szMin, 2*szMin);
	this.velY = randi(-2*szMin, 2*szMin);
	this.id = seed;

	this.draw = function(gfx) {
		gfx.color(this.color);
		gfx.drawImage(this.image, this.x, this.y, this.rot);

	}
}
var objs = [];

function adjustNumObj(count) {
	numObj = count;
}


let prevAxisY = 0;
function input(evt,device,id,value,value2) {
	if (evt === 'axis' && id === 1) {
		if(value === -1.0) {
			if(numObj>objCounts[0]) {
				for(var i=1;i<objCounts.length; ++i)
					if(objCounts[i]==numObj) {
						adjustNumObj(objCounts[i-1]);
						break;
					}
			}
		}
		else if(value === 1.0) {
			if(numObj<objCounts[objCounts.length-1]) {
				for(var i=0;i<objCounts.length-1; ++i)
					if(objCounts[i]==numObj) {
						adjustNumObj(objCounts[i+1]);
						break;
					}
			}
		}
		prevAxisY = value;
	}
}

function update(deltaT) {
	now += deltaT;
	for(let i=0; i<numObj; ++i) {
		const obj = objs[i];
		obj.x += obj.velX * deltaT;
		obj.y += obj.velY * deltaT;
		obj.rot += obj.velRot * deltaT;

		if((obj.id+frames)%15 == 0) {
			const r = 48*1.41;
			if(obj.x>window.width()+r)
				obj.x = -r;
			else if(obj.x<-r)
				obj.x = window.width()+r;
			if(obj.y>window.height()+r)
				obj.y = -r;
			else if(obj.y<-r)
				obj.y = window.height()+r;
		}
	}

	++counter, ++frames;
	if(Math.floor(now)!=Math.floor(now-deltaT)) {
		fps = frames+'fps';
		frames = 0;
	}
	return true;
}

function draw(gfx) {
	// scene:
	for(let i=0; i<numObj; ++i)
		objs[i].draw(gfx);

	// overlay:
	gfx.color(0x7f);
	gfx.fillRect(0, 97, 115, objCounts.length*LINE_SIZE);
	for(var i=0; i<objCounts.length; ++i) {
		gfx.color(objCounts[i]==numObj ? 0xFFffFFff : 0xFFffFF7f)
		gfx.fillText(0, 0,100+i*LINE_SIZE, objCounts[i]);
	}

	gfx.color(0x7f);
	gfx.fillRect(0, window.height()-LINE_SIZE-2, window.width(), LINE_SIZE+2);
	gfx.color(0xFFffFFff);
	gfx.fillText(0, 0,window.height()-20, "arcaqjs graphics performance test");
	gfx.color(0xFF5555FF);
	gfx.fillText(0, window.width()-60, window.height()-20, fps);
}

for(let i=0, end=objCounts[objCounts.length-1]; i<end; ++i)
	objs.push(new Sprite(i, window.width(), window.height(), 16, 64));
adjustNumObj(numObj);
