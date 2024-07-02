<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$aaa=$argv[1];

$query=mysqli_query($con,"select id,title,artist from song where (isrc='' or isrc is null) and nomp3=0 and artist like '%$aaa%' order by rand()");
$j=0;
for($i=1;$i<=500;$i++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $artist=$row["artist"];
  $title=$row["title"];
  $qqq=urlencode("artist:$artist track:$title");
  echo $qqq."\n";
  $access_token=file_get_contents("access_token_spotify");
  $ch=curl_init();
  curl_setopt($ch,CURLOPT_URL,"https://api.spotify.com/v1/search?type=track&q=$qqq&limit=1");
  curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
  curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
  curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
  $oo=json_decode(curl_exec($ch),true);
  curl_close($ch);
  @$isrc=$oo["tracks"]["items"][0]["external_ids"]["isrc"];
  if(strlen($isrc)==12){
    $j++;
    echo "$i,$j | $title | $artist | $isrc\n";
    mysqli_query($con,"update song set isrc='$isrc' where id='$id'");
  }
  sleep(1);
}
mysqli_close($con);
?>
