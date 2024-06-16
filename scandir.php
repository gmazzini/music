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
  $id=$v["id"];
  // echo "$id\n";
  $qq="%27$id%27+in+parents";
  $ch2=curl_init();
  curl_setopt($ch2,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files?q=$qq&pageSize=500");
  curl_setopt($ch2,CURLOPT_RETURNTRANSFER,1);
  curl_setopt($ch2,CURLOPT_SSL_VERIFYPEER,FALSE);
  curl_setopt($ch2,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
  $oo2=json_decode(curl_exec($ch2),true);
  curl_close($ch2);
  $files2=$oo2["files"];
  foreach($files2 as $k2 => $v2){
    $id2=$v2["id"];
    // echo "\t$id2\n";
    $qq="%27$id2%27+in+parents";
    $ch3=curl_init();
    curl_setopt($ch3,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files?q=$qq&pageSize=500");
    curl_setopt($ch3,CURLOPT_RETURNTRANSFER,1);
    curl_setopt($ch3,CURLOPT_SSL_VERIFYPEER,FALSE);
    curl_setopt($ch3,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
    $oo3=json_decode(curl_exec($ch3),true);
    curl_close($ch3);
    $files3=$oo3["files"];
    foreach($files3 as $k3 => $v3){
      $id3=$v3["id"];
      $query=mysqli_query($con,"select count(id) from song where id='$id3'");
      $row=mysqli_fetch_row($query);
      $cc=(int)$row[0];
      mysqli_free_result($query);
      // echo "\t\t$id3 $cc\n";
      if($cc==0)echo "$id3 $id2 $id\n";
    }
  }
}
mysqli_close($con);
?>
