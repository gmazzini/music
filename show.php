<?php
include "local.php";
$liv=$_GET["liv"];
if(!isset($liv))$liv=1;
echo "<pre>LIV: $liv, ID: $id\n";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id,name from music where top='root' order by name");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $name=$row["name"];
  echo "<a href='show.php?liv=2&id=$id'>$name</a>\n";
}
echo "</pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
