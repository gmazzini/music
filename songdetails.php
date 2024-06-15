<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id,name,parent from song where title=''");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $name=$row["name"];
  $parent=$row["parent"];
  $query1=mysqli_query($con,"select name,parent from music where id='$parent'");
  $row1=mysqli_fetch_assoc($query1);
  $liv2=$row1["name"];
  $parent=$row1["parent"];
  mysqli_free_result($query1);
  $query1=mysqli_query($con,"select name from music where id='$parent'");
  $row1=mysqli_fetch_assoc($query1);
  $liv1=$row1["name"];
  mysqli_free_result($query1);
  mysqli_query($con,"iupdate song set title='$name',album='$liv2',artist='$liv1' where id='$id'");
}
mysqli_free_result($query);
mysqli_close($con);
?>
