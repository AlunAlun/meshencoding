
function UTFMesh(vsCode, fsCode, texSize){
	this.vertShaderCode = vsCode;
	this.fragShaderCode = fsCode;
	this.TEX_WIDTH = texSize || 2048;
	this.TEX_HEIGHT = texSize || 2048;
	this.numFaceCounts = [];
	this.currentLevel = 0;
	this.baseFileName = "";
	this.model = mat4.create();
	this.mv = mat4.create();
	this.mvp = mat4.create();
	this.modelt = mat4.create();
	this.onLoadProgress = function (evt) {console.log(parseInt(evt.loaded/1000) + "kB");};
}

UTFMesh.prototype.startLoad = function(filename, callback) {
	this.filename = filename;
	var xhr = new XMLHttpRequest();
	xhr.open('GET', this.filepath+filename);

	xhr.onload = function () {
		if (xhr.status >= 200 && xhr.status < 400){
			var jsonData = JSON.parse(xhr.responseText);
			//clean up metadata
			this.meta = jsonData;
			this.meta.numLODs = this.meta.LOD.length;
			this.LOD = 0;
			this.meta.wValues = {
				vertex: Math.pow(2,this.meta.LOD[this.LOD].bits.vertex),
				normal: Math.pow(2,this.meta.LOD[this.LOD].bits.normal),
				texture: Math.pow(2, this.meta.LOD[this.LOD].bits.texture)
			}
			this.skipTexture = false;
			if (this.meta.LOD[this.LOD].bits.texture == 0)
				this.skipTexture = true;

			//initialise material if it should have one
			this.material = {};
			this.material.diffuse_texture = GL.Texture.fromURL("./assets/white.jpg");
			if (this.meta.material) {
				if (this.meta.material.diffuse_texture) {
					this.material.diffuse_texture = GL.Texture.fromURL(this.filepath+this.meta.material.diffuse_texture,
												{temp_color:[255,255,255,255], minFilter: gl.LINEAR_MIPMAP_LINEAR});
				}
				if (this.meta.material.displacement_texture) {
					this.material.displacement_texture = GL.Texture.fromURL(this.filepath+this.meta.material.displacement_texture,
												{temp_color:[0,0,0,255], minFilter: gl.LINEAR_MIPMAP_LINEAR});
				}
			}
			

			this.loadData();

			if (callback)
				callback(this.meta.AABB)

		}
	}.bind(this);
	xhr.send();
}



UTFMesh.prototype.loadData = function() {
	var xhr = new XMLHttpRequest();
	xhr.open('GET', this.filepath+this.meta.data);
	xhr.setRequestHeader('Content-type', "application/x-www-form-urlencoded; charset=utf-8")
	xhr.overrideMimeType("application/x-www-form-urlencoded; charset=utf-8");

	xhr.onload = function () {
		var data = xhr.response;
		
		this.parseUTFData(data)
	}.bind(this);
	xhr.onprogress=this.onLoadProgress
	xhr.send();
}

var glob = 0;

