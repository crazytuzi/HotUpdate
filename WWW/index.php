<?php
  ini_set("display_errors", "On");

  ini_set("error_reporting",E_ALL);

  header('Content-Type:application/json; charset=utf-8');

  $data = json_decode(file_get_contents('php://input'));

  $version = $data->{"version"};

  $platform = $data->{"platform"};

  $file = $version . "/" . $platform ."/". "version.json";

  if(file_exists($file))
  {
  	$json_contents = file_get_contents($file);

  	echo $json_contents;
  }
  else
  {
  	exit(json_encode(""));
  }
?>
