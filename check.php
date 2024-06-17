<?php
include "local.php";

$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id from song where nomp3=0");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  for($i=0;$i<4;$i++){
    $access_token=file_get_contents("access_token");
    $ch=curl_init();
    curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files/$id");
    curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
    curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
    curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
    $oo=json_decode(curl_exec($ch),true);
    curl_close($ch);
    $name=$oo["name"];
    echo "$i | $id | $name\n";
    $ch=curl_init();
    curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files/$id?fields=parents");
    curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
    curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
    curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
    $oo=json_decode(curl_exec($ch),true);
    curl_close($ch);
    $id=$oo["parents"][0];
  }
}
mysqli_free_result($query);
mysqli_close($con);
?>
