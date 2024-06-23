<?php
@$liv=(int)$_POST["liv"]; @$artist=$_POST["artist"]; @$album=$_POST["album"]; @$plin=$_POST["pl"]; @$idin=$_POST["id"]; @$pla=$_POST["pla"];
echo "<script>document.title='#directory';</script>";
echo "<pre>";
switch($liv){
  case 1:
  $query=mysqli_query($con,"select unique(artist) from song where nomp3=0 order by artist");
  for($i=0;;$i++){
    $row=mysqli_fetch_row($query);
    if($row==null)break;
    $artist=$row[0];
    myz("artist",$artist,"go","DIR","pwdmd5",$pwdmd5,"liv","2");
    echo "\n";
  }
  mysqli_free_result($query);
  break;
  case 2:
  echo ">> $artist ";
  myz("go","DIR","pwdmd5","$pwdmd5","liv",1);
  echo "\n";
  $query=mysqli_query($con,"select unique(album) from song where artist='$artist' and nomp3=0 order by album");
  for(;;){
    $row=mysqli_fetch_row($query);
    if($row==null)break;
    $album=$row[0];
    myz("act","P","go","PLY","pwdmd5",$pwdmd5,"artist",$artist,"album",$album,"pl","TMP");
    echo " ";
    myz("album",$album,"go","DIR","pwdmd5",$pwdmd5,"liv","3","artist",$artist);
    echo "\n";
  }
  mysqli_free_result($query);
  break;
  case 4:
  if($pla==1){
    $query=mysqli_query($con,"select max(position) from playlist where label='$plin' and pwdmd5='$pwdmd5'");
    $row=mysqli_fetch_row($query);
    $pllast=1+(int)$row[0];
    mysqli_free_result($query);
    mysqli_query($con,"insert into playlist (pwdmd5,id,position,label) values ('$pwdmd5','$idin',$pllast,'$plin')");
  }
  elseif($pla==2)mysqli_query($con,"delete from playlist where label='$plin' and pwdmd5='$pwdmd5' and id='$idin'");
  elseif($pla==3)echo "<audio autoplay controls src='cached/$idin' onloadstart='xhttp=new XMLHttpRequest(); xhttp.open(\"GET\",\"played.php?id=$idin\",true); xhttp.send();'></audio>\n";
  case 3:
  echo ">> $artist >> $album ";
  myz("go","DIR","pwdmd5","$pwdmd5","liv",2,"artist",$artist);
  echo "\n";
  $query=mysqli_query($con,"select id,title,duration,played,isrc from song where artist='$artist' and album='$album' and nomp3=0 order by title");
  for(;;){
    $row=mysqli_fetch_assoc($query);
    if($row==null)break;
    $id=$row["id"];
    $title=$row["title"];
    $duration=$row["duration"];
    $played=$row["played"];
    $isrc=$row["isrc"];
    myz("act","P","id",$id,"go","DIR","pwdmd5",$pwdmd5,"liv","4","artist",$artist,"album",$album,"pla",3);
    echo " ";
    for($i=0;$i<$ipl;$i++){
      $apl=$pl[$i];
      $query1=mysqli_query($con,"select position from playlist where label='$apl' and pwdmd5='$pwdmd5' and id='$id'");
      $row1=mysqli_fetch_assoc($query1);
      @$position=(int)$row1["position"];
      mysqli_free_result($query1);
      if($position==0){
        echo "+";
        myz("pl",$apl,"id",$id,"go","DIR","pwdmd5",$pwdmd5,"liv","4","artist",$artist,"album",$album,"pla",1);
        echo " ";
      }
      else {
        echo "-";
        myz("pl",$apl,"id",$id,"go","DIR","pwdmd5",$pwdmd5,"liv","4","artist",$artist,"album",$album,"pla",2);
        echo " ";
      }
    }
    echo "[$duration,$played] $title | $album | $artist | $isrc\n";
  }
  mysqli_free_result($query);
  break;
}
echo "</pre>";
?>
