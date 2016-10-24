function ProgressiveWebMesh(vsCode, fsCode) {
	this.vertShaderCode = vsCode;
    this.fragShaderCode = fsCode;
	this.meshes = [];
	this.currentMesh = 0;
	this.finished = false;
	this.meta = {};
	this.meta.size = 0;
	this.baseSize = 0;	
	this.drawMode = DRAWMODE.PROGRESSIVE;
	this.notDrawnYet = true;
}

var DRAWMODE = { PROGRESSIVE: 1, STATIC: 2};

ProgressiveWebMesh.prototype.load = function(filepath, filename, callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', this.filepath+filename);

    xhr.onload = function () {
        if (xhr.status >= 200 && xhr.status < 400){
            var jsonData = JSON.parse(xhr.responseText);
            if (!jsonData.progressive)
            	console.log("Selected file is not a progressive mesh");

            //not tested
            this.parseJSON(filepath, jsonData, callback);

        }
    }.bind(this);

    xhr.send();
}

ProgressiveWebMesh.prototype.parseJSON = function(filepath, jsonData, callback)  {
	this.filepath = filepath;
	if (!jsonData.progressive){
        console.log("Selected file is not a progressive mesh"); return;
	}
	this.levels = jsonData.progressive;
	for (var i = 0; i < this.levels.length; i++) {
		this.meta.size += this.levels[i].size;
		var theMesh = new WebMesh(this.vertShaderCode, this.fragShaderCode);
		if (this.onLoadProgress)
			theMesh.onLoadProgress = this.onLoadProgress;
		this.meshes.push(theMesh);
	}

	this.meshes[0].parseJSON(this.filepath, this.levels[0], callback);
	this.ready = true;

	this.setKeys();
}

ProgressiveWebMesh.prototype.update = function() {
	if (this.currentMesh < this.meshes.length-1) {
		if (this.meshes[this.currentMesh].ready) {
			this.baseSize += this.levels[this.currentMesh].size;
			this.currentMesh++;
			this.meshes[this.currentMesh].parseJSON(this.filepath, this.levels[this.currentMesh]);
		}
	} else if (!this.finished && this.meshes[this.meshes.length-1].ready){
		if (this.onLoadComplete) this.onLoadComplete();
		if (TIMER){
			console.log("final");
			console.log(window.performance.now() - TIMER);
		}
		this.finished = true;
	}
}

ProgressiveWebMesh.prototype.draw = function(view, proj, light_pos) {
	
	if (this.notDrawnYet){
		if (TIMER){
			console.log("first appear");
			console.log(window.performance.now() - TIMER);
		}
		this.notDrawnYet = false;
	}

	if (this.drawMode == DRAWMODE.PROGRESSIVE) {
		for (var i = this.meshes.length-1; i >= 0; i--) {
			if (this.meshes[i].ready){
				this.meshes[i].draw(view, proj, light_pos);
				break;
			}
		}
	}
	else
		this.meshes[this.selectedLevel].draw(view, proj, light_pos);
}

ProgressiveWebMesh.prototype.setKeys = function() {
	window.addEventListener( 'keydown', function onKeyDown(event) {
		if (!this.finished) return;

		switch ( event.keyCode ) {
		case 49: //"1"
			this.drawMode = DRAWMODE.STATIC;
			this.selectedLevel = 0;
			break;
		case 50: //"2"
			this.drawMode = DRAWMODE.STATIC;
			this.selectedLevel = 1;
			break;
		case 51: //"3"
			this.drawMode = DRAWMODE.STATIC;
			this.selectedLevel = 2;
			break;
		case 91: //"0"
			this.drawMode = DRAWMODE.PROGRESSIVE;
			break;

		}
	}.bind(this), false );
}