UTFMesh.prototype.parseUTFData = function(data) {

	var tStart= performance.now();
	var offset = 0;
	var newBuffers = {};
	var aabbMin = this.meta.AABB.min;
	var aabbRange = this.meta.AABB.range;
	var currNumVerts = this.meta.LOD[this.LOD].vertices;
	var currNumIndices = this.meta.LOD[this.LOD].faces*3;
	var utfIndexBuffer = this.meta.LOD[this.LOD].utfIndexBuffer;


	var vertIntsX = new Uint16Array(currNumVerts);
	var vertIntsY = new Uint16Array(currNumVerts);
	var vertIntsZ = new Uint16Array(currNumVerts);
	var vertNormsX = new Uint16Array(currNumVerts);
	var vertNormsY = new Uint16Array(currNumVerts);
	var vertCoordsU = new Uint16Array(currNumVerts);
	var vertCoordsV = new Uint16Array(currNumVerts);
	var indexArray = new Uint32Array(utfIndexBuffer);

	for (var i = 0; i < currNumVerts; i++) {vertIntsX[i] = data.charCodeAt(offset);offset++;}
	for (var i = 0; i < currNumVerts; i++) {vertIntsY[i] = data.charCodeAt(offset);offset++}
	for (var i = 0; i < currNumVerts; i++) {vertIntsZ[i] = data.charCodeAt(offset);offset++}
	for (var i = 0; i < currNumVerts; i++) {vertNormsX[i] = data.charCodeAt(offset);offset++}
	for (var i = 0; i < currNumVerts; i++) {vertNormsY[i] = data.charCodeAt(offset);offset++}
	if (!this.skipTexture){
		for (var i = 0; i < currNumVerts; i++) {vertCoordsU[i] = data.charCodeAt(offset);offset++}
		for (var i = 0; i < currNumVerts; i++) {vertCoordsV[i] = data.charCodeAt(offset);offset++}
	}
	for (var i = 0; i < utfIndexBuffer; i++) {indexArray[i] = data.charCodeAt(offset);offset++;}


	console.log("parse buffers: "+(performance.now()-tStart));

	newBuffers.vertices = new Float32Array(currNumVerts*3);
	lastIndexX = 0; lastIndexY = 0; lastIndexZ = 0;
	for (var i = 0; i < currNumVerts; i++){
		//x
		var result = this.decodeSafeInterleavedUTF(vertIntsX, i);
		
		delta = result.delta; i = result.i;
		newVal = lastIndexX+delta;
		lastIndexX = newVal;
		newBuffers.vertices[i*3] = newVal
	}
	for (var i = 0; i < currNumVerts; i++){
		//y
		var result = this.decodeSafeInterleavedUTF(vertIntsY, i);
		delta = result.delta; i = result.i;
		newVal = lastIndexY+delta;
		lastIndexY = newVal;
		newBuffers.vertices[i*3+1] = newVal
	}
	for (var i = 0; i < currNumVerts; i++){
		//z
		var result = this.decodeSafeInterleavedUTF(vertIntsZ, i);
		delta = result.delta; i = result.i;
		newVal = lastIndexZ+delta;
		lastIndexZ = newVal;
		newBuffers.vertices[i*3+2] = newVal;	
	}

	console.log("set verts: "+(performance.now()-tStart));

	newBuffers.normals = new Float32Array(currNumVerts*2);
	lastIndexX = 0; lastIndexY = 0; 
	for (var i = 0; i < currNumVerts; i++){
		//x
		delta = this.decodeInterleavedInt(vertNormsX[i]);
		newVal = lastIndexX+delta;
		lastIndexX = newVal
		newBuffers.normals[i*2] = newVal;
	}
	for (var i = 0; i < currNumVerts; i++){
		//y
		delta = this.decodeInterleavedInt(vertNormsY[i]);
		newVal = lastIndexY+delta;
		lastIndexY = newVal
		newBuffers.normals[i*2+1] = newVal;
	}

	console.log("set norms: "+(performance.now()-tStart));

	if (!this.skipTexture){
		lastIndexU = 0; lastIndexV = 0;
		newBuffers.coords = new Float32Array(currNumVerts*2);
		for (var i = 0; i < currNumVerts; i++){
			//u
			delta = this.decodeInterleavedInt(vertCoordsU[i]);
			newVal = lastIndexU+delta;
			lastIndexU = newVal;
			newBuffers.coords[i*2] = newVal;
		}
		for (var i = 0; i < currNumVerts; i++){
			//v
			delta = this.decodeInterleavedInt(vertCoordsV[i]);
			newVal = lastIndexV+delta;
			lastIndexV = newVal;
			newBuffers.coords[i*2+1] = newVal;

		}

		console.log("set coords: "+(performance.now()-tStart));
	}

	newBuffers.triangles = new Uint32Array(utfIndexBuffer);
	console.log(currNumIndices);
	console.log(utfIndexBuffer);

	lastIndex = 0;

    if (!this.meta.max_step)
        this.meta.max_step = 1;
    nextHighWaterMark = this.meta.max_step - 1;
    hi = this.meta.max_step - 1;
	triCounter = 0;
	var prev = 0;
	console.log("-");

	for (var i = 0; i < utfIndexBuffer; i++) //utfIndexBuffer
	{
		if (this.meta.indexCoding == "delta") {
			//delta decoding
			var result = this.decodeSafeInterleavedUTF(indexArray, i);
			prev += result.delta;
			i = result.i;
			newBuffers.triangles[triCounter++] = prev;
		}
		else {
			//highwatermark
            var result = this.decodeSafeUTF(indexArray, i);
            var v = result.value;
            i = result.i;

            //assert(v <=hi)
            v = hi - v;
            newBuffers.triangles[triCounter++] = v;
            hi = Math.max(hi, v + this.meta.max_step);



			//var result = this.decodeSafeUTF(indexArray, i);
            //var code = result.value;
            //i = result.i;
            //
            //newBuffers.triangles[triCounter++] = nextHighWaterMark - code;
            //if (code === 0)
            //	nextHighWaterMark+=this.meta.max_step;
		}
	}

    if (this.meta.indexCompression == "pairedtris") {
        newArray = new Uint32Array(utfIndexBuffer*1.5);
        var nACounter = 0;
        for (var base = 0; base < newBuffers.triangles.length;) {
            var a = newBuffers.triangles[base++];
            var b = newBuffers.triangles[base++];
            var c = newBuffers.triangles[base++];
            newArray[nACounter++] = a;
            newArray[nACounter++] = b;
            newArray[nACounter++] = c;

            if (a < b) {
                var d = newBuffers.triangles[base++];
                newArray[nACounter++] = a;
                newArray[nACounter++] = d;
                newArray[nACounter++] = b;
            }
        }
        newBuffers.triangles = newArray;
    }



	console.log("set inds: "+(performance.now()-tStart));

	newBuffers.ids = new Float32Array(currNumVerts)
	for (var i = 0; i < currNumVerts; i++) newBuffers.ids[i] = i;

	this.buffers = newBuffers;

	//create mesh now 
	options = {};
	this.mesh = Mesh.load(this.buffers, options);
	//add ids attribute
	this.mesh.createVertexBuffer("id", "a_id", 1, this.buffers.ids);
	//remove existing normal attribute and add new 2 component compressed normal
	delete this.mesh.vertexBuffers.normals; 
	this.mesh.createVertexBuffer("normals2", "a_normal", 2, this.buffers.normals); 

	this.compileShader();
	this.ready = true;
	console.log("ready "+(performance.now()-tStart));
	console.log ("base file ready for display");

	if (this.LOD < this.meta.numLODs-1) { //this.LOD is 0 based, this numLODs starts counting from 1
		this.LOD += 1;
		//TODO update with new LOD!
	}

	return;


}

