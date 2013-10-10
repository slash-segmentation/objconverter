<!doctype html>
<html><head>
<?php

$show_main = TRUE;
$show_head = TRUE;
$del_dir = NULL;

function file_error($file, $allow_no_file = false)
{
  $err = $file['error'];
  switch ($err)
  {
    case UPLOAD_ERR_OK:         return FALSE;
    case UPLOAD_ERR_INI_SIZE:
    case UPLOAD_ERR_FORM_SIZE:  return "The uploaded file is too large ($err)";
    case UPLOAD_ERR_PARTIAL:    return 'The uploaded file was only partially uploaded - Please retry';
    case UPLOAD_ERR_NO_FILE:    return $allow_no_file ? FALSE : 'No file was uploaded';
    case UPLOAD_ERR_NO_TMP_DIR:
    case UPLOAD_ERR_CANT_WRITE:
    case UPLOAD_ERR_EXTENSION:
    default:
      return "Server problem ($err)";
  }
}
function del($path)
{
  if (is_dir($path))
  {
    foreach (scandir($path) as $o)
      if ($o != '.' && $o != '..')
        del($path.'/'.$o);
    return rmdir($path);
  }
  else
  {
    return unlink($path);
  }
}
function file_size($file)
{
  $bytes = filesize($file);
  if ($bytes >= 1000*1024*1024) return round($bytes / (1024*1024*1024.0), 2) . ' GB';
  else if ($bytes >= 1000*1024) return round($bytes / (1024*1024.0), 2) . ' MB';
  else if ($bytes >= 1000)      return round($bytes / (1024.0), 2) . ' KB';
  else                          return $bytes . ' bytes';
}

// Get models
try
{
  $db = new PDO('sqlite:/'.dirname(__FILE__).'/models.sq3');
}
catch (PDOException $e)
{
  echo '</head><body>DB Connection failed: ' . $e->getMessage() . '</body></html>';
  exit();
}
$models = array();
foreach ($db->query('SELECT name, file FROM models') as $row)
{
  $models[$row['name']] = $row['file'];
}
uksort($models, 'strcasecmp');

// Get params
$model_name = array_key_exists('model', $_REQUEST) ? $_REQUEST['model'] : FALSE;
$model_file = array_key_exists($model_name, $models) ? $models[$model_name] : FALSE;
$upload = array_key_exists('obj', $_FILES);

$errors = array();

