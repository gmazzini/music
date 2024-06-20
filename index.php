<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"]; @$go=$_POST["go"];

echo "<style>\n";
echo "body {background-color: #F9F4B7; }\n";
echo ".mybut {background-color: #666699; border: none; color: #F9F4B7; padding: 3px 3px; text-align: center; text-decoration: none;\n";
echo "display: inline-block; font-size: 12px; border-radius: 3px; outline: none; }\n";
echo ".mybut:hover {background-color: #FF0000; }\n";
echo "form {display: inline-block; padding: 0px 0px; margin: 0px 0px;}\n";
echo "input[type=submit] {padding: 2px 2px; margin: 2px 0px; cursor: pointer; background-color: #FF0000; color: white; border: none; border-radius: 4px;}\n";
echo "</style>\n";

// authentication
if(strlen($passwd)>6)$pwdmd5=md5($passwd);
else $pwdmd5=$_POST["pwdmd5"];
$query=mysqli_query($con,"select first from login where pwdmd5='$pwdmd5'");
$row=mysqli_fetch_assoc($query);
$first=$row["first"];
mysqli_free_result($query);
if($pwdmd5=="" || $first==""){
  echo "<form method=post>";
  echo "passwd <input type=text name=passwd size=16>";
  echo "<input type=submit name=act value=Enter>";
  echo "</form>";
  exit(0);
}

// list of playlist
$query=mysqli_query($con,"select label,description from playlist_desc where pwdmd5='$pwdmd5' order by label");
for($ipl=0;;$ipl++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $pl[$ipl]=$row["label"];
  $description[$ipl]=$row["description"];
}
mysqli_free_result($query);
$query=mysqli_query($con,"select label,description,pwdmd5 from playlist_desc where shared>0 and pwdmd5<>'$pwdmd5' order by label");
for($ispl=0;;$ispl++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $spl[$ispl]=$row["label"];
  $spwdmd5=$row["pwdmd5"];
  $query1=mysqli_query($con,"select first from login where pwdmd5='$spwdmd5'");
  $row1=mysqli_fetch_assoc($query);
  $aux=$row1["first"];
  mysqli_free_result($query1);
  $sdescription[$ispl]="($sfirst) ".$row["description"];
}
mysqli_free_result($query);

echo "<pre>";
myz("go","DIR","pwdmd5","$pwdmd5","liv",1);
echo " "; myz("go","SRC","pwdmd5","$pwdmd5");
echo " "; myz("go","LST","pwdmd5","$pwdmd5");
echo " "; myz("go","PLY","pwdmd5","$pwdmd5");
echo " "; myz("go","MNG","pwdmd5","$pwdmd5");
echo " music by GM ver 1.3";
echo "</pre><hr>";