UTFMesh.prototype.decodeSafeInterleavedUTF = function(array, i) {
	var delta = 0;
	var dodgy = false;
	if (array[i] == 55295) {
		i++;
		delta = this.decodeInterleavedInt(array[i]);
		if (delta < 0)
			delta -= 14000;
		else
			delta += 14000;
	} 
	else if (array[i] >= 55296 && array[i] <= 57343) {
		var interLeavedVal = surrogatePairToCodePoint(array[i], array[i+1]);
		delta = this.decodeInterleavedInt(interLeavedVal);
		dodgy = true;
		i++;

	}
	else
		delta = this.decodeInterleavedInt(array[i]);

	return {delta, i, dodgy};
}

UTFMesh.prototype.decodeSafeUTF = function(array, i) {
	var value = 0;
	var dodgy = false;
	if (array[i] == 55295) {
		i++;
		value = array[i] + 14000;
	}
	else if (array[i] >= 55296 && array[i] <= 57343) {
		value = surrogatePairToCodePoint(array[i], array[i+1]);
		dodgy = true;
		i++;
		glob++;
	}
	else
		value = array[i];
	return {value, i, dodgy}
}

UTFMesh.prototype.parseBinaryUpdateFile = function(data) {
	//TODO adapt old progressive code for new stuff

}

