ems-collector
=============

Buderus EMS heating control data collection daemon

ems-collector logs serial data from a connected Buderus heating
by interpreting the incoming data and writing it into a mysql database. It
also provides a telnet-like interface for sending commands to the heating
and receiving a stream of incoming data when being used with a TCP-to-EMS
interface (e.g. NetIO with ethersex running on top)

The ems data is described here:
http://ems-gateway.myds.me/dokuwiki/doku.php?id=wiki:ems:telegramme

Requirements
============
(examples for debian wheezy)

The following packages are needed for building the collector:
```
apt-get install libboost1.49-all-dev
apt-get install libmysql++-dev
```

For using the bundled (demo) web interface, gnuplot is needed:
```
apt-get install gnuplot
```

Clone git repository and compile
================================
```
git clone https://github.com/maniac103/ems-collector.git
cd ems-collector/collector
make
```

Install
=======
```
cp collectord /usr/local/sbin
cd ../tools
cp ems-collector.default /etc/default/ems-collector
cp ems-collector.init /etc/init.d/ems-collector
```

Create a new /etc/ems-collector.conf
====================================
```
nano /etc/ems-collector.conf
```

now, copy and paste this into the editor and customize to meet your environment:
```
ratelimit = 120
rc-type = rc30
db-user = <user>
db-pass = <password>
```
close and save. The room controller type can be passed as either rc30 or rc35.

Make it a service and go
========================
```
insserv ems-collector
service ems-collector start
```
