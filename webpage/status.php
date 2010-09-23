<html>

<head>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <title>Heizung</title>

  <link href="style.css" type="text/css" rel="stylesheet"/>

  <script Language="JavaScript">
    function update() {
      window.location.reload();
      setTimeout("update()", 30000);
    }

    window.onload = function () {
      setTimeout("update()", 30000);
    }
  </script>
</head>

<?php
include 'sensor_utils.php.inc';
include 'utils.php.inc';

set_loc_settings();

$sensors = get_current_sensor_values();

function print_header($name) {
  print "<table border=1 cellspacing=3 cellpadding=2 width=\"100%\">\n";
  print "<tr><td colspan=2 style=\"background-color: rgb(102,153,204);\" height=21>\n";
  print "<p><b><span style=\"font-size: medium; color: rgb(255,255,255);\">" . $name . "</span></b></p>\n";
  print "</td></tr>\n";
}

function print_cell($name, $value, $color = "") {
  print "<tr>\n";
  if ($color == "green") {
    $color = "style=\"background-color: rgb(0,200,0); color: rgb(255,255,255);\"";
  } else if ($color == "red") {
    $color = "style=\"background-color: rgb(200,0,0); color: rgb(255,255,255);\"";
  }
  print "<td width=147 height=18><p>" . $name . "</p></td>\n";
  print "<td width=129 align=center><p><span " . $color . ">" . $value . "</span></p></td>\n";
  print "</tr>\n";
}
?>

<body topmargin=0 leftmargin=0 marginwidth=0 marginheight=0>
  <table border=0 cellspacing=0 cellpadding=0 style="width:800; text-align:center;">
    <tr><td width="100%">
      <h2><?php print "Momentaner Status (" . format_timestamp(time(), TRUE) . ")"; ?></h2>
    </td></tr>
    <tr><td>
      <?php include "menu.inc"; ?>
    </td></tr>
    <tr height=10></tr>
    <tr><td>
      <table>
        <tr valign="top">
          <td width=390>
            <?php
              print_header("Heizung");
              print_cell("Kessel IST", $sensors[SensorKesselIstTemp]);
              print_cell("Kessel SOLL", $sensors[SensorKesselSollTemp]);
              $value = $sensors[SensorHKPumpe] && !$ensors[SensorHKWW];
              print_cell("Vorlaufpumpe", $value ? "- an -" : "- aus -", $value ? "green" : "");
              $value = $sensors[SensorBrenner];
              print_cell("Brenner",
                         $value ? ($sensors[SensorWarmwasserbereitung] ? "WW-Bereitung" : "Heizen") : "- aus -",
                         $value ? "red" : "");
              print_cell("Betriebsart", $sensors[SensorAutomatikbetrieb] ? "Automatik" : "Manuell");
              print_cell("Tag/Nachtbetrieb", $sensors[SensorTagbetrieb] ? "Tag" : "Nacht");
              print_cell("Sommerbetrieb", $sensors[SensorSommerbetrieb] ? "aktiv" : "inaktiv");
            ?>
            </table>
          </td>
          <td width=20></td>
          <td width=390>
            <?php
              print_header("Warmwasser");
              print_cell("Warmwasser IST", $sensors[SensorWarmwasserIstTemp]);
              print_cell("Warmwasser SOLL", $sensors[SensorWarmwasserSollTemp]);
              $value = $sensors[SensorHKPumpe] && $sensors[SensorHKWW];
              print_cell("WW-Pumpe", $value ? "- an -" : "- aus -", $value ? "green" : "");
              $value = $sensors[SensorZirkulation];
              print_cell("Zirkulationspumpe", $value ? "- an -" : "- aus -", $value ? "green" : "");
              print_cell("WW-Vorrang", $sensors[SensorWWVorrang] ? "- an -" : "- aus -");
            ?>
            </table>
          </td>
        </tr>
        <tr height=6></tr>
        <tr valign="top">
          <td>
            <?php
              print_header("Sonstige Temperaturen");
              print_cell("Außen", $sensors[SensorAussenTemp]);
              print_cell("Außen gedämpft", $sensors[SensorGedaempfteAussenTemp]);
              print_cell("Raumtemp. IST", $sensors[SensorRaumIstTemp]);
              print_cell("Raumtemp. SOLL", $sensors[SensorRaumSollTemp]);
            ?>
            </table>
          </td>
          <td width=20></td>
          <td>
            <?php
              print_header("Betriebsstatus");
              print_cell("Brennerlaufzeit", $sensors[SensorBetriebszeit]);
              print_cell("Brennerstarts", $sensors[SensorBrennerstarts]);
              print_cell("Systemdruck", $sensors[SensorSystemdruck]);
              # TODO: Fehler
            ?>
            </table>
          </td>
        </tr>
      </table>
    </td></tr>
  </table>
</body>

</html>

