<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"];

// authentication
if(strlen($passwd)>6)$pwdmd5=md5($passwd);
else $pwdmd5=$_GET["pwdmd5"];
$query=mysqli_query($con,"select first from login where pwdmd5='$pwdmd5'");
$row=mysqli_fetch_assoc($query);
$first=$row["first"];
mysqli_free_result($query);
if($pwdmd5=="" || $first==""){
  echo "<form method=post>";
  echo "passwd <input type=text name=passwd size=16>";
  echo "<input type=submit name=act value=Enter>";
  echo "</form>";
  exit(0);
}

// playlist
echo "<pre>";
$query=mysqli_query($con,"select label from playlist_desc where pwdmd5='$pwdmd5'");
for($ipl=0;;$ipl++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $pl[$ipl]=$row["label"];
  echo "<a>$ps</a>\n";
}
mysqli_free_result($query);
echo "</pre>";

mysqli_close($con);
?>
