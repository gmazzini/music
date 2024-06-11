<?php
shell_exec("scp admgm02@100.100.0.30:/home/www/data/access_token /home/www/music.mazzini.org");
$access_token=file_get_contents("access_token");
@$idin=$_GET["id"];
if(!file_exists("cached/$id")){
  $ch=curl_init();
  curl_setopt($ch,CURLOPT_URL,"https://www.googleapis.com/drive/v3/files/$id?alt=media");
  curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
  curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
  curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Authorization: Bearer ".$access_token));
  $oo=curl_exec($ch);
  curl_close($ch);
  file_put_contents("cached/$id",$oo);
}
$aux=file_get_contents("cached/$id");
header('Content-type: audio/mpeg;');
header("Content-Length: ".strlen($aux));
echo $aux;
?>
