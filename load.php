<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$access_token=file_get_contents("access_token");
@$id=$_GET["id"];
$ffname="cached/$id";
mysqli_query($con,"update song set played=played+1 where id='$id'");
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
if(filesize($ffname)<1000000){
  unlink($ffname);
  $ffname="Heartbeat.mp3";
}
$aux=file_get_contents($ffname);
header('Content-type: audio/mpeg;');
header("Content-Length: ".strlen($aux));
echo $aux;
mysqli_close($con);
?>
