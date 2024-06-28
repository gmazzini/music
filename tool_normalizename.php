<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id,title,album,artist from song");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $title=$row["title"];
  $album=$row["album"];
  $artist=$row["artist"];
  $pp=strpos($album,$artist.": ");
  if($pp!==false){
    $nn=substr($album,$pp+strlen($artist)+2);
    mysqli_query($con,"update song set album='$nn' where id='$id'");
    echo "$id\n...$album\n---$nn\n";
    $album=$nn;
  }
  $pp=strpos($album,"Absolute Disco - Alle Tiders ");
  if($pp!==false){
    $qqq=urlencode("artist:$artist track:$title");
    $access_token=file_get_contents("access_token_spotify");
    $ch=curl_init();
    curl_setopt($ch,CURLOPT_URL,"https://api.spotify.com/v1/search?type=track&q=$qqq&limit=1");
    curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
    curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
    curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
    $oo=json_decode(curl_exec($ch),true);
    curl_close($ch);
    @$nn=$oo["tracks"]["items"][0]["album"]["name"];
    echo "$id\n...$album\n---$nn\n";
    $album=$nn;
  }
}
mysqli_free_result($query);
mysqli_close($con);
?>
