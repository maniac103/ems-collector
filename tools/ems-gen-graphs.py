#!/usr/bin/python
# -*- coding: utf-8 -*-
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

def do_graphdata(sensor, filename):
    datafile = open(filename, "w")
    process = subprocess.Popen(["mysql", "-uroot", "-ppass", "ems_data" ], shell = False,
                               stdin = subprocess.PIPE, stdout = datafile)
    process.communicate("""
        select time, value from numeric_data
        where sensor = %d and time >= adddate(now(), interval -1 %s)
        """ % (sensor, interval))
    datafile.close()

def do_plot(name, filename, definitions):
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

    gnuplotfile = open("/tmp/gnuplot.scr", "w")
    gnuplotfile.write("""
        set terminal png font 'times' 16 size 1000,600
        set grid lc rgb '#aaaaaa' lt 1 lw 0,5
        set title '%s'
        set xdata time
        set xlabel "Datum"
        set timefmt "%%Y-%%m-%%d %%H:%%M:%%S"
        set format x "%s"
        set xtics autofreq
        set ytics autofreq
        set output '%s'
        \n
        plot""" % (name, timeformat, os.path.join(targetpath, filename)))
    for i in range(1, len(definitions) + 1):
        definition = definitions[i - 1]
        gnuplotfile.write(" '/tmp/file%d.dat' using 1:3 with %s lw 2 title '%s'" %
                          (i, definition[2], definition[1]))
        if i != len(definitions):
            gnuplotfile.write(",")
    gnuplotfile.close()
    os.system("gnuplot /tmp/gnuplot.scr")

definitions = [ [ 11, "Aussentemperatur", "lines smooth bezier" ],
                [ 12, "Ged. Aussentemperatur", "lines" ] ]
do_plot("Aussentemperatur", "aussentemp", definitions)

definitions = [ [ 13, "Raum-Soll", "lines" ],
                [ 14, "Raum-Ist", "lines smooth bezier" ] ]
do_plot("Raumtemperatur", "raumtemp", definitions)

definitions = [ [ 1, "Kessel-Soll", "lines" ],
                [ 2, "Kessel-Ist", "lines smooth bezier" ],
                [ 6, "Vorlauf HK1", "lines smooth bezier" ],
                [ 8, "Vorlauf HK2", "lines smooth bezier" ],
                [ 10, "Ruecklauf", "lines smooth bezier" ] ]
do_plot("Temperaturen", "kessel", definitions)

definitions = [ [ 3, "Solltemperatur", "lines" ],
                [ 4, "Isttemperatur", "lines smooth bezier" ] ]
do_plot("Warmwasser", "ww", definitions)
