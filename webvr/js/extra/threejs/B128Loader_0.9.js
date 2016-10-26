/**
 * @author Alun Evans / http://alunevans.info/
 */

THREE.B128Loader = function (options, manager ) {

	this.options = options || {};
	this.manager = ( manager !== undefined ) ? manager : THREE.DefaultLoadingManager;

	this.objects = [];
	this.b128Offset = 0;

};

THREE.B128Loader.prototype = {

	constructor: THREE.B128Loader,

	load: function( url, onLoad, onProgress, onError ) {
		var scope = this;



		splPath = this.splitPath(url);
		this.filepath = splPath.dirPart;


		var loader = new THREE.XHRLoader( scope.manager );
		loader.setCrossOrigin( this.crossOrigin );
		loader.load( url, function ( text ) {

			scope.parseJSON( text, onLoad );

		}, onProgress, onError );
	},
	setCrossOrigin: function ( value ) {

		this.crossOrigin = value;

	},
	parseJSON: function ( text, onLoad ) {

		this.meta = JSON.parse(text);

		for (var i in this.meta.meshes) {
			this.meta.meshes[i].wValues = {
		        vertex: {},
		        normal: Math.pow(2,this.meta.meshes[i].bits.normal),
		        texture: Math.pow(2, this.meta.meshes[i].bits.texture)
		    }
		    //assign w values to dequantize in shader
		    if (this.meta.meshes[i].vertexQuantization == "perVertex"){
		        this.meta.meshes[i].wValues.vertex.x = Math.pow(2,this.meta.meshes[i].bits.vertex.x);
		        this.meta.meshes[i].wValues.vertex.y = Math.pow(2,this.meta.meshes[i].bits.vertex.y);
		        this.meta.meshes[i].wValues.vertex.z = Math.pow(2,this.meta.meshes[i].bits.vertex.z);
		    }
		    else {
		        this.meta.meshes[i].wValues.vertex.x = Math.pow(2,this.meta.meshes[i].bits.vertex);
		        this.meta.meshes[i].wValues.vertex.y = Math.pow(2,this.meta.meshes[i].bits.vertex);
		        this.meta.meshes[i].wValues.vertex.z = Math.pow(2,this.meta.meshes[i].bits.vertex);
		    }

		    this.meta.meshes[i].skipTexture = false;
		    if (this.meta.meshes[i].bits.texture == 0)
		        this.meta.meshes[i].skipTexture = true;
		}

		//create materials
		this.createTHREEMaterials();

	    
	    var xhr = new XMLHttpRequest();
	    xhr.open('GET', this.filepath+this.meta.meshes[0].data);
	    xhr.responseType = "arraybuffer";
	    xhr.onload = function () {
	        var arrayBuffer = xhr.response;
	  
	        if (!arrayBuffer){ console.log("no b128 data!"); return;}
	        this.parseB128(arrayBuffer, onLoad)
	    }.bind(this);
	   	xhr.onprogress=this.onLoadProgress;
	    xhr.send();
	},
	parseB128: function(arrayBuffer, onLoad) {

		console.time( 'B128Loader' );

		//cast buffer to a Uint8 array
		var byteArray = new Uint8Array(arrayBuffer);

		//size in bytes of byteArray is obviously same as number of elements 
		//(cos it's Uint8)

        var data = new Uint32Array(byteArray.length);

        var n = 0;
        var counter = 0;

        var c1, c2, c3;
        var i = new Uint32Array(3); //use this for faster bit shifting
        while (counter < byteArray.length) {
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

        for (var i in this.meta.meshes)
		this.parseOneMeshData(data, this.meta.meshes[i]);
        this.createTHREEMeshes(onLoad)
    },
    parseOneMeshData: function(data, meshMeta) {

	    var newBuffers = {};
	    var aabbMin = meshMeta.AABB.min;
	    var aabbRange = meshMeta.AABB.range;
	    var currNumVerts = meshMeta.vertices;
	    var currNumIndices = meshMeta.indexBufferSize;
	    var utfIndexBuffer = meshMeta.utfIndexBuffer;

	    var normEnc = meshMeta.normalEncoding;

	    var vertIntsX = new Uint16Array(currNumVerts);
	    var vertIntsY = new Uint16Array(currNumVerts);
	    var vertIntsZ = new Uint16Array(currNumVerts);
	    var vertNormsX = new Uint16Array(currNumVerts);
	    var vertNormsY, vertNormsZ;
	    if (normEnc == "octahedral" || normEnc == "quantisation") 
	        vertNormsY = new Uint16Array(currNumVerts);
	    if (normEnc == "quantisation")
	        vertNormsZ = new Uint16Array(currNumVerts);  
	    var vertCoordsU = new Uint32Array(currNumVerts);
	    var vertCoordsV = new Uint32Array(currNumVerts);
	    var indexArray = new Uint32Array(currNumIndices);

	    
	    for (var i = 0; i < currNumVerts; i++) {vertIntsX[i] = data[this.b128Offset];this.b128Offset++;}
	    for (var i = 0; i < currNumVerts; i++) {vertIntsY[i] = data[this.b128Offset];this.b128Offset++}
	    for (var i = 0; i < currNumVerts; i++) {vertIntsZ[i] = data[this.b128Offset];this.b128Offset++}
	    for (var i = 0; i < currNumVerts; i++) {vertNormsX[i] = data[this.b128Offset];this.b128Offset++}
	    if (normEnc == "octahedral" || normEnc == "quantisation")  
	        for (var i = 0; i < currNumVerts; i++) {vertNormsY[i] = data[this.b128Offset];this.b128Offset++}
	    if (normEnc == "quantisation")
	        for (var i = 0; i < currNumVerts; i++) {vertNormsZ[i] = data[this.b128Offset];this.b128Offset++}
	    if (!meshMeta.skipTexture){
	        for (var i = 0; i < currNumVerts; i++) {vertCoordsU[i] = data[this.b128Offset];this.b128Offset++}
	        for (var i = 0; i < currNumVerts; i++) {vertCoordsV[i] = data[this.b128Offset];this.b128Offset++}
	    }
	    for (var i = 0; i < currNumIndices; i++) {indexArray[i] = data[this.b128Offset];this.b128Offset++;}
	    


	    newBuffers.vertices = new Float32Array(currNumVerts*3);
	    var delta = 0; var lastIndexX = 0; var lastIndexY = 0; var lastIndexZ = 0;
	    for (var i = 0; i < currNumVerts; i++){
	        //x
	        delta = this.decodeInterleavedInt(vertIntsX[i]);
	        newVal = lastIndexX+delta;
	        lastIndexX = newVal;
	        // newBuffers.vertices[i*3] = newVal;
	        newBuffers.vertices[i*3] = 
	        	(newVal/meshMeta.wValues.vertex.x)*meshMeta.AABB.range[0]+meshMeta.AABB.min[0];
	    }
	    for (var i = 0; i < currNumVerts; i++){
	        //y
	        delta= this.decodeInterleavedInt(vertIntsY[i]);
	        newVal = lastIndexY+delta;
	        lastIndexY = newVal;
	        // newBuffers.vertices[i*3+1] = newVal
	        newBuffers.vertices[i*3+1] = 
	        	(newVal/meshMeta.wValues.vertex.y)*meshMeta.AABB.range[1]+meshMeta.AABB.min[1];
	    }
	    for (var i = 0; i < currNumVerts; i++){
	        //z
	        delta = this.decodeInterleavedInt(vertIntsZ[i]);
	        newVal = lastIndexZ+delta;
	        lastIndexZ = newVal;
	        // newBuffers.vertices[i*3+2] = newVal;
	        newBuffers.vertices[i*3+2] = 
	        	(newVal/meshMeta.wValues.vertex.z)*meshMeta.AABB.range[2]+meshMeta.AABB.min[2];
	    }

	    //normals

	    newBuffers.normals = new Float32Array(currNumVerts*3);
	    lastIndexX = 0; lastIndexY = 0; lastIndexZ = 0;

	    var fibIndex = 0; var decodedNormal;
	    if (normEnc == "fibonacci") {
	    	//create fibonacci sphere if required
	        var fibSphere = computeFibonacci_sphere(meshMeta.fibonacciLevel);
	        for (var i = 0; i < currNumVerts; i++){
		        //there is only one coordinate, which is the sphere index
		        delta = this.decodeInterleavedInt(vertNormsX[i]);
		        newVal = lastIndexX+delta;
		        lastIndexX = newVal;
		        fibIndex = newVal;
	            newBuffers.normals[i*3] = fibSphere[fibIndex*3];
	            newBuffers.normals[i*3+1] = fibSphere[fibIndex*3+1];
	            newBuffers.normals[i*3+2] = fibSphere[fibIndex*3+2];
	        }
	    }

	    else if (normEnc == "octahedral") {
	    	for (var i = 0; i < currNumVerts; i++){
	    		//x
				delta = this.decodeInterleavedInt(vertNormsX[i]);
		        lastIndexX = lastIndexX+delta;
		        //y
	            delta = this.decodeInterleavedInt(vertNormsY[i]);
	            lastIndexY = lastIndexY+delta;
	            //decode
	            decodedNormal = this.octDecode(lastIndexX, lastIndexY);
	            newBuffers.normals[i*3] = decodedNormal[0];
	            newBuffers.normals[i*3+1] = decodedNormal[1];
	            newBuffers.normals[i*3+2] = decodedNormal[2];
	        }

	    }
	    else if (normEnc == "quantisation") {
	    	for (var i = 0; i < currNumVerts; i++){
	    		//x
				delta = this.decodeInterleavedInt(vertNormsX[i]);
		        lastIndexX = lastIndexX+delta;
		        newBuffers.normals[i*3] = (lastIndexX/(meshMeta.wValues.normal-1.0))*2.0-1.0;
				//y
	            delta = this.decodeInterleavedInt(vertNormsY[i]);
	            lastIndexY = lastIndexY+delta;
	            newBuffers.normals[i*3+1] = (lastIndexY/(meshMeta.wValues.normal-1.0))*2.0-1.0;;
	            //z
	            delta = this.decodeInterleavedInt(vertNormsZ[i]);
	            lastIndexZ = lastIndexZ+delta;
	            newBuffers.normals[i*3+2] = (lastIndexZ/(meshMeta.wValues.normal-1.0))*2.0-1.0;
	    	}
	    }



	    if (!meshMeta.skipTexture){
	        lastIndexU = 0; lastIndexV = 0;
	        newBuffers.coords = new Float32Array(currNumVerts*2);
	        for (var i = 0; i < currNumVerts; i++){
	            //u
	            delta = this.decodeInterleavedInt(vertCoordsU[i]);
	            lastIndexU = lastIndexU+delta;
	            //v
	            delta = this.decodeInterleavedInt(vertCoordsV[i]);
	            lastIndexV = lastIndexV+delta;
	            //set
	            newBuffers.coords[i*2+1] = lastIndexV/meshMeta.wValues.texture;
	            newBuffers.coords[i*2] = lastIndexU/meshMeta.wValues.texture;

	        }


	    }

	    newBuffers.triangles = new Uint32Array(utfIndexBuffer);

	    lastIndex = 0;
	    if (!meshMeta.max_step)
	        meshMeta.max_step = 1;
	    nextHighWaterMark = meshMeta.max_step - 1;
	    hi = meshMeta.max_step - 1;
	    triCounter = 0;
	    var prev = 0;
	    var result = 0;
	    var v = 0;
	    for (var i = 0; i < utfIndexBuffer; i++) //
	    {
	        if (meshMeta.indexCoding == "delta") {
	            //delta decoding
	            result = this.decodeInterleavedInt(indexArray[i]);
	            prev += result;
	            newBuffers.triangles[triCounter++] = prev;
	        } else {
	            //highwatermark
	            v = indexArray[i];
	            v = hi - v;
	            newBuffers.triangles[triCounter++] = v;
	            hi = Math.max(hi, v + meshMeta.max_step);
	        }
	    }

	    if (meshMeta.indexCompression == "pairedtris") {
	        newArray = new Uint32Array(meshMeta.faces*3);
	        var nACounter = 0;
	        for (var base = 0; base < newBuffers.triangles.length;) {//
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

		var object;

		object = {
			name: meshMeta.name,
			geometry: newBuffers,
			mat_id: meshMeta.mat_id,
			reconstructNormals: meshMeta.reconstructNormals
		};

		this.objects.push( object );
		console.log("Parsed object "+meshMeta.name+": "+ this.objects.length);

	},
	createTHREEMeshes: function(onLoad) {
		var container = new THREE.Object3D();

		for ( var i = 0, l = this.objects.length; i < l; i ++ ) {

			//assign geometry
			object = this.objects[ i ];
			geometry = object.geometry;

			var buffergeometry = new THREE.BufferGeometry();

			buffergeometry.addAttribute( 'position', new THREE.BufferAttribute( new Float32Array( object.geometry.vertices ), 3 ) );


			if (object.geometry.coords && object.geometry.coords.length > 0 ) {
				buffergeometry.addAttribute( 'uv', new THREE.BufferAttribute( new Float32Array( object.geometry.coords ), 2 ) );
			}

			if (object.geometry.triangles && object.geometry.triangles.length > 0) {
				buffergeometry.setIndex(new THREE.BufferAttribute( object.geometry.triangles, 1 ))
			}


			if (object.reconstructNormals == true || object.geometry.normals.length == 0 ) {
				buffergeometry.computeFaceNormals();
				buffergeometry.computeVertexNormals();
			}
			else {
				buffergeometry.addAttribute( 'normal', new THREE.BufferAttribute( new Float32Array( object.geometry.normals ), 3 ) );
			}
			




			//create mesh object
			var mesh = new THREE.Mesh( buffergeometry);
			mesh.name = object.name; //TODO assign objects a name

			//assign material if it exists, if not assign a default Phong material
			if (object.mat_id != -1)
				mesh.material = this.materials[object.mat_id];
			else
				mesh.material = new THREE.MeshPhongMaterial({ color: 0xdddddd, specular: 0xCCCC99, shininess: 30 });

			mesh.geometry.computeBoundingSphere();

			//add to generic container
			container.add( mesh );

		}
		console.timeEnd( 'B128Loader' );
		
		this.container = container;

		if (onLoad) 
			onLoad(this);
	},
	createTHREEMaterials: function(){
		this.materials = [];
		for (var i = 0; i < this.meta.materials.length; i++) {
			var currMat = this.meta.materials[i];
			var threeMat;
			if (this.options.material == "basic")	
				threeMat = new THREE.MeshBasicMaterial();
			else
				threeMat = new THREE.MeshPhongMaterial();

			if (currMat.name != "")
				threeMat.name = currMat.name;


			if ((c = currMat.Kd) != ""){ //diffuse color
				if (c[0] == 0 && c[1] == 0 && c[2] == 0) {
					threeMat.color = new THREE.Color(1, 1, 1);
					console.warn("THREE.B128Loader: The material has no diffuse color, substituting 1...")
				}
				else{
					threeMat.color = new THREE.Color(c[0], c[1], c[2]);
				}
			}
			if (currMat.map_Kd != ""){ //diffuse color map
				threeMat.map = THREE.ImageUtils.loadTexture(
					this.filepath+currMat.map_Kd,
					THREE.UVMapping,
					function(tex) {
						tex.wrapS = THREE.MirroredRepeatWrapping;
						tex.wrapT = THREE.MirroredRepeatWrapping;
					}
				);
			}
			if (currMat.map_Ks != ""){ //specular color map
				threeMat.specularMap = THREE.ImageUtils.loadTexture(
					this.filepath+currMat.map_Ks,
					THREE.UVMapping,
					function(tex) {
						tex.wrapS = THREE.MirroredRepeatWrapping;
						tex.wrapT = THREE.MirroredRepeatWrapping;
					}
				);
			}


			if (this.options.material == "basic")	{

				// Ka - there is no per material ambient in three js

				// if ((c = currMat.Ke) != "") //emmissive color
				// 	threeMat.emissive = new THREE.Color(c[0], c[1], c[2]);
				// if ((c = currMat.Ks) != "") //specular color
				// 	threeMat.specular = new THREE.Color(c[0], c[1], c[2]);
				// //Ni is optical density
				// if ((c = currMat.Ns) != "") //specular factor
				// 	threeMat.shininess = c;
				// Tr is transmittance
				// dissolve
				// illum

				// map_Ka = ambient reflectivity map


				// if (currMat.map_bump != ""){ //specular color map
				// 	//we need to check for the -bm <var> <filename> format first
				// 	var spl = currMat.map_bump.split(" ");
				// 	var bumpFileName;
				// 	if (spl[0] == "-bm"){
				// 		bumpFileName = spl[2];
				// 		threeMat.bumpScale = parseFloat(spl[1]);
				// 	} else {
				// 		bumpFileName = spl[0];
				// 	}

				// 	threeMat.bumpMap = THREE.ImageUtils.loadTexture(
				// 		this.filepath+bumpFileName,
				// 		THREE.UVMapping,
				// 		function(tex) {
				// 			tex.wrapS = THREE.MirroredRepeatWrapping;
				// 			tex.wrapT = THREE.MirroredRepeatWrapping;
				// 		}
				// 	);
				// }
			}



			this.materials.push(threeMat);
		}
	},
	splitPath: function(path) {
		var dirPart, filePart;
		path.replace(/^(.*\/)?([^/]*)$/, function(_, dir, file) {
			dirPart = dir; filePart = file;
		});
		if (typeof dirPart == 'undefined')
			dirPart = "";
		return { dirPart: dirPart, filePart: filePart };
	},
	decodeInterleavedInt: function(val) {
	    if (val%2==0)
	        return -1*(val/2)
	    else
	        return ((val-1)/2)+1
	},
	fromSNorm: function( value ) {
		return Math.clamp(value, 0.0, 255.0) / 255.0 * 2.0 - 1.0;
	},
	signNotZero: function( value ) {
		return value < 0.0 ? -1.0 : 1.0;
	},
	octDecode: function (x, y) {
		var theX = this.fromSNorm(x);
		var theY = this.fromSNorm(y);
		var theZ = 1.0 - (Math.abs(theX) + Math.abs(theY));

		var result = [theX, theY, theZ];

		if (result[2] < 0.0) {
			var oldVX = result[0];
			result[0] = (1.0 - Math.abs(result[1])) * this.signNotZero(oldVX);
			result[1] = (1.0 - Math.abs(oldVX)) * this.signNotZero(result[1]);
		}
		//normalize
		var len = Math.sqrt( result[0] * result[0] + result[1] * result[1] + result[2] * result[2]);
		result[0] /= len; result[1] /= len; result[2] /= len;
		return result;
	}

};

(function(){Math.clamp=function(a,b,c){return Math.max(b,Math.min(c,a));}})();


