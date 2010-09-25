<?php

include 'constants.php.inc';

function open_db() {
  return new PDO('mysql:dbname=ems_data;host=localhost', 'root', 'pass');
}

function format_value($row) {
  $value = (float) $row->value;
  if ($row->reading_type == ReadingTypeTime && $row->unit == "h") {
    $hours = (int) floor($value);
    $mins = (int) round(60 * ($value - $hours));
    if ($hours > 0) {
      $value = sprintf("%dh %dmin", $hours, $mins);
    } else {
      $value = $mins . "min";
    }
  } else {
    $precision = ($row->precision == NULL) ? 0 : (int) $row->precision;
    $value = sprintf("%." . $precision . "f", $value) . $row->unit;
  }
  return $value;
}

function get_current_sensor_values() {
  $connection = open_db();

  $query = "select s.type, s.reading_type, s.precision, v.value, s.unit from sensors s
            inner join (select sensor, max(time) maxtime
            from TABLE group by sensor) maxtimes
            on s.type = maxtimes.sensor
            inner join TABLE v
            on maxtimes.sensor = v.sensor and maxtimes.maxtime = v.time;";

  $values = array();

  $numeric = $connection->query(str_replace("TABLE", "numeric_data", $query));
  $numeric->setFetchMode(PDO::FETCH_OBJ);
  foreach ($numeric as $row) {
    $type = (int) $row->type;
    $values[$type] = format_value($row);
  }

  $boolean = $connection->query(str_replace("TABLE", "boolean_data", $query));
  $boolean->setFetchMode(PDO::FETCH_OBJ);
  foreach ($boolean as $row) {
    $type = (int) $row->type;
    $value = (boolean) $row->value;
    $values[$type] = $value;
  }

  return $values;
}

function get_min_max($sensor, $time_clause) {
  $connection = open_db();

  $query = "select s.reading_type, s.precision, unix_timestamp(v.time) time, v.value, s.unit from sensors s
            inner join (select sensor, time, value from numeric_data
            where sensor = " . $sensor . " and " . $time_clause . "
            order by value DIRECTION limit 1) v
            on s.type = v.sensor;";

  $avg_query = "select s.reading_type, s.precision, v.value, s.unit from sensors s
                inner join (select sensor, avg(value) value from numeric_data
                where sensor = " . $sensor . " and " . $time_clause . ") v
                on s.type = v.sensor;";

  $min = $connection->query(str_replace("DIRECTION", "asc", $query))->fetch(PDO::FETCH_OBJ);
  $max = $connection->query(str_replace("DIRECTION", "desc", $query))->fetch(PDO::FETCH_OBJ);
  $avg = $connection->query($avg_query)->fetch(PDO::FETCH_OBJ);

  $retval = array();
  $retval["min_time"] = $min->time;
  $retval["min"] = format_value($min);
  $retval["max_time"] = $max->time;
  $retval["max"] = format_value($max);
  $retval["avg"] = format_value($avg);

  return $retval;
}

function get_min_max_interval($sensor, $interval) {
  $time_clause = "time >= adddate(now(), interval -1 " . $interval . ")";
  return get_min_max($sensor, $time_clause);
}

function get_min_max_for_day($sensor, $days_ago) {
  if ($days_ago == 0) {
    $time_clause = "time >= curdate()";
  } else {
    $time_clause = "time >= date_sub(curdate(), interval " . $days_ago . " day) and " .
                   "time < date_sub(curdate(), interval " . ($days_ago - 1) . " day)";
  }
  return get_min_max($sensor, $time_clause);
}

function get_sensor_changes_for_day($days_ago) {
  $connection = open_db();

  $upper = ($days_ago > 0) ? " where time < date_sub(curdate(), interval " . ($days_ago - 1) . " day)" : "";
  $lower = " where time < date_sub(curdate(), interval " . $days_ago . " day)";
  $query = "select s.type, s.reading_type, s.precision, (upper.value - lower.value) value, s.unit from sensors s
            inner join (select v.sensor, v.value from numeric_data v
                        inner join (select sensor, max(time) maxtime from numeric_data" . $upper . " group by sensor) uppertimes
                        on v.sensor = uppertimes.sensor and v.time = uppertimes.maxtime) upper
            on upper.sensor = s.type
            inner join (select v.sensor, v.value from numeric_data v
                        inner join (select sensor, max(time) maxtime from numeric_data" . $lower . " group by sensor) lowertimes
                        on v.sensor = lowertimes.sensor and v.time = lowertimes.maxtime) lower
            on lower.sensor = s.type;";

  $values = array();
  $results = $connection->query($query);
  $results->setFetchMode(PDO::FETCH_OBJ);
  foreach ($results as $row) {
    $type = (int) $row->type;
    $values[$type] = format_value($row);
  }

  return $values;
}

?>
