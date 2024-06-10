<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"]; @$liv=$_GET["liv"]; @$idin=$_GET["idin"];

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
$query=mysqli_query($con,"select label from playlist_desc where pwdmd5='$pwdmd5'");
for($ipl=0;;$ipl++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $pl[$ipl]=$row["label"];
}
mysqli_free_result($query);

// navigation
if($liv==""){$liv=1; $idin="root";}
if($liv<3){$nextliv=$liv+1; $db="music";}
else {$nextliv=3; $db="song";}
if($liv>1)$prevliv=$liv-1;
else $prevliv=$liv;

echo "<pre>$first liv:$liv, idin:$idin <a href='show.php?liv=$prevliv&idin=$idin&pwdmd5=$pwdmd5'>Prev</a>\n";
$query=mysqli_query($con,"select id,name from $db where parent='$idin' order by name");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $name=$row["name"];
  if($liv<3)echo "<a href='show.php?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5'>$name</a>\n";
  else {
    echo "$name";
    for($i=0;$i<$ipl;$i++){
      $apl=$pl[$i];
      echo " <a href='show.php?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&pl=$apl'>$apl</a>";
    }
    echo "\n";
  }
}
echo "</pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
