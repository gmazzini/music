<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
$query=mysqli_query($con,"select id from song where duration=0 and nomp3=0");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  echo "$id\n";
  $access_token=file_get_contents("access_token");
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
    $oo=json_decode(shell_exec("ffprobe -v quiet -print_format json -show_format $ffname"),true);
    $ooo=$oo["format"];
    $duration=(int)$ooo["duration"];
    $format=$ooo["format_name"];
    $aux=$ooo["tags"]["title"];
    $aux=preg_replace("/[ ]{0,}\([^)]+\)[ ]{0,}/","",$aux);
    $aux=preg_replace("/[ ]{0,}\[[^)]+\][ ]{0,}/","",$aux);
    $title=mysqli_real_escape_string($con,$aux);
    $aux=$ooo["tags"]["album"];
    $aux=preg_replace("/[ ]{0,}\([^)]+\)[ ]{0,}/","",$aux);
    $aux=preg_replace("/[ ]{0,}\[[^)]+\][ ]{0,}/","",$aux);
    $album=mysqli_real_escape_string($con,$aux);
    $aux=$ooo["tags"]["artist"];
    $aux=preg_replace("/[ ]{0,}\([^)]+\)[ ]{0,}/","",$aux);
    $aux=preg_replace("/[ ]{0,}\[[^)]+\][ ]{0,}/","",$aux);
    $artist=mysqli_real_escape_string($con,$aux);
    if($format!="mp3"){
      unlink($ffname);
      mysqli_query($con,"update song set nomp3=1 where id='$id'");
    }
    else {
      $md5=md5_file($ffname,false);
      mysqli_query($con,"update song set played=played+1,duration=$duration,title='$title',album='$album',artist='$artist',md5='$md5' where id='$id'");
    }
    sleep(5);
  }
}
mysqli_free_result($query);
mysqli_close($con);
?>
