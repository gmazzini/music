<?php
@$plin=$_POST["pl"]; @$act=$_POST["act"]; @$posin=$_POST["pos"];
echo "<pre>$first\n";
for($i=0;$i<$ipl;$i++){
  echo "$description[$i] ";
  myz("pl",$pl[$i],"go","LST","pwdmd5",$pwdmd5);
  echo "\n";
}
switch($act){
  case "P":
  $query1=mysqli_query($con,"select id from playlist where pwdmd5='$pwdmd5' and position=$posin and label='$plin'");
  $row1=mysqli_fetch_assoc($query1);
  $id=$row1["id"];
  mysqli_free_result($query1);
  echo "<audio autoplay controls src='cached/$id' onloadstart='xhttp=new XMLHttpRequest(); xhttp.open(\"GET\",\"played.php?id=$idin\",true); xhttp.send();'></audio>\n";
  break;
  case "C":
  mysqli_query($con,"delete from playlist where pwdmd5='$pwdmd5' and position=$posin and label='$plin'");
  break;
  case "U":
  $query1=mysqli_query($con,"select min(position) from playlist where pwdmd5='$pwdmd5' and position>$posin and label='$plin'");
  $row1=mysqli_fetch_row($query1);
  $swap=(int)$row1[0];
  mysqli_free_result($query1);
  if($swap>0){
    mysqli_query($con,"update playlist set position=30000 where pwdmd5='$pwdmd5' and position=$posin and label='$plin'");
    mysqli_query($con,"update playlist set position=$posin where pwdmd5='$pwdmd5' and position=$swap and label='$plin'");
    mysqli_query($con,"update playlist set position=$swap where pwdmd5='$pwdmd5' and position=30000 and label='$plin'");
  }
  break;
  case "D":
  $query1=mysqli_query($con,"select max(position) from playlist where pwdmd5='$pwdmd5' and position<$posin and label='$plin'");
  $row1=mysqli_fetch_row($query1);
  $swap=(int)$row1[0];
  mysqli_free_result($query1);
  if($swap>0){
    mysqli_query($con,"update playlist set position=30000 where pwdmd5='$pwdmd5' and position=$posin and label='$plin'");
    mysqli_query($con,"update playlist set position=$posin where pwdmd5='$pwdmd5' and position=$swap and label='$plin'");
    mysqli_query($con,"update playlist set position=$swap where pwdmd5='$pwdmd5' and position=30000 and label='$plin'");
  }
  break;
}
$query=mysqli_query($con,"select id,position from playlist where pwdmd5='$pwdmd5' and label='$plin' order by position");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $position=$row["position"];
  $query1=mysqli_query($con,"select title,album,artist,duration,played,isrc from song where id='$id'");
  $row1=mysqli_fetch_assoc($query1);
  $title=$row1["title"];
  $album=$row1["album"];
  $artist=$row1["artist"];
  $duration=$row1["duration"];
  $played=$row1["played"];
  $isrc=$row1["isrc"];
  mysqli_free_result($query1);
  myz("act","P","go","LST","pwdmd5",$pwdmd5,"pos",$position,"pl",$plin);
  echo " ";
  myz("act","C","go","LST","pwdmd5",$pwdmd5,"pos",$position,"pl",$plin);
  echo " ";
  myz("act","U","go","LST","pwdmd5",$pwdmd5,"pos",$position,"pl",$plin);
  echo " ";
  myz("act","D","go","LST","pwdmd5",$pwdmd5,"pos",$position,"pl",$plin);
  echo " $position [$duration,$played] $title | $album | $artist | $isrc\n";
}
echo "<pre>";
mysqli_free_result($query);
?>
