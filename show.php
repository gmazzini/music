<?php
include "local.php";

$myid=$_POST["myid"];
if(strlen($myid)>7)$pwdmd5=md5($myid);
else $pwdmd5=$_GET["pwdmd5"];
if($pwdmd5==""){
  echo "<form method=post>";
  echo "passwd <input type=text name=myid size=16>";
  echo "<input type=submit name=act value=idd>";
  echo "</form>";
  exit(0);
}
$query=mysqli_query($con,"select first from user where pwdmd5='$pwdmd5'");
$first=$row["first"];
mysqli_free_result($query);

$liv=$_GET["liv"];
$idin=$_GET["id"];
$idorg=$_GET["idorg"];
if(!isset($liv)){$liv=1; $idin="root";}
if(!isset($idorg))$idorg="root";
if($liv<3){$nextliv=$liv+1; $db="music";}
else $db="song";
if($liv>1)$prevliv=$liv-1;
else $prevliv=$liv;
echo "<pre>$first LIV:$liv, ID:$idin <a href='show.php?liv=$prevliv&id=$idorg'>Prev</a>\n";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id,name from $db where top='$idin' order by name");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $name=$row["name"];
  echo "<a href='show.php?liv=$nextliv&id=$id&idorg=$idin'>$name</a>\n";
}
echo "</pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