UTFMesh.prototype.draw = function(view, proj, light_pos) {

	

	//create modeview matrix
	mat4.multiply(this.mv,view,this.model);
	//create mvp matrix
	mat4.multiply(this.mvp,proj,this.mv);
	//create world-normal matrix
	mat4.toRotationMat4(this.modelt, this.model);

	if (this.material){
		if (this.material.diffuse_texture)
			this.material.diffuse_texture.bind(2);
		if (this.material.displacement_texture) 
			this.material.displacement_texture.bind(3);
	}

	this.shader.uniforms({
		u_model:this.model,
		u_modelt:this.modelt,
		u_mvp: this.mvp,
		u_lightPosition: light_pos,
		u_materialColor: [1,1,1],
		u_diffuse_texture: 2,
		u_displacement_Texture: 2,
		u_wVertex: this.meta.wValues.vertex,
		u_wNormal: this.meta.wValues.normal,
		u_wTexture: this.meta.wValues.texture,
		u_aabbMin: this.meta.AABB.min,
		u_aabbRange: this.meta.AABB.range
	}).draw(this.mesh);
}


UTFMesh.prototype.compileShader = function() {
	if (!this.vertShaderCode | !this.fragShaderCode)
		throw("Mesh doesn't have shader code");

	this.shader = new Shader(this.vertShaderCode, this.fragShaderCode)


}

UTFMesh.prototype.decodeInterleavedInt = function(val) {
	if (val%2==0)
		return -1*(val/2)
	else
		return ((val-1)/2)+1
}

UTFMesh.prototype.decodeInterleavedInt2 = function(val) {
	return (val >> 1) ^ (-(val & 1));
}

String.prototype.replaceLastCharacter = function(newChar) {

	return this.slice(0,this.length-1)+newChar;
}

// helper functions

function surrogatePairToCodePoint(charCode1, charCode2) {
    return ((charCode1 & 0x3FF) << 10) + (charCode2 & 0x3FF) + 0x10000;
}

function stringToCodePointArray(str) {
    var codePoints = [], i = 0, charCode;
    while (i < str.length) {
        charCode = str.charCodeAt(i);
        if ((charCode & 0xF800) == 0xD800) {
            codePoints.push(surrogatePairToCodePoint(charCode, str.charCodeAt(++i)));
        } else {
            codePoints.push(charCode);
        }
        ++i;
    }
    return codePoints;
}

function codePointArrayToString(codePoints) {
    var stringParts = [];
    for (var i = 0, len = codePoints.length, codePoint, offset, codePointCharCodes; i < len; ++i) {
        codePoint = codePoints[i];
        if (codePoint > 0xFFFF) {
            offset = codePoint - 0x10000;
            codePointCharCodes = [0xD800 + (offset >> 10), 0xDC00 + (offset & 0x3FF)];
        } else {
            codePointCharCodes = [codePoint];
        }
        stringParts.push(String.fromCharCode.apply(String, codePointCharCodes));
    }
    return stringParts.join("");
}

function clamp(value, min, max) {
    
    return value < min ? min : value > max ? max : value;
};

function fromSNorm(value) {
	return clamp(value, 0.0, 255.0) / 255.0 * 2.0 - 1.0;
}

function signNotZero(value) {
    return value < 0.0 ? -1.0 : 1.0;
};

function octDecode(x, y) {

    if (x < 0 || x > 255 || y < 0 || y > 255) {
        throw new Error('x and y must be a signed normalized integer between 0 and 255');
    }

    var theX = fromSNorm(x);
    var theY = fromSNorm(y);
    var theZ = 1.0 - (Math.abs(theX) + Math.abs(theY));

    var result = vec3.set(vec3.create(), theX,theY,theZ);

    if (result[2] < 0.0)
    {
        var oldVX = result[0];
        result[0] = (1.0 - Math.abs(result[1])) * signNotZero(oldVX);
        result[1] = (1.0 - Math.abs(oldVX)) * signNotZero(result[1]);
    }

    return vec3.normalize(result, result);
}
