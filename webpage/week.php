<html>

<head>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <title>Heizung</title>

  <link href="style.css" type="text/css" rel="stylesheet"/>
</head>

<?php
include 'sensor_utils.php.inc';
include 'utils.php.inc';

set_loc_settings();

$aussentemp = get_min_max_interval(SensorAussenTemp, "week");
$raumtemp = get_min_max_interval(SensorRaumIstTemp, "week");
?>

<body topmargin=0 leftmargin=0 marginwidth=0 marginheight=0>
  <table style="width:800; text-align:center;">
    <tr><td>
      <h2>Letzte Woche</h2>
    </td></tr>
    <tr><td>
      <?php include 'menu.inc'; ?>
    </td></tr>
    <tr height=10></tr>
    <tr><td>
      <table border=0 cellspacing=0 cellpadding=0 width="100%">
        <tr><td>
          <?php print_min_max_table("Außentemperatur", $aussentemp); ?>
        </td></tr>
        <tr height=6></tr>
        <tr><td>
          <?php print_min_max_table("Raumtemperatur", $raumtemp); ?>
        </td></tr>
      </table>
    </td></tr>
  </table>
  <h3>Graphen</h3>
  <p>
    <img src="graphs/aussentemp-week.png" alt="Außentemperaturentwicklung">
  </p>
  <p>
    <img src="graphs/raumtemp-week.png" alt="Raumtemperaturentwicklung">
  </p>
  <p>
    <img src="graphs/kessel-week.png" alt="Kesseltemperaturentwicklung">
  </p>
  <p>
    <img src="graphs/ww-week.png" alt="Warmwassertemperaturentwicklung">
  </p>
</body>

</html>

