<!DOCTYPE html>
<html>
<head>

    <!-- BOOTSTRAP -->
	 <!-- Latest compiled and minified CSS -->
	<link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap.min.css" integrity="sha384-1q8mTJOASx8j1Au+a5WDVnPi2lkFfwwEAa8hDDdjZlpLegxhjVME1fgjWPGmkzs7" crossorigin="anonymous">
	<link rel="stylesheet" href="https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap-theme.min.css" integrity="sha384-fLW2N01lMqjakBkx3l/M9EahuwpSfeNvV63J5ezn3uZzapT0u7EYsXMjQV+0En5r" crossorigin="anonymous">
</head>
<body style="margin: 10px; padding:10px">
<div style="width:650px; margin:0 auto; text-align:center">
<h1>OBJ2B128 Convertor Results</h1>

<?php

ini_set('display_startup_errors',1);
ini_set('display_errors',1);
error_reporting(-1);

$verbose = false;


function generate_random_letters($length) {
  $random = '';
  for ($i = 0; $i < $length; $i++) {
    $random .= chr(rand(ord('a'), ord('z')));
  }
  return $random;
}

/* creates a compressed zip file */
function create_zip($files = array(),$destination = '',$overwrite = false) {
	//if the zip file already exists and overwrite is false, return false
	if(file_exists($destination) && !$overwrite) { return false; }
	//vars
	$valid_files = array();
	//if files were passed in...
	if(is_array($files)) {
		//cycle through each file
		foreach($files as $file) {
			//make sure the file exists
			if(file_exists($file)) {
				$valid_files[] = $file;
			}
		}
	}
	//if we have good files...
	if(count($valid_files)) {
		//create the archive
		$zip = new ZipArchive();
		if($zip->open($destination,$overwrite ? ZIPARCHIVE::OVERWRITE : ZIPARCHIVE::CREATE) !== true) {
			return false;
		}
		//add the files
		foreach($valid_files as $file) {
			$zip->addFile($file,$file);
		}
		//debug
		//echo 'The zip archive contains ',$zip->numFiles,' files with a status of ',$zip->status;
		
		//close the zip -- done!
		$zip->close();
		
		//check to make sure the file exists
		return file_exists($destination);
	}
	else
	{
		return false;
	}
}

$target_dir = "uploads/";
$target_file = $target_dir . basename($_FILES["fileToUpload"]["name"]);
$uploadOk = 1;
$needUnzip = 0;
$needMoveOBJ = 0;
$processOK = 0;
$imageFileType = pathinfo($target_file,PATHINFO_EXTENSION);

// Allow certain file formats
if($imageFileType != "zip" && $imageFileType != "obj" ) {
	echo "<p>$imageFileType</p>";
    echo "<p>Only zip or obj files are allowed.</p>";
    $uploadOk = 0;
}

// Check if $uploadOk is set to 0 by an error
if ($uploadOk == 0) {
    echo "<p>Your file was not uploaded.</p>";
// if everything is ok, try to upload file
} else {
    if (move_uploaded_file($_FILES["fileToUpload"]["tmp_name"], $target_file)) {
    	if ($verbose)
        	echo "<p>The file ". basename( $_FILES["fileToUpload"]["name"]). " has been uploaded.</p>";

        if ($imageFileType == "zip")
        	$needUnzip = 1;
        else
        	$needMoveOBJ = 1;
    } else {
        echo "<p>There was an error uploading your file.</p>";
    }
}

$theOBJFile = "";
$uid = "";
$meshName = "";
$fullpath = "";

if ($needUnzip == 1) {

	chmod($target_file, 0777);
	// get the absolute path to the uploaded file
	//$path = pathinfo(realpath($target_file), PATHINFO_DIRNAME);
	//make unique id
	$uid = generate_random_letters(5);
	//make a directory with this id
	mkdir($uid);
	//get full path to new directory
	$fullpath = dirname(__FILE__)."/".$uid;
	//give full permissions (probably dangerously)
	chmod($fullpath, 0777);

	$zip = new ZipArchive;
	$res = $zip->open($target_file);
	if ($res === TRUE) {
		// extract it to the path we determined above
		$zip->extractTo($fullpath);
		$zip->close();
		if ($verbose)
			echo "<p>$target_file extracted to $fullpath</p>";

		$objSearchPath = $fullpath."/*.obj";

		$objFiles = glob($objSearchPath);
		$theOBJFile = $objFiles[0];

		//TODO error no object found in zip file

		$meshName = basename($objFiles[0], ".obj");

		if ($verbose)
			echo "<p>Found obj file with name: $meshName</p>";
		$processOK = 1;


	} else {
	  echo "<p>Doh! I couldn't open $target_file</p>";
	}
} elseif ($needMoveOBJ == 1)
{
	//create uid as above
	chmod($target_file, 0777);
	$uid = generate_random_letters(5);
	mkdir($uid);
	$fullpath = dirname(__FILE__)."/".$uid;
	chmod($fullpath, 0777);

	$theOBJFile = $fullpath."/".basename($target_file);
	$meshName = basename($theOBJFile, ".obj");
	//move obj to uid directory
    if ($verbose)
		echo "moving obj file to $theOBJFile";
	rename($target_file, $theOBJFile );
	$processOK = 1;
}

if ($processOK == 1) {
	$processCmd = "./webmesh $theOBJFile $fullpath/$meshName";
	
	if ($verbose) echo "<p> $processCmd </p>";
	
	exec($processCmd);
	
	if ($verbose) echo "<p>done</p>";

	$indexFile = $uid.'/index.html';
	$htmlToWrite = '
		<!DOCTYPE html>
		<html>
		<head>
		    <script src="../js/three.min.js"></script>
		    <script src="../js/OBJLoader.js"></script>
		    <script src="../js/B128Loader.js"></script>
		    <script src="../js/OrbitControls.js"></script>
		    <link rel="stylesheet" href="../css/style.css">
		</head>
		<body>
		<div id="toolbar">
		        <img id="rotateX" src="../img/icon-rotate.png">
		</div>
		<div id="container"></div>
		<script src="../js/scene.js"></script>
		<script>
		init("'.$meshName.'.json");
		</script>
		</body>
		</html>
		';
	file_put_contents($indexFile, $htmlToWrite);

	$shortPathJson = $uid."/".$meshName.".json";
	$shortPathB128 = $uid."/".$meshName.".b128";

	//create zip file
	$files_to_zip = array(
		$shortPathJson,
		$shortPathB128
	);
	//if true, good; if false, zip creation failed
	$zipFileName = $uid."/".$meshName.".zip";
	$result = create_zip($files_to_zip, $zipFileName);

	echo "<p><a href='$zipFileName'>Download zip file with b128 and json</a></p>";

	//echo "<iframe style='overflow:hidden; border:none;' width='400' height=400' src='$uid'></iframe>";

	echo "<p>Permalink to scene <a href='http://alunevans.info/obj2b128/$uid'>http://alunevans.info/obj2b128/$uid</a></p>";

}


//TODO delete zip file

?>
</div>
</body>
</html>

