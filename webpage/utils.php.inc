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

?>
