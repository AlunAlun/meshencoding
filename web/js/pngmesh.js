function PNGMesh(vsCode, fsCode, texSize){
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

PNGMesh.prototype.startLoad = function(filename, callback) {
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

PNGMesh.prototype.loadData = function() {

    var meshImage = new Image();
    meshImage.src = this.filepath+this.meta.data;
    meshImage.onload = function(){
        this.tStart= performance.now();
        var canvas = document.createElement('canvas');
        canvas.width = meshImage.width;
        canvas.height = meshImage.height;
        canvas.getContext('2d').drawImage(meshImage, 0, 0, meshImage.width, meshImage.height);
        var vCounter = 0;
        var iCounter = 0;
        var data = [];

        var vertexMuliplier = 5;
       // if (this.meta.LOD[this.LOD].bits.texture != 0)
            vertexMuliplier = 7;
        var dataEntries = this.meta.LOD[this.LOD].vertices*vertexMuliplier;
        dataEntries+= this.meta.LOD[this.LOD].faces*3;
        numRowsToRead= Math.ceil(dataEntries/4096);


        var pixelData = canvas.getContext('2d').getImageData(0, 0, meshImage.width, 4096).data;

        //console.log(pixelData);
        for (var i = 0; i < dataEntries*4; i+=4) {
            var RGB = (pixelData[i] << 16);
            RGB = RGB | (pixelData[i+1] << 8);
            RGB = RGB | (pixelData[i+2]);
            data.push(RGB);
        }

        console.log("read png: "+(performance.now()-this.tStart));
        this.parseUTFData(data);

    }.bind(this);

    //todo onloadprogress
}

PNGMesh.prototype.parseUTFData = function(data) {
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
    var indexArray = new Uint32Array(currNumIndices);

    for (var i = 0; i < currNumVerts; i++) {vertIntsX[i] = data[offset];offset++;}
    for (var i = 0; i < currNumVerts; i++) {vertIntsY[i] = data[offset];offset++}
    for (var i = 0; i < currNumVerts; i++) {vertIntsZ[i] = data[offset];offset++}
    for (var i = 0; i < currNumVerts; i++) {vertNormsX[i] = data[offset];offset++}
    for (var i = 0; i < currNumVerts; i++) {vertNormsY[i] = data[offset];offset++}
    if (!this.skipTexture){
        for (var i = 0; i < currNumVerts; i++) {vertCoordsU[i] = data[offset];offset++}
        for (var i = 0; i < currNumVerts; i++) {vertCoordsV[i] = data[offset];offset++}
    }
    for (var i = 0; i < currNumIndices; i++) {indexArray[i] = data[offset];offset++;}


    console.log("parse buffers: "+(performance.now()-this.tStart));

    newBuffers.vertices = new Float32Array(currNumVerts*3);
    lastIndexX = 0; lastIndexY = 0; lastIndexZ = 0;
    for (var i = 0; i < currNumVerts; i++){
        //x
        delta = this.decodeInterleavedInt(vertIntsX[i]);

        newVal = lastIndexX+delta;
        lastIndexX = newVal;
        newBuffers.vertices[i*3] = newVal
    }
    for (var i = 0; i < currNumVerts; i++){
        //y
        delta= this.decodeInterleavedInt(vertIntsY[i]);

        newVal = lastIndexY+delta;
        lastIndexY = newVal;
        newBuffers.vertices[i*3+1] = newVal
    }
    for (var i = 0; i < currNumVerts; i++){
        //z
        delta = this.decodeInterleavedInt(vertIntsZ[i]);

        newVal = lastIndexZ+delta;
        lastIndexZ = newVal;
        newBuffers.vertices[i*3+2] = newVal;
    }

    console.log("set verts: "+(performance.now()-this.tStart));

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

    console.log("set norms: "+(performance.now()-this.tStart));

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

        console.log("set coords: "+(performance.now()-this.tStart));
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

    for (var i = 0; i < currNumIndices; i++)
    {
        //highwatermark
        var v = indexArray[i];
        v = hi - v;
        newBuffers.triangles[triCounter++] = v;
        hi = Math.max(hi, v + this.meta.max_step);
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

    console.log("set inds: "+(performance.now()-this.tStart));

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
    console.log("ready "+(performance.now()-this.tStart));
    console.log ("base file ready for display");

    if (this.LOD < this.meta.numLODs-1) { //this.LOD is 0 based, this numLODs starts counting from 1
        this.LOD += 1;
        //TODO update with new LOD!
    }

    return;


}

PNGMesh.prototype.decodeInterleavedInt = function(val) {
    if (val%2==0)
        return -1*(val/2)
    else
        return ((val-1)/2)+1
}

PNGMesh.prototype.draw = function(view, proj, light_pos) {



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


PNGMesh.prototype.compileShader = function() {
    if (!this.vertShaderCode | !this.fragShaderCode)
        throw("Mesh doesn't have shader code");

    this.shader = new Shader(this.vertShaderCode, this.fragShaderCode)


}