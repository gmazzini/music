<?php
@$search=$_POST["search"]; @$artist=$_POST["artist"]; @$album=$_POST["album"]; @$plin=$_POST["pl"]; @$idin=$_POST["id"]; @$pla=$_POST["pla"];
echo "<form method='post'>";
echo "search <input type=text name=search size=16>";
echo "<input type=hidden name=pwdmd5 value='$pwdmd5'>";
echo "<input type=hidden name=go value='SRC'>";
echo "<input type=submit name=act value=Enter>";
echo "</form><pre>";
echo "Looking: $search\n";
if(strlen($search)<2)$search="ZZZZZ";
if($pla==1){
  $query=mysqli_query($con,"select max(position) from playlist where label='$plin' and pwdmd5='$pwdmd5'");
  $row=mysqli_fetch_row($query);
  $pllast=1+(int)$row[0];
  mysqli_free_result($query);
  mysqli_query($con,"insert into playlist (pwdmd5,id,position,label) values ('$pwdmd5','$idin',$pllast,'$plin')");
}
elseif($pla==2)mysqli_query($con,"delete from playlist where label='$plin' and pwdmd5='$pwdmd5' and id='$idin'");
elseif($pla==3)echo "<audio autoplay controls src='cached/$idin' onloadstart='xhttp=new XMLHttpRequest(); xhttp.open(\"GET\",\"played.php?id=$idin\",true); xhttp.send();'></audio>\n";
$query=mysqli_query($con,"select id,title,album,artist,duration,played,isrc from song where title like '%$search%' and nomp3=0 order by title");
for($i=0;;$i++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $title=$row["title"];
  $album=$row["album"];
  $artist=$row["artist"];
  $duration=$row["duration"];
  $played=$row["played"];
  $isrc=$row["isrc"];
  myz("act","P","id",$id,"go","SRC","pwdmd5",$pwdmd5,"artist",$artist,"album",$album,"pla",3,"search",$search);
  echo " ";
  for($i=0;$i<$ipl;$i++){
    $apl=$pl[$i];
    $query1=mysqli_query($con,"select position from playlist where label='$apl' and pwdmd5='$pwdmd5' and id='$id'");
    $row1=mysqli_fetch_assoc($query1);
    @$position=(int)$row1["position"];
    mysqli_free_result($query1);
    if($position==0){
      echo "+";
      myz("pl",$apl,"id",$id,"go","SRC","pwdmd5",$pwdmd5,"artist",$artist,"album",$album,"pla",1,"search",$search);
      echo " ";
    }
    else {
      echo "-";
      myz("pl",$apl,"id",$id,"go","SRC","pwdmd5",$pwdmd5,"artist",$artist,"album",$album,"pla",2,"search",$search);
      echo " ";
    }
  }
  echo " [$duration,$played] $title | $album | $artist | $isrc\n";
}
echo "</pre>";
?>
