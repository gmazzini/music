<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"]; @$liv=$_GET["liv"]; @$idin=$_GET["idin"]; @$plin=$_GET["pl"]; @$pla=$_GET["pla"];

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





// navigation
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
  // playlist add or remove
  if($pla==1){
    $query=mysqli_query($con,"select max(position) from playlist where label='$plin' and pwdmd5='$pwdmd5'");
    $row=mysqli_fetch_row($query);
    $pllast=1+(int)$row[0];
    mysqli_free_result($query);
    mysqli_query($con,"insert into playlist (pwdmd5,id,position,label) values ('$pwdmd5','$idin',$pllast,'$plin')");
  }
  elseif($pla==2)mysqli_query($con,"delete from playlist where label='$plin' and pwdmd5='$pwdmd5' and id='$idin'");
  // back
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

echo "<pre>$first liv:$liv, idin:$idin idprev:$idprev <a href='show.php?liv=$prevliv&idin=$idprev&pwdmd5=$pwdmd5'>Prev</a>\n";
$query=mysqli_query($con,"select id,name from $db where parent='$idin' order by name");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id=$row["id"];
  $name=$row["name"];
  if($liv<3)echo "<a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5'>$name</a>\n";
  else {
    echo "$name";
    for($i=0;$i<$ipl;$i++){
      $apl=$pl[$i];
      $query1=mysqli_query($con,"select position from playlist where label='$apl' and pwdmd5='$pwdmd5' and id='$id'");
      $row1=mysqli_fetch_assoc($query1);
      $position=(int)$row1["position"];
      mysqli_free_result($query1);
      if($position==0)echo " <a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&pl=$apl&pla=1'>+$apl</a>";
      else echo " <a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&pl=$apl&pla=2'>-$apl</a>";
    }
    echo "\n";
  }
}
echo "</pre>";
mysqli_free_result($query);
mysqli_close($con);
?>
