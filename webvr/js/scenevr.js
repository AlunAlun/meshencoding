/*
 * Debug parameters.
 */
WebVRConfig = {
    /**
     * webvr-polyfill configuration
     */

    // Forces availability of VR mode.
    FORCE_ENABLE_VR: true, // Default: false.
    // Complementary filter coefficient. 0 for accelerometer, 1 for gyro.
    //K_FILTER: 0.98, // Default: 0.98.
    // How far into the future to predict during fast motion.
    //PREDICTION_TIME_S: 0.040, // Default: 0.040 (in seconds).
    // Flag to disable touch panner. In case you have your own touch controls
    //TOUCH_PANNER_DISABLED: true, // Default: false.
    // Enable yaw panning only, disabling roll and pitch. This can be useful for
    // panoramas with nothing interesting above or below.
    //YAW_ONLY: true, // Default: false.

    /**
     * webvr-boilerplate configuration
     */
    // Forces distortion in VR mode.
    //FORCE_DISTORTION: true, // Default: false.
    // Override the distortion background color.
    // DISTORTION_BGCOLOR: {x: 1, y: 0, z: 0, w: 1}, // Default: (0,0,0,1).
    // Prevent distortion from happening.
    //PREVENT_DISTORTION: true, // Default: false.
    // Show eye centers for debugging.
    // SHOW_EYE_CENTERS: true, // Default: false.
    // Prevent the online DPDB from being fetched.
    // NO_DPDB_FETCH: true,  // Default: false.
};

var scene;
var camera;
var controls;
var manager;
var mainObject;
var hasSetCamera = false;

function createRenderer() {
    //Note: Antialiasing is a big performance hit.
    renderer = new THREE.WebGLRenderer({
        antialias: true
    });
    renderer.setPixelRatio(window.devicePixelRatio);

    // Apply VR stereo rendering to renderer.
    effect = new THREE.VREffect(renderer);
    effect.setSize(window.innerWidth, window.innerHeight);

    // Create a VR manager helper to enter and exit VR mode.
    var params = {
        hideButton: false, // Default: false.
        isUndistorted: false // Default: false.
    };
    manager = new WebVRManager(renderer, effect, params);



    document.body.appendChild(renderer.domElement);

}

function createCameraAndControls() {

    camera = new THREE.PerspectiveCamera(45, window.innerWidth / window.innerHeight, 0.1, 10000);

    // Apply VR headset positional data to camera.
    controls = new THREE.VRControls(camera);
}

function setCameraFromObject(obj) {
    var fov = camera.fov * (Math.PI / 180);
    var bbox = new THREE.Box3().setFromObject(obj);
    camera.position.set(0,
        bbox.min.y + ((bbox.max.y - bbox.min.y) / 2),
        obj.position.z + Math.abs((bbox.max.y - bbox.min.y) / Math.sin(fov / 2)));

    //obj.position.y -= (bbox.max.y-bbox.min.y)/3;
    camera.lookAt(new THREE.Vector3(bbox.min.x + ((bbox.max.x - bbox.min.x) / 2),
        bbox.min.y + ((bbox.max.y - bbox.min.y) / 2),
        bbox.min.z + ((bbox.max.z - bbox.min.z) / 2)));

}

function loadModel(file) {

    var onLoadFirstMesh = function(b128) {

        scene.remove(mainObject);
        mainObject = b128;
        scene.add(mainObject);

        if (hasSetCamera == false) {

            setCameraFromObject(mainObject);


            hasSetCamera = true;

        }

    }


    b128Loader = new THREE.B128Loader(); //{material:"basic"}
    b128Loader.load(file, onLoadFirstMesh);

}

function loadB128(file) {
    b128Loader = new THREE.B128Loader({
        material: "basic"
    });
    b128Loader.load(file, function(b128) {

        b128.rotation.z = 0.03;
        setCameraFromObject(b128);
        scene.add(b128);
    });
}

function createLights() {
    var directionalLight = new THREE.DirectionalLight(0xffffff, 0.7);
    directionalLight.position.set(1, 1, 0);
    scene.add(directionalLight);

    var directionalLight2 = new THREE.DirectionalLight(0xffffff, 0.3);
    directionalLight2.position.set(-1, 0, 1);
    scene.add(directionalLight2);

    var light = new THREE.AmbientLight(0x222222); // soft white light 404040
    scene.add(light);
}



function walk() {
    var forward = camera.getWorldDirection();
    forward.normalize();
    camera.position.x += forward.x * 0.05;
    camera.position.z += forward.z * 0.05;

}


// Request animation frame loop function
var lastRender = 0;

function animate(timestamp) {
    var delta = Math.min(timestamp - lastRender, 500);
    lastRender = timestamp;

    // Apply rotation to cube mesh
    //scene.getObjectByName( "cube" ).rotation.y += delta * 0.0006;

    if (walking)
        walk();

    // Update VR headset position and apply to camera.
    controls.update();

    // Render the scene through the manager.
    manager.render(scene, camera, timestamp);

    requestAnimationFrame(animate);
}

function init() {

    scene = new THREE.Scene();

    createRenderer();

    createCameraAndControls();

    createLights();

    loadModel('../assets/lee/lee-prog.json');

    animate(performance ? performance.now() : Date.now());

}







// Reset the position sensor when 'z' pressed.
function onKeyDown(event) {
    if (event.keyCode == 90) { // z
        controls.resetSensor();
    }
    if (event.keyCode == 87) {
        walking = true;
    }
}

function onKeyUp(event) {

    if (event.keyCode == 87) {
        walking = false;
    }
}
window.addEventListener('keydown', onKeyDown, true);
window.addEventListener('keyup', onKeyUp, true);

var lowthresh = -1.0;
var lookStepDown = false;
var walking = false;

var store = [];
var storeCounter = 6;
var z;

function motion(event) {
    z = event.acceleration.z;

    if (z < lowthresh)
        walking = true;

    store[storeCounter++] = z;
    if (Math.abs(store[storeCounter - 2]) < 0.3 &&
        Math.abs(store[storeCounter - 3]) < 0.3 &&
        Math.abs(store[storeCounter - 4]) < 0.3 &&
        Math.abs(store[storeCounter - 5]) < 0.3 &&
        Math.abs(store[storeCounter - 6]) < 0.3) {
        walking = false;
    }
}


/*if(window.DeviceMotionEvent){
    window.addEventListener("devicemotion", motion, false);
} else {
    console.log("DeviceMotionEvent is not supported");
}*/