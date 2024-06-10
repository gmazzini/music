<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_GET["passwd"]; @$plin=$_GET["pl"]; @$idin=$_GET["id"]; @$act=$_GET["act"]; @$posin=$_GET["pos"];

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
echo "<hr>";

// acdtion on playlist
switch($act){
  case "C":
  mysqli_query($con,"delete from playlist where pwdmd5='$pwdmd5' and id='$idin' and position=$posin and label='$plin'");
echo "delete from playlist where pwdmd5='$pwdmd5' and id='$idin' and position=$posin and label='$plin'\n";
  
  break;
}

// single play list 
$query=mysqli_query($con,"select id,position from playlist where pwdmd5='$pwdmd5' and label='$plin' order by position");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $position=$row["position"];
  $query1=mysqli_query($con,"select name,parent from song where id='$id'");
  $row1=mysqli_fetch_assoc($query1);
  $name=$row1["name"];
  $parent=$row1["parent"];
  mysqli_free_result($query1);
  $query1=mysqli_query($con,"select name,parent from music where id='$parent'");
  $row1=mysqli_fetch_assoc($query1);
  $liv2=$row1["name"];
  $parent=$row1["parent"];
  mysqli_free_result($query1);
  $query1=mysqli_query($con,"select name,parent from music where id='$parent'");
  $row1=mysqli_fetch_assoc($query1);
  $liv1=$row1["name"];
  $parent=$row1["parent"];
  mysqli_free_result($query1);
  echo "<a href=list.php?act=C&id=$id&pl=$plin&pwdmd5=$pwdmd5&pos=$position>C</a>";
  echo " $position | $id | $name | $liv2 | $liv1\n";
}
echo "<pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
