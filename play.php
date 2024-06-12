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
$query=mysqli_query($con,"select label,description from playlist_desc where pwdmd5='$pwdmd5' order by label");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $pl=$row["label"];
  $description=$row["description"];
  echo "<a href='play.php?pl=$pl&pwdmd5=$pwdmd5'>$pl $description</a>\n";
}
mysqli_free_result($query);
echo "<hr>";

// play
$query=mysqli_query($con,"select id,position from playlist where pwdmd5='$pwdmd5' and label='$plin' order by position");
for($i=0;;$i++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id[$i]=$row["id"];
  $query1=mysqli_query($con,"select name,parent from song where id='$id[$i]'");
  $row1=mysqli_fetch_assoc($query1);
  $data[$i]=$i." | ".$row1["name"];
  $parent=$row1["parent"];
  mysqli_free_result($query1);
  $query1=mysqli_query($con,"select name,parent from music where id='$parent'");
  $row1=mysqli_fetch_assoc($query1);
  $data[$i].=" | ".$row1["name"];
  $parent=$row1["parent"];
  mysqli_free_result($query1);
  $query1=mysqli_query($con,"select name from music where id='$parent'");
  $row1=mysqli_fetch_assoc($query1);
  $data[$i].=" | ".$row1["name"];
  mysqli_free_result($query1);
}
mysqli_free_result($query);

// player

echo "<span id='mydesc'></span>\n";
echo "<audio autoplay controls id='Player' src='load.php?id=$id[0]' onclick='this.paused ? this.play() : this.pause();'>Nooo</audio>\n";
echo "<script>\n";
echo "var src=["; for($j=0;$j<$i;$j++){if($j>0)echo ","; echo "'load.php?id=$id[$j]'";} echo "]\n";
echo "var desc=["; for($j=0;$j<$i;$j++){if($j>0)echo ",";echo "'$data[$j]'";} echo "]\n";
echo "var elm=0;\n";
echo "document.getElementById('mydesc').textContent=desc[elm];\n";
echo "var Player=document.getElementById('Player');\n";
echo "Player.onended=function(){\n";
echo "  elm++;\n";
echo "  if(elm < src.length){\n";
echo "    Player.src=src[elm];\n";
echo "    document.getElementById('mydesc').textContent=desc[elm];\n";
echo "    Player.play();\n";
echo "  }\n";
echo "}\n";
echo "function next(){\n";
echo "  elm++;\n";
echo "  if(elm < src.length){\n";
echo "    Player.src=src[elm];\n";
echo "    document.getElementById('mydesc').textContent=desc[elm];\n";
echo "    Player.play();\n";
echo "  }\n";
echo "  else elm=src.length-1;\n";
echo "}\n";
echo "function prev(){\n";
echo "  elm--;\n";
echo "  if(elm >= 0){\n";
echo "    Player.src=src[elm];\n";
echo "    document.getElementById('mydesc').textContent=desc[elm];\n";
echo "    Player.play();\n";
echo "  }\n";
echo "  else elm=0;\n";
echo "}\n";
echo "</script>\n";
echo "<button onclick='prev()'>prev</button><button onclick='next()'>next</button>\n";

echo "<pre>";
mysqli_close($con);
?>
