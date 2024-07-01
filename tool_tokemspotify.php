<?php
include "local.php";
$ch=curl_init();
curl_setopt($ch,CURLOPT_URL,"https://accounts.spotify.com/api/token");
curl_setopt($ch,CURLOPT_RETURNTRANSFER,1);
curl_setopt($ch,CURLOPT_POST,1);
curl_setopt($ch,CURLOPT_SSL_VERIFYPEER,FALSE);
curl_setopt($ch,CURLOPT_HTTPHEADER,Array("Content-Type: application/x-www-form-urlencoded"));
curl_setopt($ch,CURLOPT_POSTFIELDS,"grant_type=client_credentials&client_id=$client_id_spotify&client_secret=$client_secret_spotify");
$oo=json_decode(curl_exec($ch),true);
file_put_contents("access_token_spotify",$oo["access_token"]);
print_r($oo);
curl_close($ch);
?>
