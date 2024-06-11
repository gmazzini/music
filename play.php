<?php
include "local.php";
$access_token=file_get_contents("access_token");
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

// caching and play
$query=mysqli_query($con,"select id,position from playlist where pwdmd5='$pwdmd5' and label='$plin' order by position");
for($i=0;;$i++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  if(!file_exists("cached/$id")){
    echo "Cache: $id\n";
    $ch=curl_init();
    curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files/$id?alt=media");
    curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
    curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
    curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
    $oo=curl_exec($ch);
    curl_close($ch);
    file_put_contents("cached/$id",$oo);
  }
  if($i==0){
    echo "<audio autoplay controls id=\"Player\" src=\"cached/$id\" onclick=\"this.paused ? this.play() : this.pause();\">Nooo</video>";
    echo "<script>var nextsrc = [\"cached/$id\"";
  }
  else echo ",\"cached/$id\"";
  
}
echo "]; var elm=0; var Player=document.getElementById('Player'); Player.onended=function(){if(++elm<nextsrc.length+1){Player.src=nextsrc[elm-1];Player.play();}}</script>";

echo "<pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