if ($upload)
{
  $show_head = FALSE;
?>
<title>WebGL OBJ Previewer - Upload</title>
</head>
<body>
<h1>Upload</h1>
<?php
  if (strlen(rtrim($model_name)) < 1)                                                 { $errors[] = 'You must specify a model name'; goto end; }
  if ($model_file)                                                                    { $errors[] = "There is already a model with the name '$model_name'"; goto end; }

  $obj = $_FILES['obj'];
  $mtl = $_FILES['mtl'];
  if (($err = file_error($obj)) != FALSE || ($err = file_error($mtl, TRUE)) != FALSE) { $errors[] = $err; goto end; }

  $has_mtl = $mtl['error'] != UPLOAD_ERR_NO_FILE;
  $normalize = array_key_exists('normalize', $_POST) && ($_POST['normalize'] == '1');
  $objname = basename($obj['name']);
  $mtlname = basename($mtl['name']);
  $dot = strrpos($objname, '.');
  $name = ($dot > 0) ? substr($objname, 0, $dot) : $objname;
  $objname = "$name.obj";
  $model_name = preg_replace('/[^0-9a-zA-Z.-]+/', '_', trim($model_name));
  $dir = dirname(__FILE__).'/'.$model_name;
  $tmp = 'tmp'.rand().'.obj';
  $obj_cmd = substr(escapeshellarg($objname), 1, -1);
  $mtl_cmd = substr(escapeshellarg($mtlname), 1, -1);
  $tmp_cmd = substr(escapeshellarg($tmp), 1, -1);
  $name_cmd = substr(escapeshellarg($name), 1, -1);

  // Setup
  if (!mkdir($dir, 0775))                                                             { $errors[] = 'Server Issue: unable to create directory'; goto end; }
  if (!chdir($del_dir = $dir))                                                        { $errors[] = 'Server Issue: unable to create directory'; goto end; }
  if (!move_uploaded_file($obj['tmp_name'], $normalize ? $tmp : $objname))            { $errors[] = 'Server Issue: unable to retrieve uploaded file'; goto end; }
  if ($has_mtl && !move_uploaded_file($mtl['tmp_name'], $mtlname))                    { $errors[] = 'Server Issue: unable to retrieve uploaded file'; goto end; }

  // Normalization
  if ($normalize)
  {
    $cmd = "objnormalize '$tmp_cmd' '$obj_cmd' 2>&1";
    echo '<p>Normalizing... (<code>'.htmlentities($cmd).'</code>)</p><pre>';
    flush();
    $start = microtime(true);
    system($cmd, $retval);
    $end = microtime(true);
    echo '</pre><p>Finished in '.($end-$start).' seconds</p>';
    if ($retval != 0)                                                                 { $errors[] = "Failed to normalize the OBJ file ($retval)"; goto end; }
  }

  // Compression
  $w = $has_mtl?'':'-w ';
  $cmd = "objcompress $w'$obj_cmd' '$name_cmd.utf8' 3>&1 1>'$name_cmd.js' 2>&3";
  echo '<p>Compressing... (<code>'.htmlentities($cmd).'</code>)</p><pre>';
  flush();
  $start = microtime(true);
  system($cmd, $retval);
  $end = microtime(true);
  echo '</pre><p>Finished in '.($end-$start).' seconds</p>';
  if ($retval != 0)                                                                   { $errors[] = "Failed to compress the OBJ file ($retval)"; goto end; }

  // Remove temp file
  if ($normalize && (!unlink($objname) || !rename($tmp, $objname)))                   { $errors[] = 'Server Issue: unable to remove temp file'; goto end; }

  // Packaging
  echo '<p>Compressing and packaging... (just some tar/gzip commands to save things for faster and easier downloading)</p><pre>';
  flush();
  $start = microtime(true);
  system("gzip -cv9 '$name_cmd.js' 3>&1 1>'$name_cmd.js.gz' 2>&3");
  system('find . -type f -name \'*.utf8\' -exec sh -c \'gzip -cv9 "{}" > "{}.gz"\' \; 2>&1');
  system("tar -cvf '$name_cmd.utf8.tar' *.utf8 2>&1", $retval);
  system("gzip -v9 '$name_cmd.utf8.tar' 2>&1", $retval2);
  system("gzip -v9 '$obj_cmd' 2>&1", $retval3);
  if ($has_mtl) system("gzip -v9 '$mtl_cmd' 2>&1", $retval4);
  else          $retval4 = 0;
  $end = microtime(true);
  echo '</pre><p>Finished in '.($end-$start).' seconds</p>';
  if ($retval != 0 || $retval2 != 0 || $retval3 != 0 || $retval4 != 0)                { $errors[] = "Failed to package the files ($retval/$retval2/$retval3/$retval4)"; goto end; }

  // Add to Database
  $st = $db->prepare('INSERT INTO models (name, file) VALUES (?,?)'); // TODO: check error
  $st->execute(array($model_name, $name));

  $del_dir = NULL;
  echo '<p>Successfully Added Model!</p>';
  echo "<p><a href=\"?model=$model_name\">View it now</a> (next time it will be available below)</p>";
  echo "<a href='$model_name/$objname.gz'>OBJ (".file_size("$objname.gz").')</a> ';
  if ($has_mtl)
    echo "<a href='$model_name/$mtlname.gz'>MTL (".file_size("$mtlname.gz").')</a>';
  echo '<br>';
  $n = count(glob("*.$name.utf8"));
  $text = ($n==1) ? 'UTF8' : "$n UTF8s";
  echo "<a href='$model_name/$name.utf8.tar.gz'>$text (".file_size("$name.utf8.tar.gz").")</a> ";
  echo "<a href='$model_name/$name.js'>JS (".file_size("$name.js").')</a>';
}
else if ($model_file)
{
  $show_main = FALSE;
?>
<link rel="stylesheet" type="text/css" href="samples.css">
<title>WebGL OBJ Previewer - <?php echo $model_name; ?></title>
</head>
<body>

<!-- Basic Setup: canvas object and background -->
<canvas id="canvas" class="full"></canvas>
<span class="backdrop full" style="z-Index:-2;"></span>

<!-- Some WebGL stuff I don't understand yet -->
<script id="SIMPLE_VERTEX_SHADER" type="text/x-vertex">
#ifdef GL_ES
precision highp float;
#endif

uniform mat4 u_mvp;
uniform mat3 u_model;

attribute vec3 a_position;
attribute vec2 a_texcoord;
attribute vec3 a_normal;

varying vec2 v_texcoord;
varying vec3 v_normal;

void main(void) {
   v_texcoord = a_texcoord;
   v_normal = u_model * a_normal;
   gl_Position = u_mvp * vec4(a_position, 1.0);
}
</script>

<script id="SIMPLE_FRAGMENT_SHADER" type="text/x-fragment">
#ifdef GL_ES
precision mediump float;
#endif

uniform sampler2D u_diffuse_sampler;

varying vec2 v_texcoord;
varying vec3 v_normal;

void main(void) {
    // Simple, soft directional lighting.
    vec4 fetch = texture2D(u_diffuse_sampler, v_texcoord);
    vec3 normal = normalize(v_normal);
    vec3 light_vec = normalize(vec3(-0.25, -0.25, 1.0));
    float light = 0.7 + 0.6*dot(normal, light_vec);
    gl_FragData[0] = vec4(light*light*fetch.rgb, fetch.a);
}
</script>

<!-- Load Libraries -->
<script type="text/javascript" src="gl-matrix-min.js"></script><!-- Thrid party matrix library -->
<script type="text/javascript" src="base.js"></script><!-- Basic JS/DOM/XHR stuff, nothing with WebGL -->
<script type="text/javascript" src="webgl.js"></script><!-- WebGL base, could also use webgl-debug.js. Includes defaults, Shader, Program, Texture, and Mesh -->
<script type="text/javascript" src="loader.js"></script><!-- WebGL-loader library for downloading and decoding UTF8 files -->
<script type="text/javascript" src="renderer.js"></script><!-- Render class -->
<script type="text/javascript" src="samples.js"></script><!-- Basic sample usage of WebGL stuff to display the models -->

<!-- Load Model Information -->
<script type="text/javascript" src="<?php echo "$model_name/$model_file.js" ?>"></script>

<!-- Do the actual loading -->
<script type="text/javascript">

var path = '<?php echo $model_name ?>/';

// For good measure
function computeNormals(attribArray, indexArray, bboxen, meshEntry, opt_normalize) {
  var positionParams = NO_TEXCOORD_VERTEX_FORMAT[0];
  var normalParams = NO_TEXCOORD_VERTEX_FORMAT[1];
  var positionOffset = positionParams.offset;
  var normalOffset = normalParams.offset;
  var stride = normalParams.stride;

  var numIndices = indexArray.length;
  var p0 = vec3.create();
  var p1 = vec3.create();
  var p2 = vec3.create();
  for (var i = 0; i < numIndices; i += 3) {
    var v0 = indexArray[i];
    var v1 = indexArray[i + 1];
    var v2 = indexArray[i + 2];
    for (var j = 0; j < 3; ++j) {
      p0[j] = attribArray[stride * v0 + positionOffset + j];
      p1[j] = attribArray[stride * v1 + positionOffset + j];
      p2[j] = attribArray[stride * v2 + positionOffset + j];
    }
    vec3.subtract(p1, p0);
    vec3.subtract(p2, p0);
    vec3.cross(p1, p2, p0);
    for (var j = 0; j < 3; ++j) {
      attribArray[stride * v0 + normalOffset + j] += p0[j];
      attribArray[stride * v1 + normalOffset + j] += p0[j];
      attribArray[stride * v2 + normalOffset + j] += p0[j];
    }
  }

  if (opt_normalize) {
    var numAttribs = attribArray.length;
    for (var i = normalOffset; i < numAttribs; i += stride) {
      var x = attribArray[i];
      var y = attribArray[i + 1];
      var z = attribArray[i + 2];
      var n = 1.0/Math.sqrt(x*x + y*y + z*z);
      attribArray[i] *= n;
      attribArray[i + 1] *= n;
      attribArray[i + 2] *= n;
    }
  }

  onLoad(attribArray, indexArray, bboxen, meshEntry);
}

function extend(o1, o2) { for (var k in o2) { if (o2.hasOwnProperty(k)) { o1[k] = o2[k]; } } }

MATERIALS = {};
for (var name in MODELS)
{
  if (MODELS.hasOwnProperty(name))
  {
    extend(MATERIALS, MODELS[name].materials);
  }
}

for (var name in MODELS)
{
  if (MODELS.hasOwnProperty(name))
  {
//   given:    default:
//0: 0.205237  0.00012208521548040532  // Position. Use a uniform scale.
//1: 0.205237  0.00012208521548040532
//2: 0.205237  0.00012208521548040532
//3: 0.000978  0.0009775171065493646   // Texture Coords.
//4: 0.000978  0.0009775171065493646
//5: 0.001957  0.0009775171065493646   // Normal. Always uniform range.
//6: 0.001957  0.0009775171065493646
//7: 0.001957  0.0009775171065493646
    // WHY!?
    //MODELS[name].decodeParams.decodeScales = DEFAULT_DECODE_PARAMS.decodeScales;
    while (MODELS[name].decodeParams.decodeScales[0] >= 0.001)
    {
      MODELS[name].decodeParams.decodeScales[0] /= 10;
      MODELS[name].decodeParams.decodeScales[1] /= 10;
      MODELS[name].decodeParams.decodeScales[2] /= 10;
    }
    downloadModel(path, name, computeNormals); //onLoad);
  }
}

</script>

<?php
}
else if ($model_name) {echo $errors[] = "There is no model with the name '$model_name'"; }

