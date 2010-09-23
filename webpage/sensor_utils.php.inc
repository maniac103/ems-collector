<?php

include 'constants.php.inc';

function open_db() {
  $connection = mysql_connect("localhost", "root", "pass");
  mysql_select_db("ems_data");
  return $connection;
}

function format_value($row) {
  $value = (float) $row->value;
  if ($row->reading_type == ReadingTypeTime && $row->unit == "h") {
    $hours = (int) floor($value);
    $mins = (int) round(60 * ($value - $hours));
    $value = sprintf("%dh %dmin", $hours, $mins);
  } else {
    $precision = ($row->precision == NULL) ? 0 : (int) $row->precision;
    $value = number_format($value, $precision) . $row->unit;
  }
  return $value;
}

function get_current_sensor_values() {
  global $sensors;
  $connection = open_db();

  $query = "select s.type, s.reading_type, s.precision, v.value, s.unit from sensors s
            inner join (select sensor, max(time) maxtime
            from TABLE group by sensor) maxtimes
            on s.type = maxtimes.sensor
            inner join TABLE v
            on maxtimes.sensor = v.sensor and maxtimes.maxtime = v.time;";

  $values = array();

  $numeric = mysql_query(str_replace("TABLE", "numeric_data", $query));
  while ($row = mysql_fetch_object($numeric)) {
    $type = (int) $row->type;
    $values[$type] = format_value($row);
  }

  $boolean = mysql_query(str_replace("TABLE", "boolean_data", $query));
  while ($row = mysql_fetch_object($boolean)) {
    $type = (int) $row->type;
    $value = (boolean) $row->value;
    $values[$type] = $value;
  }

  mysql_close($connection);

  return $values;
}

function get_min_max($sensor, $interval) {
  $connection = open_db();

  $query = "select s.reading_type, s.precision, v.time, v.value, s.unit from sensors s
            inner join (select sensor, time, OPERATION(value) value from numeric_data
            where sensor = " . $sensor . " on s.type = v.sensor;";

  $min = mysql_fetch_object(mysql_query(str_replace("OPERATION", "min", $query)));
  $max = mysql_fetch_object(mysql_query(str_replace("OPERATION", "max", $query)));

  $retval = array();
  $retval["min_time"] = $min->time;
  $retval["min"] = format_value($min);
  $retval["max_time"] = $max->time;
  $retval["max"] = format_value($max);

  mysql_close($connection);

  return $retval;
}

?>
