<?php
@$plin=$_POST["pl"]; @$act=$_POST["act"]; @$spwdmd5in=$_POST["spwdmd5"];
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
myz("act","shuffle","go","PLY","pwdmd5",$pwdmd5,"pl",$plin,"spwdmd5",$spwdmd5in);
echo "\n";
for($i=0;$i<$ipl;$i++){
  echo "$description[$i] ";
  myz("pl",$pl[$i],"go","PLY","pwdmd5",$pwdmd5,"spwdmd5","");
  echo "\n";
}
for($i=0;$i<$ispl;$i++){
  echo "$sdescription[$i] ";
  myz("pl",$spl[$i],"go","PLY","pwdmd5",$pwdmd5,"spwdmd5",$spwdmd5[$i]);
  echo "\n";
}
if($spwdmd5in=="")$query=mysqli_query($con,"select id from playlist where pwdmd5='$pwdmd5' and label='$plin' order by position");
else $query=mysqli_query($con,"select id from playlist where pwdmd5='$spwdmd5in' and label='$plin' order by position");
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
echo "var msg='#play';\n";
echo "var pos=0;\n";
echo "myscroll();\n";
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
echo "  msg=desc[elm];\n";
echo "  pos=0;\n";
echo "}\n";
echo "function myscroll(){\n";
echo "  document.title=msg.substring(pos,msg.length)+msg.substring(0,pos);\n";
echo "  if(++pos > msg.length)pos=0;\n";
echo "  window.setTimeout('myscroll()',1000);\n";
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
?>
