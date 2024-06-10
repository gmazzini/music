<?php
include "local.php";
$access_token=file_get_contents("access_token");
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id from music where top='root'");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  mysqli_query($con,"update music set done=1 where id='$id'");
  $qq="%27$id%27+in+parents";
  $ch=curl_init();
  curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files?q=$qq&pageSize=500");
  curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
  curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
  curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
  $oo=json_decode(curl_exec($ch),true);
  curl_close($ch);
  $files=$oo["files"];
  $av3=mysqli_real_escape_string($con,$id);
  foreach($files as $k => $v){
    $av1=mysqli_real_escape_string($con,$v["name"]);
    $av2=mysqli_real_escape_string($con,$v["id"]);
    $aux=sprintf("insert ignore into music (name,id,parent) value ('%s','%s','%s')",$av1,$av2,$av3);
    echo "$aux\n";
    mysqli_query($con,$aux);
  }
}
mysqli_free_result($query);
mysqli_close($con);
?>
