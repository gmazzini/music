<?php
include "local.php";
$version="1.4";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"]; @$go=$_POST["go"];

echo "<style>\n";
echo "body {background-color: #F9F4B7; font-size: 30px;}\n";
echo ".mybut {background-color: #666699; font-size: 30px; border: none; color: #F9F4B7; padding: 3px 3px; text-align: center; text-decoration: none;\n";
echo "display: inline-block; font-size: 30px; border-radius: 3px; outline: none; }\n";
echo ".mybut:hover {background-color: #FF0000; }\n";
echo "form {display: inline-block; padding: 0px 0px; margin: 0px 0px;}\n";
echo "input[type=submit] {padding: 2px 2px; margin: 2px 0px; cursor: pointer; background-color: #FF0000; font-size: 30px; color: white; border: none; border-radius: 4px;}\n";
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
$query=mysqli_query($con,"select label,description,shared from playlist_desc where pwdmd5='$pwdmd5' order by label");
for($ipl=0;;$ipl++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $pl[$ipl]=$row["label"];
  $aux=$row["shared"];
  $description[$ipl]=$row["description"]."($aux)";
}
mysqli_free_result($query);
$query=mysqli_query($con,"select label,description,pwdmd5 from playlist_desc where shared=1 and pwdmd5<>'$pwdmd5' order by label");
for($ispl=0;;$ispl++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $spl[$ispl]=$row["label"];
  $spwdmd5[$ispl]=$row["pwdmd5"];
  $query1=mysqli_query($con,"select first from login where pwdmd5='$spwdmd5[$ispl]'");
  $row1=mysqli_fetch_assoc($query1);
  $aux=$row1["first"];
  mysqli_free_result($query1);
  $sdescription[$ispl]="($aux) ".$row["description"];
}
mysqli_free_result($query);

echo "<pre>";
myz("go","DIR","pwdmd5","$pwdmd5","liv",1);
echo " "; myz("go","SRC","pwdmd5","$pwdmd5");
echo " "; myz("go","LST","pwdmd5","$pwdmd5");
echo " "; myz("go","PLY","pwdmd5","$pwdmd5");
echo " "; myz("go","MNG","pwdmd5","$pwdmd5");
echo " music by GM ver $version";
echo "</pre><hr>";

if($go=="")$go="PLY";
switch($go){
  case "DIR": include "run_directory.php"; break;
  case "SRC": include "run_search.php"; break;
  case "LST": include "run_list.php"; break;
  case "PLY": include "run_play.php"; break;

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
  echo "<input type='submit' name='act' value='random'> label:<input type='text' name='par9' size=8>\n";
  echo "<input type='submit' name='act' value='sorttitle'> label:<input type='text' name='par10' size=8>\n";
  echo "<input type='submit' name='act' value='sortalbum'> label:<input type='text' name='par11' size=8>\n";
  echo "<input type='submit' name='act' value='sortartist'> label:<input type='text' name='par12' size=8>\n";
  echo "<input type='submit' name='act' value='sharedoff'> label:<input type='text' name='par13' size=8>\n";
  echo "<input type='submit' name='act' value='sharedon'> label:<input type='text' name='par14' size=8>\n";
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
