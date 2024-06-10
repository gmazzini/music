<?php
include "local.php";
$access_token=file_get_contents("access_token");
$qq="%27$folderStart%27+in+parents";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$ch=curl_init();
curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files?q=$qq&pageSize=500");
curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
$oo=json_decode(curl_exec($ch),true);
curl_close($ch);
$files=$oo["files"];
foreach($files as $k => $v){
  $av1=mysqli_real_escape_string($con,$v["name"]);
  $av2=mysqli_real_escape_string($con,$v["id"]);
  $aux=sprintf("insert ignore into music (name,id,parent) value ('%s','%s','root')",$av1,$av2);
  echo "$aux\n";
  mysqli_query($con,$aux);
}
mysqli_close($con);
?>
