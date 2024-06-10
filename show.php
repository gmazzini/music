<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);

// authentication
@$myid=$_POST["myid"];
if(strlen($myid)>7)$pwdmd5=md5($myid);
else $pwdmd5=$_GET["pwdmd5"];
$query=mysqli_query($con,"select first from login where pwdmd5='$pwdmd5'");
$row=mysqli_fetch_assoc($query);
$first=$row["first"];
mysqli_free_result($query);
if($pwdmd5=="" || $first==""){
  echo "<form method=post>";
  echo "passwd <input type=text name=myid size=16>";
  echo "<input type=submit name=act value=Enter>";
  echo "</form>";
  exit(0);
}

// playlist
$query=mysqli_query($con,"select label from playlist_desc where pwdmd5='$pwdmd5'");
for($ipl=0;;$ipl++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $pl[$ipl]=$row["label"];
}
mysqli_free_result($query);

@$liv=$_GET["liv"];
@$idin=$_GET["id"];
@$idorg=$_GET["idorg"];
if(!isset($liv)){$liv=1; $idin="root";}
if(!isset($idorg))$idorg="root";
if($liv<3){$nextliv=$liv+1; $db="music";}
else $db="song";
if($liv>1)$prevliv=$liv-1;
else $prevliv=$liv;
echo "<pre>$first LIV:$liv, ID:$idin <a href='show.php?liv=$prevliv&id=$idorg&pwdmd5=$pwdmd5'>Prev</a>\n";
$query=mysqli_query($con,"select id,name from $db where top='$idin' order by name");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $name=$row["name"];
  if($liv<3)echo "<a href='show.php?liv=$nextliv&id=$id&idorg=$idin&pwdmd5=$pwdmd5'>$name</a>\n";
  else {
    echo "$name";
    for($i=0;$i<$ipl;$i++)echo " <a href='show.php?liv=$nextliv&id=$id&idorg=$idin&pwdmd5=$pwdmd5'>$pl[$i]</a>";
    echo "\n";
  }
}
echo "</pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
