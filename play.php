<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"]; @$plin=$_GET["pl"];

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
  echo "<a href='play.php?pl=$pl&pwdmd5=$pwdmd5'>$pl</a>\n";
}
mysqli_free_result($query);
echo "<hr>";

// play
$query=mysqli_query($con,"select id,position from playlist where pwdmd5='$pwdmd5' and label='$plin' order by position");
for($i=0;;$i++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id[$i]=$row["id"];
}
mysqli_free_result($query);

echo "<span id=\"mydesc\"></span>";
echo "<audio autoplay controls id=\"Player\" src=\"cached/$id[0]\" onclick=\"this.paused ? this.play() : this.pause();\">Nooo</video>";
echo "<script>";
echo "document.getElementById(\"mydesc\").textContent=\"$id[0]\";";
echo "var nextsrc=[";
for($j=1;$j<$i;$j++){
  if($i>1)echo ",";
  echo "\"cached/$id[$j]\"";
}
echo "];";
echo "var elm=0; var Player=document.getElementById('Player'); Player.onended=function(){if(++elm < nextsrc.length+1){Player.src=nextsrc[elm-1];Player.play();}}</script>";

echo "<pre>";
mysqli_close($con);
?>
