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
  case "MNG": include "run_manage.php"; break;
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
