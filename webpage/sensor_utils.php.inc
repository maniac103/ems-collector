<?php

include 'constants.php.inc';

function open_db() {
  return new PDO('mysql:dbname=ems_data;host=localhost', 'emsdata', 'emsdata');
}

function format_value($row) {
  $value = (float) $row->value;
  if ($row->reading_type == ReadingTypeTime && $row->unit == "min") {
    $hours = (int) floor($value / 60);
    $mins = (int) ($value - 60 * $hours);
    if ($hours > 0) {
      return sprintf("%dh %dmin", $hours, $mins);
    }
  }

  $precision = ($row->precision == NULL) ? 0 : (int) $row->precision;
  return sprintf("%." . $precision . "f", $value) . $row->unit;
}

function get_current_sensor_values() {
  $connection = open_db();

  $query = "select s.type, s.reading_type, s.precision, v.value, s.unit from sensors s
            inner join (select sensor, max(endtime) maxtime
            from TABLE group by sensor) maxtimes
            on s.type = maxtimes.sensor
            inner join TABLE v
            on maxtimes.sensor = v.sensor and maxtimes.maxtime = v.endtime;";

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

  $state = $connection->query(str_replace("TABLE", "state_data", $query));
  $state->setFetchMode(PDO::FETCH_OBJ);
  foreach ($state as $row) {
    $type = (int) $row->type;
    $values[$type] = $row->value;
  }

  return $values;
}

function get_min_max($sensor, $start_clause, $end_clause) {
    $connection = open_db();
    $connection->exec("set @starttime = " . $start_clause . ";");
    $connection->exec("set @endtime = " . $end_clause . ";");

    $query = "select s.reading_type, s.precision, unix_timestamp(v.time) time, v.value, s.unit from sensors s
              inner join (select sensor, if(endtime > @endtime, @endtime, endtime) time, value from numeric_data
                          where sensor = " . $sensor . " and starttime < @endtime and endtime >= @starttime
                          order by value DIRECTION limit 1) v
              on s.type = v.sensor;";
    $avg_query = "select s.reading_type, s.precision, v.value, s.unit from sensors s
                  inner join (select sensor, sum(time * value) / sum(time) value from (
                              select sensor, value, timediff(endtime, starttime) time from (
                              select sensor, value,
                                     if(starttime < @starttime, @starttime, starttime) starttime,
                                     if(endtime > @endtime, @endtime, endtime) endtime from numeric_data
                              where sensor = " . $sensor . " and starttime < @endtime and endtime >= @starttime) t1) t2) v
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
  $start = "subdate(now(), interval " . $interval . ")";
  $end = "now()";
  return get_min_max($sensor, $start, $end);
}

function get_min_max_for_day($sensor, $days_ago) {
  if ($days_ago == 0) {
    $start = "curdate()";
    $end = "now()";
  } else {
    $start = "subdate(curdate(), interval " . $days_ago . " day)";
    $end = "subdate(curdate(), interval " . ($days_ago - 1) . " day)";
  }
  return get_min_max($sensor, $start, $end);
}

function get_sensor_changes_for_day($days_ago) {
  $connection = open_db();

  $upper = ($days_ago > 0) ? " where endtime < subdate(curdate(), interval " . ($days_ago - 1) . " day)" : "";
  $lower = " where endtime < subdate(curdate(), interval " . $days_ago . " day)";
  $query = "select s.type, s.reading_type, s.precision, (upper.value - lower.value) value, s.unit from sensors s
            inner join (select v.sensor, v.value from numeric_data v
                        inner join (select sensor, max(endtime) maxtime from numeric_data" . $upper . " group by sensor) uppertimes
                        on v.sensor = uppertimes.sensor and v.endtime = uppertimes.maxtime) upper
            on upper.sensor = s.type
            inner join (select v.sensor, v.value from numeric_data v
                        inner join (select sensor, max(endtime) maxtime from numeric_data" . $lower . " group by sensor) lowertimes
                        on v.sensor = lowertimes.sensor and v.endtime = lowertimes.maxtime) lower
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
