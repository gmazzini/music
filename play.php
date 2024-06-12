<?php
include "local.php";
$con=mysqli_connect($dbhost,$dbuser,$dbpassword,$dbname);
@$passwd=$_POST["passwd"]; @$plin=$_GET["pl"];

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
echo "<pre>$first\n";
$query=mysqli_query($con,"select label from playlist_desc where pwdmd5='$pwdmd5'");
for(;;){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $pl=$row["label"];
  echo "<a href='play.php?pl=$pl&pwdmd5=$pwdmd5'>$pl</a>\n";
}
mysqli_free_result($query);
echo "<hr>";

// play
$query=mysqli_query($con,"select id,position from playlist where pwdmd5='$pwdmd5' and label='$plin' order by position");
for($i=0;;$i++){
  $row=mysqli_fetch_assoc($query);
  if($row==null)break;
  $id[$i]=$row["id"];
  $query1=mysqli_query($con,"select name,parent from song where id='$id[$i]'");
  $row1=mysqli_fetch_assoc($query1);
  $data[$i]=$i." | ".$row1["name"];
  $parent=$row1["parent"];
  mysqli_free_result($query1);
  $query1=mysqli_query($con,"select name,parent from music where id='$parent'");
  $row1=mysqli_fetch_assoc($query1);
  $data[$i].=" | ".$row1["name"];
  $parent=$row1["parent"];
  mysqli_free_result($query1);
  $query1=mysqli_query($con,"select name from music where id='$parent'");
  $row1=mysqli_fetch_assoc($query1);
  $data[$i].=" | ".$row1["name"];
  mysqli_free_result($query1);
}
mysqli_free_result($query);

// player
?>
<span id="mydesc"></span>
<audio autoplay controls id="Player" src="load.php?id=<?php echo $id[0];?>" onclick="this.paused ? this.play() : this.pause();">Nooo</audio>
<script>
var src=[<?php for($j=0;$j<$i;$j++){if($j>0)echo ",";echo "\"load.php?id=$id[$j]\"";}?>]
var desc=[<?php for($j=0;$j<$i;$j++){if($j>0)echo ",";echo "\"$data[$j]\"";}?>]
var elm=0;
document.getElementById("mydesc").textContent=desc[elm];
var Player=document.getElementById("Player");
Player.onended=function(){
  elm++;
  if(elm < src.length){
    Player.src=src[elm];
    document.getElementById("mydesc").textContent=desc[elm];
    Player.play();
  }
}
function next(){
  elm++;
  if(elm < src.length){
    Player.src=src[elm];
    document.getElementById("mydesc").textContent=desc[elm];
    Player.play();
  }
}
</script>
<button onclick="next()">next</button><button onclick="prev()">prev</button>

<?php
echo "<pre>";
mysqli_close($con);
?>
