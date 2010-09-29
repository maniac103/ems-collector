#!/usr/bin/python
# -*- coding: utf-8 -*-
import contextlib
import errno
import os
import subprocess
import sys
import time

if len(sys.argv) != 3:
    sys.exit(1)

targetpath = sys.argv[1]
interval = sys.argv[2]

if not os.path.isdir(targetpath):
    os.makedirs(targetpath)

if not interval in ["day", "week", "month"]:
    sys.exit(1)

@contextlib.contextmanager
def flock(path, wait_delay = 1):
    while True:
        try:
            fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_RDWR)
        except OSError, e:
            if e.errno != errno.EEXIST:
                raise
            time.sleep(wait_delay)
            continue
        else:
            break
    try:
        yield fd
    finally:
        os.close(fd)
        os.unlink(path)

def do_graphdata(sensor, filename):
    datafile = open(filename, "w")
    process = subprocess.Popen(["mysql", "-A", "-uroot", "-ppass", "ems_data" ], shell = False,
                               stdin = subprocess.PIPE, stdout = datafile)
    process.communicate("""
        set @starttime = subdate(now(), interval 1 %s);
        set @endtime = now();
        select time, value from (
            select adddate(if(starttime < @starttime, @starttime, starttime), interval 1 second) time, value from numeric_data
            where sensor = %d and endtime >= @starttime
            union all
            select if(endtime > @endtime, @endtime, endtime) time, value from numeric_data
            where sensor = %d and endtime >= @starttime)
        t1 order by time;
        """ % (interval, sensor, sensor))
    datafile.close()

def do_plot(name, filename, ylabel, definitions):
    i = 1
    for definition in definitions:
        do_graphdata(definition[0], "/tmp/file%d.dat" % i)
        i = i + 1

    filename = filename + "-" + interval + ".png"

    if interval == "day":
        timeformat = "%H:%M"
    elif interval == "week":
        timeformat = "%d.%m (%Hh)"
    else:
        timeformat = "%d.%m"

    process = subprocess.Popen("gnuplot", shell = False, stdin = subprocess.PIPE)
    process.stdin.write("set terminal png font 'arial' 12 size 800, 450\n")
    process.stdin.write("set grid lc rgb '#aaaaaa' lt 1 lw 0,5\n")
    process.stdin.write("set title '%s'\n" % name)
    process.stdin.write("set xdata time\n")
    process.stdin.write("set xlabel 'Datum'\n")
    process.stdin.write("set ylabel '%s'\n" % ylabel)
    process.stdin.write("set timefmt '%Y-%m-%d %H:%M:%S'\n")
    process.stdin.write("set format x '%s'\n" % timeformat)
    process.stdin.write("set xtics autofreq rotate by -45\n")
    process.stdin.write("set ytics autofreq\n")
    process.stdin.write("set output '%s'\n" % os.path.join(targetpath, filename))
    process.stdin.write("plot")
    for i in range(1, len(definitions) + 1):
        definition = definitions[i - 1]
        process.stdin.write(" '/tmp/file%d.dat' using 1:3 with %s lw 2 title '%s'" %
                           (i, definition[2], definition[1]))
        if i != len(definitions):
            process.stdin.write(",")
    process.stdin.write("\n")
    process.stdin.close()
    process.wait()

    for i in range(1, len(definitions) + 1) :
        os.remove("/tmp/file%d.dat" % i)

with flock("/tmp/graph-gen.lock"):
    definitions = [ [ 11, "Außentemperatur", "lines smooth bezier" ],
                    [ 12, "Ged. Außentemperatur", "lines" ] ]
    do_plot("Aussentemperatur", "aussentemp", "Temperatur (°C)", definitions)

    definitions = [ [ 13, "Raum-Soll", "lines" ],
                    [ 14, "Raum-Ist", "lines smooth bezier" ] ]
    do_plot("Raumtemperatur", "raumtemp", "Temperatur (°C)", definitions)

    definitions = [ [ 1, "Kessel-Soll", "lines" ],
                    [ 2, "Kessel-Ist", "lines smooth bezier" ],
                    [ 6, "Vorlauf HK1", "lines smooth bezier" ],
                    [ 8, "Vorlauf HK2", "lines smooth bezier" ],
                    [ 10, "Rücklauf", "lines smooth bezier" ] ]
    do_plot("Temperaturen", "kessel", "Temperatur (°C)", definitions)

    definitions = [ [ 3, "Solltemperatur", "lines" ],
                    [ 4, "Isttemperatur", "lines smooth bezier" ] ]
    do_plot("Warmwasser", "ww", "Temperatur (°C)", definitions)
