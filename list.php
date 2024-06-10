<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"]; @$plin=$_POST["pl"];

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

// list of playlist
echo "<pre>$first\n";
$query=mysqli_query($con,"select label from playlist_desc where pwdmd5='$pwdmd5'");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $pl=$row["label"];
  echo "<a href='list.php?pl=$pl&pwdmd5=$pwdmd5'>$pl</a>\n";
}
mysqli_free_result($query);
echo "<br>XXXXX\n";


echo "select id,position from playlist where pwdmd5='$pwdmd5' and label='$plin'\n";
// simngle play list 
$query=mysqli_query($con,"select id,position from playlist where pwdmd5='$pwdmd5' and label='$plin'");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $position=$row["position"];
  echo "$position $id\n";
}
echo "<pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
