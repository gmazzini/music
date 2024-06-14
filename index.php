<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"]; @$liv=$_GET["liv"]; @$idin=$_GET["idin"]; @$plin=$_GET["pl"]; 
@$pla=$_GET["pla"]; @$go=$_GET["go"]; @$act=$_GET["act"]; @$posin=(int)$_GET["pos"];
@$search=$_GET["search"];

echo "<style>\n";
echo "body {background-color: #F9F4B7; }\n";
echo ".mybut {background-color: #666699; border: none; color: #F9F4B7; padding: 10px 20px; text-align: center; text-decoration: none;\n";
echo "display: inline-block; font-size: 16px; border-radius: 15px; outline:none; }\n";
echo ".mybut:hover {background-color: #402B8; }\n";
echo "</style>\n";

// authentication
if(strlen($passwd)>6)$pwdmd5=md5($passwd);
else $pwdmd5=$_GET["pwdmd5"];
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

echo "<pre><a href='?liv=1&pwdmd5=$pwdmd5&go=DIR'>DIRECTORY</a>";
echo " <a href='?pwdmd5=$pwdmd5&go=SRC'>SEARCH</a>";
echo " <a href='?pwdmd5=$pwdmd5&go=LST'>LIST</a>";
echo " <a href='?pwdmd5=$pwdmd5&go=PLY'>PLAY</a>";
echo " <a href='?pwdmd5=$pwdmd5&go=MNG'>MANAGE</a></pre><hr>";

