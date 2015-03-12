ems-collector
=============

Buderus EMS heating control data collection daemon

ems-collector logs serial data from a connected Buderus heating
by interpreting the incoming data and writing it into a mysql database.

The ems data is described here:
http://ems-gateway.myds.me/dokuwiki/doku.php?id=wiki:ems:telegramme

Requirements
============
(examples for debian wheezy)

The following packages are needed:
```
apt-get install libboost1.49-all-dev
apt-get install libmysql++-dev
apt-get install gnuplot
```

Clone git-Repository and compile
================================
```
git clone https://github.com/maniac103/ems-collector.git /usr/local/src/ems-collector
cd /usr/local/src/ems-collector/collector
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
db-path = localhost:3306
db-user = <user>
db-pass = <password>
```
close and save.

Make it a service and go
========================
```
insserv ems-collector
service ems-collector start
```
