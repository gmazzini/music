<?php
include "local.php";
echo "<pre>CIAOOOO\n";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id,name from music where top='root'");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $name=$row["name"];
  echo "--> $id $name\n";
}
echo "</pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
