<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"]; @$liv=$_GET["liv"]; @$idin=$_GET["idin"]; @$plin=$_GET["pl"]; 
@$pla=$_GET["pla"]; @$go=$_GET["go"]; @$act=$_GET["act"]; @$posin=(int)$_GET["pos"];

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

echo "<a href='?liv=1&pwdmd5=$pwdmd5&go=NAV'>NAV</a>";
echo "<a href='?pwdmd5=$pwdmd5&go=LST'>LIST</a>";
echo "<hr>";

if($go=="")$go="NAV";
switch($go){
  
  // navigation
  case "NAV":
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
  echo "<pre>$first liv:$liv, idin:$idin idprev:$idprev <a href='?liv=$prevliv&idin=$idprev&pwdmd5=$pwdmd5&go=NAV'>Prev</a>\n";
  $query=mysqli_query($con,"select id,name from $db where parent='$idin' order by name");
  for(;;){
    $row=mysqli_fetch_assoc($query);
    if($row==null)break;
    $id=$row["id"];
    $name=$row["name"];
    if($liv<3)echo "<a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&go=NAV'>$name</a>\n";
    else {
      echo "$name";
      for($i=0;$i<$ipl;$i++){
        $apl=$pl[$i];
        $query1=mysqli_query($con,"select position from playlist where label='$apl' and pwdmd5='$pwdmd5' and id='$id'");
        $row1=mysqli_fetch_assoc($query1);
        $position=(int)$row1["position"];
        mysqli_free_result($query1);
        if($position==0)echo " <a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&pl=$apl&pla=1&go=NAV'>+$apl</a>";
        else echo " <a href='?liv=$nextliv&idin=$id&pwdmd5=$pwdmd5&pl=$apl&pla=2&go=NAV'>-$apl</a>";
      }
      echo "\n";
    }
  }
  echo "</pre>";
  mysqli_free_result($query);
  break;

  // action on playlist
  case "LST":
  for($i=0;$i<$ipl;$i++)echo "<a href='?pl=$pl&pwdmd5=$pwdmd5&go=LST'>$pl[$i] $description[$i]</a>\n";
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
    $query1=mysqli_query($con,"select name,parent from music where id='$parent'");
    $row1=mysqli_fetch_assoc($query1);
    $liv1=$row1["name"];
    $parent=$row1["parent"];
    mysqli_free_result($query1);
    echo "<a href=?act=C&pl=$plin&pwdmd5=$pwdmd5&pos=$position&go=LST>C</a> ";
    echo "<a href=?act=U&pl=$plin&pwdmd5=$pwdmd5&pos=$position&go=LST>U</a> ";
    echo "<a href=?act=D&pl=$plin&pwdmd5=$pwdmd5&pos=$position&go=LST>D</a> ";
    echo " $position | $id | $name | $liv2 | $liv1\n";
  }
  echo "<pre>";
  mysqli_free_result($query);
  break;

}
mysqli_close($con);
?>
