<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$access_token=file_get_contents("access_token");
@$id=$_GET["id"];
$ffname="cached/$id";
if(!file_exists($ffname)){
  $ch=curl_init();
  curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files/$id?alt=media");
  curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
  curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
  curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
  $oo=curl_exec($ch);
  curl_close($ch);
  file_put_contents($ffname,$oo);
}
$oo=json_decode(shell_exec("ffprobe -v quiet -print_format json -show_format $ffname"),true);
$ooo=$oo["format"];
$duration=(int)$ooo["duration"];
$format=$ooo["format_name"];
$title=mysqli_real_escape_string($con,preg_replace("/[ ]{0,}\([^)]+\)[ ]{0,}/","",$ooo["tags"]["title"]));
$album=mysqli_real_escape_string($con,preg_replace("/[ ]{0,}\([^)]+\)[ ]{0,}/","",$ooo["tags"]["album"]));
$artist=mysqli_real_escape_string($con,preg_replace("/[ ]{0,}\([^)]+\)[ ]{0,}/","",$ooo["tags"]["artist"]));
if(filesize($ffname)<1000000 || $duration<5 || $format!="mp3"){
  unlink($ffname);
  $ffname="Heartbeat.mp3";
}
else mysqli_query($con,"update song set played=played+1,duration=$duration,title='$title',album='$album',artist='$artist' where id='$id'");
$aux=file_get_contents($ffname);
header('Content-type: audio/mpeg;');
header("Content-Length: ".strlen($aux));
echo $aux;
mysqli_close($con);
?>
