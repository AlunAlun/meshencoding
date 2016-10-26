//global variables
var renderer;
var rendererContainer;
var scene;
var camera;
var cameraControl;
var mainObject;
var hasSetCamera = false;


function createRenderer() {
    renderer = new THREE.WebGLRenderer();
    renderer.setClearColor(0x000000, 1.0);
    renderer.setSize(rendererContainer.offsetWidth, rendererContainer.offsetHeight);
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
}

function createCamera() {
    camera = new THREE.PerspectiveCamera(
        45,
        rendererContainer.offsetWidth / rendererContainer.offsetHeight,
        0.1, 10000);


    camera.position.z = 2;


    //camera.lookAt(scene.position);
    cameraControl = new THREE.OrbitControls(camera);
}

function createLights() {
    var directionalLight = new THREE.DirectionalLight(0xffffff, 0.7);
    directionalLight.position.set(1, 1, 0);
    scene.add(directionalLight);

    // var directionalLight = new THREE.DirectionalLight( 0xffffff, 0.8 );
    // directionalLight.position.set( -0.25, -0.25, 1 );
    // scene.add( directionalLight );

    var directionalLight2 = new THREE.DirectionalLight(0xffffff, 0.3);
    directionalLight2.position.set(-1, 0, 1);
    scene.add(directionalLight2);

    var light = new THREE.AmbientLight(0x222222); // soft white light 404040
    scene.add(light);
}

function setCameraFromObject(obj) {
    var fov = camera.fov * (Math.PI / 180);
    var bbox = new THREE.Box3().setFromObject(obj);
    camera.position.set(0,
        bbox.min.y + ((bbox.max.y - bbox.min.y) / 2),
        obj.position.z + Math.abs((bbox.max.y - bbox.min.y) / Math.sin(fov / 2)));

    //obj.position.y -= (bbox.max.y-bbox.min.y)/3;
    cameraControl.target = new THREE.Vector3(bbox.min.x + ((bbox.max.x - bbox.min.x) / 2),
        bbox.min.y + ((bbox.max.y - bbox.min.y) / 2),
        bbox.min.z + ((bbox.max.z - bbox.min.z) / 2));

}

function loadModel(file) {

    var onLoadFirstMesh = function(b128) {

        scene.remove(mainObject);
        mainObject = b128;
        scene.add(mainObject);

        //mainObject.children[0].material = new THREE.MeshLambertMaterial({color:0xffffff});

        if (hasSetCamera == false) {

            setCameraFromObject(mainObject);


            hasSetCamera = true;

        }

    }


    b128Loader = new THREE.B128Loader(); //{material:"basic"}
    b128Loader.load(file, onLoadFirstMesh);

}

//init() gets executed once
function init(file) {

    rendererContainer = document.getElementById("container");

    scene = new THREE.Scene();

    clock = new THREE.Clock();

    createRenderer();
    createCamera();
    createLights();

    loadModel(file);
    rendererContainer.appendChild(renderer.domElement);

    render();
}

//infinite loop
function render() {
    cameraControl.update();
    renderer.render(scene, camera);
    requestAnimationFrame(render);
}

/*
 * Simple function for extracting the url query parameters
 */
function getQueryParams(qs) {
    qs = qs.split("+").join(" ");
    var params = {},
        tokens,
        re = /[?&]?([^=]+)=([^&]*)/g;
    while (tokens = re.exec(qs)) {
        params[decodeURIComponent(tokens[1])] = decodeURIComponent(tokens[2]);
    }
    return params;
}

var query = getQueryParams(document.location.search);

if (query.file) {
    init(query.file);
}

/*
 *
 */
var GUI = {
    buttons: {
        rotateX: document.getElementById("rotateX")
    },
    init: function() {
        if (this.buttons.rotateX) {
            this.buttons.rotateX.addEventListener("click", function() {
                mainObject.rotateX(Math.PI / 2);
            })
        }
    }
}
GUI.init();