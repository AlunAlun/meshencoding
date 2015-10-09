HEIGHT_PAN = false;
MOVE_CAMERA = false;
ROT_SPEED = 0.01;
ZOOM_SPEED = 0.1;

var MeshType = { UTF: 1, PNG: 2 }

var MESHAPP = {

	//camera variables
	nodes:[],
	mousePos:[0,0],
	cam_pos:[0, 1, 6],
	cam_target:[0,0,0],
	cam_angle:Math.PI,
	maxZoom: 1,

	//light
	light_pos:[-1000, 1000, 1000],

	init: function(filepath, filename, vertexShader, fragmentShader){
		
		//init stats
		this.initStats();

		//initGL
		var gl = GL.create({width:window.innerWidth,height:window.innerHeight});
		gl.getExtension("OES_element_index_uint");
		gl.animate();
		var container = document.querySelector("#content");
		container.appendChild(gl.canvas);

        //get metadata and then load mesh
        var that = this;
        this.peekMeshMetaData(filepath+filename, function(){
            that.loadMesh(filepath, filename, vertexShader, fragmentShader);
        });

		//create basic matrices for cameras and transformation
		var view = mat4.create();
		var model = mat4.create();
		var persp = mat4.perspective(mat4.create(), 45 * DEG2RAD, gl.canvas.width / gl.canvas.height, 0.1, 10000);


		//generic gl flags and settings
		gl.clearColor(0.0,0.0,0.0,1);
		gl.enable( gl.DEPTH_TEST );
		gl.enable( gl.CULL_FACE);
		gl.cullFace(gl.BACK);
		
		
		//rendering loop
		gl.ondraw = function()
		{
			gl.viewport(0,0,gl.canvas.width, gl.canvas.height);
			gl.clear(gl.DEPTH_BUFFER_BIT | gl.COLOR_BUFFER_BIT);

			//create view matrix
			mat4.lookAt(view, this.cam_pos, this.cam_target, [0,1,0]);

		
			for (i in this.nodes) {
				theMesh = this.nodes[i];
				if (theMesh.ready) {
					theMesh.draw(view, persp, this.light_pos);
				}
			}			
		}.bind(this);

		
		gl.onupdate = function(dt)
		{
			MESHAPP.stats.update();
		};

		////////////////////////////////////////////////////////////////
		// Get Mouse
		////////////////////////////////////////////////////////////////
		gl.captureMouse();
		gl.onmousemove = function(e)
		{
			if(e.dragging) {

				MESHAPP.updateCamera(e.deltax, e.deltay);
			}

			//update Mouse position for picking purposes
			MESHAPP.mousePos[0] = e.clientX-gl.canvas.leftTopScreen()[0];
			MESHAPP.mousePos[1] = e.clientY;
		};
		gl.onmousedown = function(e)
		{
			if (e.rightButton > 0)
				MOVE_CAMERA = true;
		}
		gl.onmouseup = function(e)
		{
			if (MOVE_CAMERA == true)
				MOVE_CAMERA = false;

		}
		gl.captureKeys(true);
		gl.onkeydown = function(e){
			if (e.keyCode == 37) { //left
				this.cam_target[0] -= 1.0;
			} else
			if (e.keyCode == 38) { //up
				this.cam_target[0] -= 1.0;
			} else
			if (e.keyCode == 39) { //right
				this.cam_target[0] = 1.0;
			} else
			if (e.keyCode == 40) { //down
				this.cam_target[0] -= 1.0;
			} 



		}.bind(this);

	},
	updateCamera: function(deltax, deltay) {

		if (MOVE_CAMERA) {
			var vecToCenter = vec3.subtract(vec3.create(), this.cam_target, this.cam_pos);

			vec3.normalize(vecToCenter, vecToCenter);
			vec3.scale(vecToCenter, vecToCenter, -ZOOM_SPEED*deltay);
			vec3.add(this.cam_pos, vecToCenter, this.cam_pos);


		} else {
			if (!this.nodes[0].rotateAngle)
				this.nodes[0].rotateAngle = 0;
			this.nodes[0].rotateAngle += deltax*ROT_SPEED;
			//move model
			var rotation = mat4.rotateY(this.nodes[0].model, mat4.create(), this.nodes[0].rotateAngle);

		}
		

	},
	initStats: function() {
		this.stats = new Stats();
		this.stats.setMode(0); // 0: fps, 1: ms
		this.stats.domElement.style.position = 'absolute';
		this.stats.domElement.style.left = '0px';
		this.stats.domElement.style.top = '0px';
		document.body.appendChild( this.stats.domElement );
	},
    loadMesh: function(filepath, filename, vertexShader, fragmentShader) {

        //callback to set camera position
        function setCamera(AABB) {
            MESHAPP.cam_target = [AABB.min[0]+(AABB.range[0]/2),AABB.min[1]+(AABB.range[1]/2),AABB.min[2]+(AABB.range[2]/2)];

            distToCenter = MESHAPP.cam_target[2]-AABB.range[2]*3;
            MESHAPP.cam_pos = [ Math.sin(MESHAPP.cam_angle)*distToCenter,
                MESHAPP.cam_target [1],
                Math.cos(MESHAPP.cam_angle)*distToCenter];
            MESHAPP.maxZoom = AABB.range[0]*2;
        }

        //create textures if need be
        var colorTexture = GL.Texture.fromURL("assets/checkers.jpg",{temp_color:[255,255,255,255], minFilter: gl.LINEAR_MIPMAP_LINEAR});

        //create nodes
        var theMesh;
        if (this.meshType == MeshType.UTF)
            theMesh = new UTFMesh(vertexShader,fragmentShader);
        else if (this.meshType == MeshType.PNG)
            theMesh = new PNGMesh(vertexShader, fragmentShader);
        theMesh.filepath = filepath;
        //custom loading message
        theMesh.onLoadProgress = function(evt) {
            document.getElementById("infoBox").innerHTML = parseInt((evt.loaded/10)/theMesh.meta.size) + "%";
        }
        //start load and pass callback to set camera position
        theMesh.startLoad(filename, setCamera);
        theMesh.colorTexture = colorTexture;
        //append to node list
        this.nodes.push(theMesh);

    },
    peekMeshMetaData: function(jsonFile, callback){
        var xhr = new XMLHttpRequest();
        xhr.open('GET', jsonFile);

        xhr.onload = function () {
            if (xhr.status >= 200 && xhr.status < 400) {
                var jsonData = JSON.parse(xhr.responseText);
                if (jsonData.data.endsWith("png"))
                    this.meshType = MeshType.PNG;
                else if (jsonData.data.endsWith("utf8"))
                    this.meshType = MeshType.UTF;

                if (callback) callback();
            }
        }.bind(this);
        xhr.send();
    }

}












	

function onMouseZoom(delta)
{

}

//extend element class to get top-left of canvas
Element.prototype.leftTopScreen = function () {
    var x = this.offsetLeft;
    var y = this.offsetTop;
    var element = this.offsetParent;

    while (element !== null) {
        x = parseInt (x) + parseInt (element.offsetLeft);
        y = parseInt (y) + parseInt (element.offsetTop);

        element = element.offsetParent;
    }

    return new Array (x, y);
};

if (!String.prototype.endsWith) {
    String.prototype.endsWith = function(suffix) {
        return this.indexOf(suffix, this.length - suffix.length) !== -1;
    };
}