if($go=="")$go="PLY";
switch($go){
  
  // directory
  case "DIR":
  if($liv=="")$liv=1;
  switch($liv){
    case 1:
    $idprev="root";
    $prevliv=1;
    $idin="root";
    $nextliv=2;
    $db="music";
    break;
    case 2:
    $query=mysqli_query($con,"select parent from music where id='$idin'");
    $row=mysqli_fetch_assoc($query);
    $idprev=$row["parent"];
    mysqli_free_result($query);
    $prevliv=1;
    $nextliv=3;
    $db="music";
    break;
    case 3:
    $query=mysqli_query($con,"select parent from music where id='$idin'");
    $row=mysqli_fetch_assoc($query);
    $idprev=$row["parent"];
    mysqli_free_result($query);
    $prevliv=2;
    $nextliv=4;
    $db="song";
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
    $query=mysqli_query($con,"select parent from song where id='$idin'");
    $row=mysqli_fetch_assoc($query);
    $idin=$row["parent"];
    mysqli_free_result($query);
    $query=mysqli_query($con,"select parent from music where id='$idin'");
    $row=mysqli_fetch_assoc($query);
    $idprev=$row["parent"];
    mysqli_free_result($query);
    $prevliv=2;
    $nextliv=4;
    $liv=3;
    $db="song";
    break;
  }
  echo "<pre>$first liv:$liv, idin:$idin idprev:$idprev <a href='?liv=$prevliv&idin=$idprev&pwdmd5=$pwdmd5&go=DIR'>Prev</a>\n";
  $query=mysqli_query($con,"select id,name from $db where parent='$idin' order by name");
  for(;;){
    $row=mysqli_fetch_assoc($query);
    if($row==null)break;
    $id=$row["id"];
    $name=$row["name"];
    if($liv<3)echo "<a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&go=DIR'>$name</a>\n";
    else {
      echo "$name";
      for($i=0;$i<$ipl;$i++){
        $apl=$pl[$i];
        $query1=mysqli_query($con,"select position from playlist where label='$apl' and pwdmd5='$pwdmd5' and id='$id'");
        $row1=mysqli_fetch_assoc($query1);
        $position=(int)$row1["position"];
        mysqli_free_result($query1);
        if($position==0)echo " <a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&pl=$apl&pla=1&go=DIR'>+$apl</a>";
        else echo " <a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&pl=$apl&pla=2&go=DIR'>-$apl</a>";
      }
      echo "\n";
    }
  }
  echo "</pre>";
  mysqli_free_result($query);
  break;

   // search
  case "SRC":
  echo "<form>";
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
  $query=mysqli_query($con,"select id from song where name like '%$search%' order by name");
  for($i=0;;$i++){
    $row=mysqli_fetch_assoc($query);
    if($row==null)break;
    $id=$row["id"];
    $query1=mysqli_query($con,"select name,parent from song where id='$id'");
    $row1=mysqli_fetch_assoc($query1);
    $name=$row1["name"];
    $parent=$row1["parent"];
    mysqli_free_result($query1);
    $query1=mysqli_query($con,"select name,parent from music where id='$parent'");
    $row1=mysqli_fetch_assoc($query1);
    $liv2=$row1["name"];
    $parent=$row1["parent"];
    mysqli_free_result($query1);
    $query1=mysqli_query($con,"select name from music where id='$parent'");
    $row1=mysqli_fetch_assoc($query1);
    $liv1=$row1["name"];
    mysqli_free_result($query1);
    echo "$id | $name | $liv2 | $liv1 ";
    for($i=0;$i<$ipl;$i++){
      $apl=$pl[$i];
      $query1=mysqli_query($con,"select position from playlist where label='$apl' and pwdmd5='$pwdmd5' and id='$id'");
      $row1=mysqli_fetch_assoc($query1);
      $position=(int)$row1["position"];
      mysqli_free_result($query1);
      if($position==0)echo " <a href='?idin=$id&pwdmd5=$pwdmd5&pl=$apl&pla=1&go=SRC&search=$search'>+$apl</a>";
      else echo " <a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&pl=$apl&pla=2&go=SRC&search=$search'>-$apl</a>";
    }
    echo "\n";
  }
  echo "</pre>";
  break;

  // action on playlist
  case "LST":
  echo "<pre>$first\n";
  for($i=0;$i<$ipl;$i++)echo "<a href='?pl=$pl[$i]&pwdmd5=$pwdmd5&go=LST'>$pl[$i] $description[$i]</a>\n";
  switch($act){
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
    $query1=mysqli_query($con,"select name,parent from song where id='$id'");
    $row1=mysqli_fetch_assoc($query1);
    $name=$row1["name"];
    $parent=$row1["parent"];
    mysqli_free_result($query1);
    $query1=mysqli_query($con,"select name,parent from music where id='$parent'");
    $row1=mysqli_fetch_assoc($query1);
    $liv2=$row1["name"];
    $parent=$row1["parent"];
    mysqli_free_result($query1);
    $query1=mysqli_query($con,"select name from music where id='$parent'");
    $row1=mysqli_fetch_assoc($query1);
    $liv1=$row1["name"];
    mysqli_free_result($query1);
    echo "<a href=?act=C&pl=$plin&pwdmd5=$pwdmd5&pos=$position&go=LST>C</a> ";
    echo "<a href=?act=U&pl=$plin&pwdmd5=$pwdmd5&pos=$position&go=LST>U</a> ";
    echo "<a href=?act=D&pl=$plin&pwdmd5=$pwdmd5&pos=$position&go=LST>D</a> ";
    echo " $position | $id | $name | $liv2 | $liv1\n";
  }
  echo "<pre>";
  mysqli_free_result($query);
  break;
  
  // play
  case "PLY":
  echo "<pre>$first $plin\n";
  for($i=0;$i<$ipl;$i++)echo "<a href='?pl=$pl[$i]&pwdmd5=$pwdmd5&go=PLY'>$pl[$i] $description[$i]</a>\n";
  $query=mysqli_query($con,"select id,position from playlist where pwdmd5='$pwdmd5' and label='$plin' order by position");
  for($i=0;;$i++){
    $row=mysqli_fetch_assoc($query);
    if($row==null)break;
    $id[$i]=$row["id"];
    $query1=mysqli_query($con,"select name,parent from song where id='$id[$i]'");
    $row1=mysqli_fetch_assoc($query1);
    $data[$i]=$i." | ".mys($row1["name"]);
    $parent=$row1["parent"];
    mysqli_free_result($query1);
    $query1=mysqli_query($con,"select name,parent from music where id='$parent'");
    $row1=mysqli_fetch_assoc($query1);
    $data[$i].=" | ".mys($row1["name"]);
    $parent=$row1["parent"];
    mysqli_free_result($query1);
    $query1=mysqli_query($con,"select name from music where id='$parent'");
    $row1=mysqli_fetch_assoc($query1);
    $data[$i].=" | ".mys($row1["name"]);
    mysqli_free_result($query1);
  }
  mysqli_free_result($query);
  echo "<audio autoplay controls id='Player' src='load.php?id=$id[0]' onclick='this.paused ? this.play() : this.pause();'>Nooo</audio>\n";
  echo "<script>\n";
  echo "var src=["; for($j=0;$j<$i;$j++){if($j>0)echo ","; echo "'load.php?id=$id[$j]'";} echo "]\n";
  echo "var desc=["; for($j=0;$j<$i;$j++){if($j>0)echo ",";echo "'$data[$j]'";} echo "]\n";
  echo "var elm=0;\n";
  echo "var Player=document.getElementById('Player');\n";
  echo "Player.onended=function(){\n";
  echo "  elm++;\n";
  echo "  if(elm < src.length){\n";
  echo "    Player.src=src[elm];\n";
  echo "    Player.play();\n";
  echo "    myshow();\n";
  echo "  }\n";
  echo "}\n";
  echo "Player.onloadstart=function(){\n";
  echo "  myshow();\n";
  echo "}\n";
  echo "function next(){\n";
  echo "  elm++;\n";
  echo "  if(elm < src.length){\n";
  echo "    Player.src=src[elm];\n";
  echo "    Player.play();\n";
  echo "    myshow();\n";
  echo "  }\n";
  echo "  else elm=src.length-1;\n";
  echo "}\n";
  echo "function prev(){\n";
  echo "  elm--;\n";
  echo "  if(elm >= 0){\n";
  echo "    Player.src=src[elm];\n";
  echo "    Player.play();\n";
  echo "    myshow();\n";
  echo "  }\n";
  echo "  else elm=0;\n";
  echo "}\n";
  echo "function myshuffle(){\n";
  echo "  for(i=desc.length-1;i > 0;i--){\n";
  echo "    j=Math.floor(Math.random()*(i+1))\n";
  echo "    temp=desc[i]; desc[i]=desc[j]; desc[j]=temp;\n";
  echo "    temp=src[i]; src[i]=desc[j]; src[j]=temp;\n";
  echo "  }\n";
  echo "  myshow();\n";
  echo "}\n";
  echo "function myshow(){\n";
  echo "  aux='';\n";
  echo "  for(i=0;i < src.length;i++){\n";
  echo "    if(i==elm)aux+='!!! ';\n";
  echo "    aux+=desc[i]+'\\n';\n";
  echo "  }\n";
  echo "  document.getElementById('mylist').textContent=aux;\n";
  echo "}\n";
  echo "</script>\n";
  echo "<button class='mybut' onclick='prev()'>prev</button><button class='mybut' onclick='next()'>next</button><button class='mybut' onclick='myshuffle()'>shuffle</button>\n";
  echo "<pre><span id='mylist'></span></pre>\n";
  echo "<pre>";
  break;

  // manage
  case "MNG":
  switch($act){
    case "create":
    @$aux1=$_GET["par1"]; @$aux2=$_GET["par2"];
    if(ctype_alnum($aux1) && strlen($aux2)>4)mysqli_query($con,"insert into playlist_desc (pwdmd5,label,description) values ('$pwdmd5','$aux1','$aux2')");
    break;
    case "relabel":
    @$aux1=$_GET["par4"]; @$aux2=$_GET["par5"];
    if(ctype_alnum($aux1) && ctype_alnum($aux2)){
      mysqli_query($con,"update playlist_desc set label='$aux2' where pwdmd5='$pwdmd5' and label='$aux1'");
      mysqli_query($con,"update playlist set label='$aux2' where pwdmd5='$pwdmd5' and label='$aux1'");
    }
    case "rename":
    @$aux1=$_GET["par6"]; @$aux2=$_GET["par7"];
    if(ctype_alnum($aux1) && strlen($aux2)>4)mysqli_query($con,"update playlist_desc set description='$aux2' where pwdmd5='$pwdmd5' and label='$aux1'");
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
  echo "<pre><form>";
  echo "<input type=submit name=act value=create> label:<input type=text name=par1 size=8> description:<input type=text name=par2 size=100>\n";
  echo "<input type=submit name=act value=remove> label:<input type=text name=par3 size=8>\n";
  echo "<input type=submit name=act value=relabel> labelorg:<input type=text name=par4 size=8> labeldest:<input type=text name=par5 size=8>\n";
  echo "<input type=submit name=act value=rename> labelorg:<input type=text name=par6 size=8> dest:<input type=text name=par7 size=100>";
  echo "<input type=hidden name=pwdmd5 value='$pwdmd5'>";
  echo "<input type=hidden name=go value='MNG'>";
  echo "</form></pre>";
  break;

}
mysqli_close($con);
function mys($s){
  return str_replace("'","\'",$s);
}
?>
