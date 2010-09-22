<?php

include 'constants.php.inc';

$connection = mysql_connect("localhost", "root", "pass");
mysql_select_db("ems_data");

$query = "select s.type, s.reading_type, s.precision, v.value, s.unit from sensors s
          inner join (select sensor, max(time) maxtime
          from TABLE group by sensor) maxtimes
          on s.type = maxtimes.sensor
          inner join TABLE v
          on maxtimes.sensor = v.sensor and maxtimes.maxtime = v.time;";

$sensors = array();

$numeric = mysql_query(str_replace("TABLE", "numeric_data", $query));
while ($row = mysql_fetch_object($numeric)) {
  $type = (int) $row->type;
  $value = (float) $row->value;
  if ($row->reading_type == ReadingTypeTime && $row->unit == "h") {
    $hours = (int) floor($value);
    $mins = (int) round(60 * ($value - $hours));
    $value = sprintf("%dh %dmin", $hours, $mins);
  } else {
    $precision = ($row->precision == NULL) ? 0 : (int) $row->precision;
    $value = number_format($value, $precision) . $row->unit;
  }
  $sensors[$type] = $value;
}

$boolean = mysql_query(str_replace("TABLE", "boolean_data", $query));
while ($row = mysql_fetch_object($boolean)) {
    $type = (int) $row->type;
    $value = (boolean) $row->value;
    $sensors[$type] = $value;
}

mysql_close($connection);
?>
