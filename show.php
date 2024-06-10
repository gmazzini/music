<?php
include "local.php";
$liv=$_GET["liv"];
$idin=$_GET["id"];
if(!isset($liv)){$liv=1; $idin="root";}
echo "<pre>LIV: $liv, ID: $idin\n";
if($liv<3){$nextliv=$liv+1; $db="music";}
else db="song";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id,name from $db where top='$idin' order by name");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $name=$row["name"];
  echo "<a href='show.php?liv=$nextliv&id=$id'>$name</a>\n";
}
echo "</pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