if($go=="")$go="PLY";
switch($go){
  
  // directory
  case "DIR":
  @$liv=(int)$_POST["liv"]; @$artist=$_POST["artist"]; @$album=$_POST["album"]; @$plin=$_POST["pl"]; @$idin=$_POST["id"]; @$pla=$_POST["pla"];
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
    $query=mysqli_query($con,"select id,title,duration,played from song where artist='$artist' and album='$album' and nomp3=0 order by title");
    for(;;){
      $row=mysqli_fetch_assoc($query);
      if($row==null)break;
      $id=$row["id"];
      $title=$row["title"];
      $duration=$row["duration"];
      $played=$row["played"];
      myz("act","P","id",$id,"go","DIR","pwdmd5",$pwdmd5,"liv","4","artist",$artist,"album",$album,"pla",3);
      echo " [$duration,$played] $title | $album | $artist ";
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
      echo "\n";
    }
    mysqli_free_result($query);
    break;
    
  }
  echo "</pre>";
  break;
  
  // search
  case "SRC":
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
  $query=mysqli_query($con,"select id,title,album,artist,duration,played from song where title like '%$search%' and nomp3=0 order by title");
  for($i=0;;$i++){
    $row=mysqli_fetch_assoc($query);
    if($row==null)break;
    $id=$row["id"];
    $title=$row["title"];
    $album=$row["album"];
    $artist=$row["artist"];
    $duration=$row["duration"];
    $played=$row["played"];
    myz("act","P","id",$id,"go","SRC","pwdmd5",$pwdmd5,"artist",$artist,"album",$album,"pla",3,"search",$search);
    echo " [$duration,$played] $title | $album | $artist ";
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
    echo "\n";
  }
  echo "</pre>";
  break;

  // action on playlist
  case "LST":
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
    $query1=mysqli_query($con,"select title,album,artist,duration,played from song where id='$id'");
    $row1=mysqli_fetch_assoc($query1);
    $title=$row1["title"];
    $album=$row1["album"];
    $artist=$row1["artist"];
    $duration=$row1["duration"];
    $played=$row1["played"];
    mysqli_free_result($query1);
    myz("act","P","go","LST","pwdmd5",$pwdmd5,"pos",$position,"pl",$plin);
    echo " ";
    myz("act","C","go","LST","pwdmd5",$pwdmd5,"pos",$position,"pl",$plin);
    echo " ";
    myz("act","U","go","LST","pwdmd5",$pwdmd5,"pos",$position,"pl",$plin);
    echo " ";
    myz("act","D","go","LST","pwdmd5",$pwdmd5,"pos",$position,"pl",$plin);
    echo " $position [$duration,$played] $title | $album | $artist\n";
  }
  echo "<pre>";
  mysqli_free_result($query);
  break;
  
  // play
  case "PLY":
  @$plin=$_POST["pl"]; @$act=$_POST["act"];
  if($act=="P"){
    @$artist=$_POST["artist"]; @$album=$_POST["album"];
    mysqli_query($con,"delete from playlist where pwdmd5='$pwdmd5' and label='TMP'");
    $query=mysqli_query($con,"select id from song where artist='$artist' and album='$album' and nomp3=0 order by title");
    for($i=1;;$i++){
      $row=mysqli_fetch_assoc($query);
      if($row==null)break;
      $idaux=$row["id"];
      mysqli_query($con,"insert into playlist (id,position,label,pwdmd5) values ('$idaux',$i,'TMP','$pwdmd5')");
    }
    mysqli_free_result($query);
  }
  echo "<pre>$first $plin ";
  myz("act","shuffle","go","PLY","pwdmd5",$pwdmd5,"pl",$plin);
  echo "\n";
  for($i=0;$i<$ipl;$i++){
    echo "$description[$i] ";
    myz("pl",$pl[$i],"go","PLY","pwdmd5",$pwdmd5);
    echo "\n";
  }
  echo "ciao\n";
  for($i=0;$i<$ispl;$i++){
    echo "$sdescription[$i] ";
    myz("pl",$spl[$i],"go","PLY","pwdmd5",$pwdmd5);
    echo "\n";
  }
  $query=mysqli_query($con,"select id from playlist where pwdmd5='$pwdmd5' and label='$plin' order by position");
  for($i=0;;$i++){
    $row=mysqli_fetch_assoc($query);
    if($row==null)break;
    $id[$i]=$row["id"];
    $query1=mysqli_query($con,"select title,album,artist,duration,played from song where id='$id[$i]'");
    $row1=mysqli_fetch_assoc($query1);
    $title=$row1["title"];
    $album=$row1["album"];
    $artist=$row1["artist"];
    $duration=$row1["duration"];
    $played=$row1["played"];
    mysqli_free_result($query1);
    $data[$i]=mys("[$duration,$played] $title | $album | $artist");
  }
  mysqli_free_result($query);
  if($act=="shuffle"){
    $order=range(0,$i-1);
    shuffle($order);
    array_multisort($order,$id,$data);
  }
  echo "<audio autoplay controls id='Player' src='cached/$id[0]'></audio>\n";
  echo "<script>\n";
  echo "var src=["; for($j=0;$j<$i;$j++){if($j>0)echo ","; echo "'$id[$j]'";} echo "]\n";
  echo "var desc=["; for($j=0;$j<$i;$j++){if($j>0)echo ",";echo "'$data[$j]'";} echo "]\n";
  echo "var elm=0;\n";
  echo "var Player=document.getElementById('Player');\n";
  echo "Player.onended=function(){\n";
  echo "  elm++;\n";
  echo "  if(elm < src.length){\n";
  echo "    Player.src='cached/'+src[elm];\n";
  echo "    Player.play();\n";
  echo "  }\n";
  echo "}\n";
  echo "Player.onloadstart=function(){\n";
  echo "  aux='';\n";
  echo "  for(i=0;i < src.length;i++){\n";
  echo "    if(i==elm){aux+='>> '; xhttp=new XMLHttpRequest(); xhttp.open('GET','played.php?id='+src[elm],true); xhttp.send();}\n";
  echo "    else aux+='   ';\n";
  echo "    aux+=desc[i]+'\\n';\n";
  echo "  }\n";
  echo "  document.getElementById('mylist').textContent=aux;\n";
  echo "}\n";
  echo "function next(){\n";
  echo "  elm++;\n";
  echo "  if(elm < src.length){\n";
  echo "    Player.src='cached/'+src[elm];\n";
  echo "    Player.play();\n";
  echo "  }\n";
  echo "  else elm=src.length-1;\n";
  echo "}\n";
  echo "function prev(){\n";
  echo "  elm--;\n";
  echo "  if(elm >= 0){\n";
  echo "    Player.src='cached/'+src[elm];\n";
  echo "    Player.play();\n";
  echo "  }\n";
  echo "  else elm=0;\n";
  echo "}\n";
  echo "</script>\n";
  echo "<button class='mybut' onclick='prev()'>prev</button> <button class='mybut' onclick='next()'>next</button>\n";
  echo "<pre><span id='mylist'></span></pre>\n";
  echo "<pre>";
  break;

  // manage
  case "MNG":
  @$act=$_POST["act"];
  switch($act){
    case "create":
    @$aux1=$_POST["par1"]; @$aux2=$_POST["par2"];
    if(ctype_alnum($aux1) && strlen($aux2)>4)mysqli_query($con,"insert into playlist_desc (pwdmd5,label,description) values ('$pwdmd5','$aux1','$aux2')");
    break;
    case "remove":
    @$aux1=$_POST["par3"];
    if(ctype_alnum($aux1)){
      mysqli_query($con,"delete from playlist_desc where pwdmd5='$pwdmd5' and label='$aux1'");
      mysqli_query($con,"delete from playlist where pwdmd5='$pwdmd5' and label='$aux1'");
    }
    break;
    case "relabel":
    @$aux1=$_POST["par4"]; @$aux2=$_POST["par5"];
    if(ctype_alnum($aux1) && ctype_alnum($aux2)){
      mysqli_query($con,"update playlist_desc set label='$aux2' where pwdmd5='$pwdmd5' and label='$aux1'");
      mysqli_query($con,"update playlist set label='$aux2' where pwdmd5='$pwdmd5' and label='$aux1'");
    }
    case "rename":
    @$aux1=$_POST["par6"]; @$aux2=$_POST["par7"];
    if(ctype_alnum($aux1) && strlen($aux2)>4)mysqli_query($con,"update playlist_desc set description='$aux2' where pwdmd5='$pwdmd5' and label='$aux1'");
    break;
    case "download":
    @$aux1=$_POST["par8"];
    $myname=rand().rand().rand().rand().".list";
    $ffname="download/$myname";
    $fp=fopen($ffname,"w");
    $query=mysqli_query($con,"select id from playlist where pwdmd5='$pwdmd5' and label='$aux1' order by position");
    for(;;){
      $row=mysqli_fetch_assoc($query);
      if($row==null)break;
      fprintf($fp,"%s\n",$row["id"]);
    }
    mysqli_free_result($query);
    fclose($fp);
    echo "<pre><a href='$ffname' download>Download</a><br>";
    break;
    case "random":
    @$aux1=$_POST["par9"];
    $query=mysqli_query($con,"select id from playlist where pwdmd5='$pwdmd5' and label='$aux1'");
    for($i=0;;$i++){
      $row=mysqli_fetch_assoc($query);
      if($row==null)break;
      $id[$i]=$row["id"];
    }
    mysqli_free_result($query);
    $order=range(0,$i-1);
    shuffle($order);
    for($j=0;$j<$i;$j++){
      $aux=$id[$order[$j]];
      mysqli_query($con,"update playlist set position=$j where pwdmd5='$pwdmd5' and label='$aux1' and id='$aux'");
    }
    break;
    case "sorttitle":
    @$aux1=$_POST["par10"];
    $query=mysqli_query($con,"select id from playlist where pwdmd5='$pwdmd5' and label='$aux1'");
    for($i=0;;$i++){
      $row=mysqli_fetch_assoc($query);
      if($row==null)break;
      $id[$i]=$row["id"];
      $query1=mysqli_query($con,"select title from song where id='$id[$i]'");
      $row1=mysqli_fetch_assoc($query1);
      $title[$i]=$row1["title"];
      mysqli_free_result($query1);
    }
    mysqli_free_result($query);
    $order=range(0,$i-1);
    array_multisort($title,$order);
    for($j=0;$j<$i;$j++){
      $aux=$id[$order[$j]];
      mysqli_query($con,"update playlist set position=$j where pwdmd5='$pwdmd5' and label='$aux1' and id='$aux'");
    }
    break;
    case "sortalbum":
    @$aux1=$_POST["par11"];
    $query=mysqli_query($con,"select id from playlist where pwdmd5='$pwdmd5' and label='$aux1'");
    for($i=0;;$i++){
      $row=mysqli_fetch_assoc($query);
      if($row==null)break;
      $id[$i]=$row["id"];
      $query1=mysqli_query($con,"select album from song where id='$id[$i]'");
      $row1=mysqli_fetch_assoc($query1);
      $album[$i]=$row1["album"];
      mysqli_free_result($query1);
    }
    mysqli_free_result($query);
    $order=range(0,$i-1);
    array_multisort($album,$order);
    for($j=0;$j<$i;$j++){
      $aux=$id[$order[$j]];
      mysqli_query($con,"update playlist set position=$j where pwdmd5='$pwdmd5' and label='$aux1' and id='$aux'");
    }
    break;
    case "sortartist":
    @$aux1=$_POST["par12"];
    $query=mysqli_query($con,"select id from playlist where pwdmd5='$pwdmd5' and label='$aux1'");
    for($i=0;;$i++){
      $row=mysqli_fetch_assoc($query);
      if($row==null)break;
      $id[$i]=$row["id"];
      $query1=mysqli_query($con,"select artist from song where id='$id[$i]'");
      $row1=mysqli_fetch_assoc($query1);
      $artist[$i]=$row1["artist"];
      mysqli_free_result($query1);
    }
    mysqli_free_result($query);
    $order=range(0,$i-1);
    array_multisort($artist,$order);
    for($j=0;$j<$i;$j++){
      $aux=$id[$order[$j]];
      mysqli_query($con,"update playlist set position=$j where pwdmd5='$pwdmd5' and label='$aux1' and id='$aux'");
    }
    break;
  }
  $query=mysqli_query($con,"select label,description from playlist_desc where pwdmd5='$pwdmd5' order by label");
  for($ipl=0;;$ipl++){
    $row=mysqli_fetch_assoc($query);
    if($row==null)break;
    $pl[$ipl]=$row["label"];
    $description[$ipl]=$row["description"];
  }
  mysqli_free_result($query);
  echo "<pre>$first $plin\n";
  for($i=0;$i<$ipl;$i++)echo "$pl[$i] $description[$i]\n";
  echo "<pre><form method='post'>";
  echo "<input type='submit' name='act' value='create'> label:<input type='text' name='par1' size=8> description:<input type='text' name='par2' size=100>\n";
  echo "<input type='submit' name='act' value='remove'> label:<input type='text' name='par3' size=8>\n";
  echo "<input type='submit' name='act' value='relabel'> labelorg:<input type='text' name='par4' size=8> labeldest:<input type='text' name='par5' size=8>\n";
  echo "<input type='submit' name='act' value='rename'> labelorg:<input type='text' name='par6' size=8> dest:<input type='text' name='par7' size=100>\n";
  echo "<input type='submit' name='act' value='download'> label:<input type='text' name='par8' size=8>\n";
  echo "<input type='submit' name='act' value='random'> label:<input type='text' name='par9' size=8>\n";
  echo "<input type='submit' name='act' value='sorttitle'> label:<input type='text' name='par10' size=8>\n";
  echo "<input type='submit' name='act' value='sortalbum'> label:<input type='text' name='par11' size=8>\n";
  echo "<input type='submit' name='act' value='sortartist'> label:<input type='text' name='par12' size=8>\n";
  echo "<input type='hidden' name='pwdmd5' value='$pwdmd5'>";
  echo "<input type='hidden' name='go' value='MNG'>";
  echo "</form></pre>";
  break;

}
mysqli_close($con);
function mys($s){
  return str_replace("'","\'",$s);
}
function myz(...$par){
  $n=count($par);
  echo "<form method='post'>";
  echo "<input type='submit' name='$par[0]' value='$par[1]'>";
  for($i=1;$i<$n/2;$i++){
    $aux1=$par[2*$i];
    $aux2=$par[2*$i+1];
    echo "<input type='hidden' name='$aux1' value='$aux2'>";
  }
  echo "</form>";
}
?>
