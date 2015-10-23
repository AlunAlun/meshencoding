function B128Mesh(vsCode, fsCode, texSize){
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

B128Mesh.prototype.startLoad = function(filename, callback) {
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
                vertex: {},
                normal: Math.pow(2,this.meta.LOD[this.LOD].bits.normal),
                texture: Math.pow(2, this.meta.LOD[this.LOD].bits.texture)
            }
            //assign w values to dequantize in shader
            if (this.meta.LOD[this.LOD].vertexQuantization == "perVertex"){
                this.meta.wValues.vertex.x = Math.pow(2,this.meta.LOD[this.LOD].bits.vertex.x);
                this.meta.wValues.vertex.y = Math.pow(2,this.meta.LOD[this.LOD].bits.vertex.y);
                this.meta.wValues.vertex.z = Math.pow(2,this.meta.LOD[this.LOD].bits.vertex.z);
            }
            else {
                this.meta.wValues.vertex.x = Math.pow(2,this.meta.LOD[this.LOD].bits.vertex);
                this.meta.wValues.vertex.y = Math.pow(2,this.meta.LOD[this.LOD].bits.vertex);
                this.meta.wValues.vertex.z = Math.pow(2,this.meta.LOD[this.LOD].bits.vertex);
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
            this.tStart= performance.now();
            this.loadData();

            if (callback)
                callback(this.meta.AABB)

        }
    }.bind(this);

    xhr.send();
}

B128Mesh.prototype.loadData = function() {


    var xhr = new XMLHttpRequest();
    xhr.open('GET', this.filepath+this.meta.data);
    xhr.responseType = "arraybuffer";
    xhr.onload = function () {
        var arrayBuffer = xhr.response;
        if (!arrayBuffer){ console.log("no b128 data!"); return;}

        console.log("received all data: "+(performance.now()-this.tStart));

        var byteArray = new Uint8Array(arrayBuffer);

        var dataToRead = this.meta.LOD[this.LOD].vertices*5; //positions and norms
        if (!this.skipTexture)
            dataToRead += this.meta.LOD[this.LOD].vertices*2;

        dataToRead += this.meta.LOD[this.LOD].faces*3;


        var data = new Uint32Array(dataToRead);

        var n = 0;
        var counter = 0;

        var c1, c2, c3;
        var i = new Uint32Array(3); //use this for faster bit shifting
        while (counter < dataToRead) {
            var c1 = byteArray[n++];
            if (c1 & 0x80) { //first bit is set, so we need to read another byte
                c2 = byteArray[n++];
                if (c2 & 0x80) {
                    c3 = byteArray[n++];
                    i[0] = c1; //cast first byte to uint32
                    i[0] &= ~(1 << 7); // reset marker bit to 0

                    i[1] = c2 << 7; //cast second byte uint32
                    i[1] &= ~(1 << 14); //reset marker bit

                    i[2] = c3 << 14;

                    data[counter++] = i[0] | i[1] | i[2]; //join all three
                }
                else {

                    i[0] = c1; //cast first byte to uint32
                    i[0] &= ~(1 << 7); //set marker bit to 0
                    i[1] = c2 << 7; //cast second byte to uint32
                    data[counter++] = i[0] | i[1]; //join both
                }
            }
            else
                data[counter++] = c1;

        }
        console.log(data);
        console.log("read b128: "+(performance.now()-this.tStart));



        this.parseData(data);
    }.bind(this);
    xhr.onprogress=this.onLoadProgress;

    xhr.send();

}

B128Mesh.prototype.parseData = function(data) {
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
    console.log(indexArray);

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

    for (var i = 0; i < utfIndexBuffer; i++) //
    {
        if (this.meta.indexCoding == "delta") {
            //delta decoding
            var result = this.decodeInterleavedInt(indexArray[i]);
            newBuffers.triangles[triCounter++] = prev;
        } else {
            //highwatermark
            var v = indexArray[i];
            v = hi - v;
            newBuffers.triangles[triCounter++] = v;
            hi = Math.max(hi, v + this.meta.max_step);
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

B128Mesh.prototype.decodeInterleavedInt = function(val) {
    if (val%2==0)
        return -1*(val/2)
    else
        return ((val-1)/2)+1
}

B128Mesh.prototype.draw = function(view, proj, light_pos) {



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
        u_wVertexX: this.meta.wValues.vertex.x,
        u_wVertexY: this.meta.wValues.vertex.y,
        u_wVertexZ: this.meta.wValues.vertex.z,
        u_wNormal: this.meta.wValues.normal,
        u_wTexture: this.meta.wValues.texture,
        u_aabbMin: this.meta.AABB.min,
        u_aabbRange: this.meta.AABB.range
    }).draw(this.mesh);
}


B128Mesh.prototype.compileShader = function() {
    if (!this.vertShaderCode | !this.fragShaderCode)
        throw("Mesh doesn't have shader code");

    this.shader = new Shader(this.vertShaderCode, this.fragShaderCode)


}