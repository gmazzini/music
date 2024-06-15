<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id from song where duration=0");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  echo "$id\n";
  file_get_contents("https://music.mazzini.org/load.php?id=$id");
  sleep(10);
}
mysqli_free_result($query);
mysqli_close($con);
?>
