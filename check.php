<?php
include "local.php";

$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id,title,album,artist from song where nomp3=0");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $title=$row["title"];
  $album=$row["album"];
  $artist=$row["artist"];
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
    if($i==0 && $title==$name)echo "Ok\n";
    elseif($i==1 && $album==$name)echo "Ok\n";
    elseif($i==2 && $artist==$name)echo "Ok\n";
    elseif($i==3 && "Music"==$name)echo "Ok\n";
    if($i==1 && $album!=$name){
      $ch=curl_init();
      curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files/$id");
      curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
      curl_setopt($ch,CURLOPT_POST,1);
      curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
      curl_setopt($ch,CURLOPT_HTTPHEADER,array("Content-Type: application/json","Authorization: Bearer ".$access_token));
      curl_setopt($ch,CURLOPT_CUSTOMREQUEST,"PATCH");
      curl_setopt($ch,CURLOPT_POSTFIELDS,'{"name": "'.$album.'"}');
      $oo=json_decode(curl_exec($ch),true);
      curl_close($ch);
    }
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