end:
chdir(dirname(__FILE__));
if ($del_dir)
  del($del_dir);
if ($show_main)
{
  if ($show_head)
  {
    echo '<title>WebGL OBJ Previewer</title></head><body>';
  }
?>
<style type="text/css">
.error
{
  font-weight: bold;
  color: red;
}
</style>
<?php
foreach ($errors as $err)
{
  echo '<p class="error">'.htmlspecialchars($err).'</p>';
}

?>
<h1>Models</h1>
<p>Below are models that have been processed with this server.<br>
Clicking the the model name will open a <i>very</i> basic WebGL previewer. One issue with it at the moment is that it does not show the back of surfaces.<br>
Other columns are links to downloads for the original OBJ/MTL files along with the normalized/converted/compressed UTF8/JS files.<br>
All files are gzipped, and the gzipped files are sent to the server is the browser supports it. The UTF8 files are tarred to make it easier to download.<br>
The <a href="http://en.wikipedia.org/wiki/Data_compression_ratio">compression ratio</a> (CR) is the ratio of gzipped UTF8/JS size to gzipped OBJ/MTL size.</p>
<table>
<thead><tr><th>Model Name</th><th>File Names</th><th>OBJ</th><th>MTL</th><th>UTF8</th><th>JS</th><th>CR</th></tr></thead>
<tbody>
<?php
foreach ($models as $name=>$file)
{
  $mtls = glob("$name/*.mtl.gz");
  $utf8s = glob("$name/*.$file.utf8.gz");

  echo "<tr><td><b><a href='?model=$name'>$name</a></b></td><td>".htmlspecialchars($file).'</td>';
  $filename = htmlspecialchars("$name/$file");
  echo "<td align=right><a href='$filename.obj.gz'>".file_size("$filename.obj.gz").'</a></td>';
  echo '<td align=right>';
  if ($mtls) echo "<a href='$mtls[0]'>".file_size("$mtls[0]").'</a>';
  echo '</td>';
  $n = count($utf8s);
  $text = ($n==1) ? '' : "$n - ";
  echo "<td align=right>$text<a href='$filename.utf8.tar.gz'>".file_size("$filename.utf8.tar.gz").'</a></td>';
  echo "<td align=right><a href='$filename.js.gz'>".file_size("$filename.js.gz").'</a></td>';

  $orig = filesize("$filename.obj.gz") + ($mtls ? filesize("$mtls[0]") : 0);
  // filesize("$filename.utf8.tar.gz") is fairly accruate, but we might as well be really accurate since it isn't difficult
  $new = filesize("$filename.js.gz");
  foreach ($utf8s as $utf8)
    $new += filesize($utf8);
  echo '<td align=right>'.round($new/$orig*100, 1).'%</td>';

  echo '</tr>';
}
?>
</tbody>
</table>
<p><small>In a final system the JS files would be minified (10-25% smaller) and combined (better compression). Minification is just removing all whitespace not in strings (JSON has no necessary whitespace).</small></p>

<form action="." method="post" enctype="multipart/form-data">
<h1>Add New Model</h1>
<p>Max file size is 500 MB</p>
<p><b>Warning</b>: Since this software is constantly changing models may be cleaned out periodically!</p>
<input type="hidden" name="MAX_FILE_SIZE" value="524288000">
<label for="model">Model Name: <input name="model" id="model"></label> (spaces and some other characters changed to underscores)<br>
<label for="obj">OBJ File: <input type="file" name="obj" id="obj"></label><br>
<label for="mtl">MTL File: <input type="file" name="mtl" id="mtl"></label> (optional - will assume solid white)<br>
<label for="normalize">Normalize? <input type="checkbox" name="normalize" id="normalize" value="1" checked></label> (causes object to be moved to be centered around origin)<br>
<input type="submit" value="Upload and Convert">
</form>
<?php
}
?>
</body></html>