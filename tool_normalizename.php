<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id,title,album,artist from login where pwdmd5='$pwdmd5'");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $title=$row["title"];
  $album=$row["album"];
  $artist=$row["artist"];
  $pp=strpos($album,$artist.": ",);
  if($pp!==false)echo "$album\n";
}
mysqli_free_result($query);
mysqli_close($con);
?>
