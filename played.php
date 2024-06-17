<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id from song where duration=0 and nomp3=0");
$id=$_GET["id"];
mysqli_query($con,"update song set played=played+1 where id='$id'");
mysqli_close($con);
?>
