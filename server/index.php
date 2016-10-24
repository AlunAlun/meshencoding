<!DOCTYPE html>
<html>
<body style="margin: 10px; padding:10px">
<div style="width:650px; margin:0 auto; text-align:center">
<h1>OBJ2B128 Convertor</h1>
<p>You can upload either:</p>
<ul style="text-align:left">
<li>A zip file containing a Wavefront OBJ file, MTL file, and textures <br>(textures these should be <span style="font-weight:bold">square and power-of-two</span> i.e. 512x512, 2048x2048)</li>
<li>A wavefront OBJ file on its own</li>
</ul><br>
<p>After uploading, the mesh will be converted to the B128 webmesh format, and a sample rendering
will be displayed using <a href="http://threejs.org/">THREE.js</a>. If you uploaded a MTL file, 
the material in the THREE.js scene will be as specified in the file. Otherwise, it is renderered 
with a default material.</p>
<p>With big files, the conversion process may take several seconds, there is no feedback, please be patient!</p>
<br>
<form action="upload.php" method="post" enctype="multipart/form-data">
    Select file to upload:
    <input type="file" name="fileToUpload" id="fileToUpload">
    <input type="submit" value="Upload File" name="submit">
</form>

<div style="text-align:left; margin-top:50px">
<h2>Previous scenes</h2>
<?php
$directories = glob('*' , GLOB_ONLYDIR);
foreach ($directories as $dir) {
	if ($dir != css && $dir != img && $dir != js && $dir != uploads){
    	$json = glob($dir."/*.json");
    	$jsonFileName = substr($json[0], strpos($json[0], "/")+1);
    	echo "<p><a href='http://alunevans.info/obj2b128/$dir'>$jsonFileName</a></p>";
	}
}
?>
</div>
</div>
</body>
</html>