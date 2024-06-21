<?php
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
  case "upload":
  @$aux1=$_POST["par15"];
  $hh=fopen($_FILES['par16']['tmp_name'],"r");
  for(;;){
    if(eof($hh))break;
    $line=trim(fgets($hh));
    echo "$line\n";
  }
  fclose($hh);
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
  case "sharedoff":
  @$aux1=$_POST["par13"];
  mysqli_query($con,"update playlist_desc set shared=0 where pwdmd5='$pwdmd5' and label='$aux1'");
  break;
  case "sharedon":
  @$aux1=$_POST["par14"];
  mysqli_query($con,"update playlist_desc set shared=1 where pwdmd5='$pwdmd5' and label='$aux1'");
  break;
}
$query=mysqli_query($con,"select label,description,shared from playlist_desc where pwdmd5='$pwdmd5' order by label");
for($ipl=0;;$ipl++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $pl[$ipl]=$row["label"];
  $aux=$row["shared"];
  $description[$ipl]=$row["description"]."($aux)";
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
echo "<input type='submit' name='act' value='upload'> label:<input type='text' name='par15' size=8> <input type='file' name='par16'>\n";
echo "<input type='submit' name='act' value='random'> label:<input type='text' name='par9' size=8>\n";
echo "<input type='submit' name='act' value='sorttitle'> label:<input type='text' name='par10' size=8>\n";
echo "<input type='submit' name='act' value='sortalbum'> label:<input type='text' name='par11' size=8>\n";
echo "<input type='submit' name='act' value='sortartist'> label:<input type='text' name='par12' size=8>\n";
echo "<input type='submit' name='act' value='sharedoff'> label:<input type='text' name='par13' size=8>\n";
echo "<input type='submit' name='act' value='sharedon'> label:<input type='text' name='par14' size=8>\n";
echo "<input type='hidden' name='pwdmd5' value='$pwdmd5'>";
echo "<input type='hidden' name='go' value='MNG'>";
echo "</form></pre>";
?>
