<?php

function format_timestamp($timestamp, $short) {
  if ($short) {
    $format = "%d.%m.%Y %H:%M:%S";
  } else {
    $format = "%A, %d. %B %Y, %H:%M:%S";
  }
  return strftime($format, $timestamp);
}

function set_loc_settings() {
    setlocale(LC_ALL, 'de_DE');
    date_default_timezone_set('Europe/Berlin');
}

function print_min_max_table($name, $data) {
  print "<table border=1 cellspacing=3 cellpadding=2 width=\"100%\"><tr>\n";
  print "<td colspan=2 style=\"background-color: rgb(102,153,204);\" height=21>\n";
  print "<p><b><span style=\"font-size: medium; color: rgb(255,255,255);\">" . $name . "</span></b></p>\n";
  print "</td></tr><tr>\n";
  print "<td width=147 height=18><p>Minimum</p></td>\n";
  print "<td width=300 align=center><p>" . $data["min"] . " (" .
        format_timestamp($data["min_time"], TRUE) . ")</p></td>\n";
  print "</tr><tr>\n";
  print "<td width=147 height=18><p>Maximum</p></td>\n";
  print "<td width=300 align=center><p>" . $data["max"] . " (" .
        format_timestamp($data["max_time"], TRUE) . ")</p></td>\n";
  print "</tr><tr>\n";
  print "<td width=147 height=18><p>Durchschnitt</p></td>\n";
  print "<td width=300 align=center><p>" . $data["avg"] . "</p></td>\n";
  print "</tr></table>\n";
}

?>
