#!/bin/sh
# WARNING: This file was auto-generated. Do not edit!

PATH=/var/snotify/bin:/bin:/usr/bin:/usr/local/bin:/usr/local/sbin
export PATH

exec </dev/null
exec >/dev/null
exec 2>/dev/null

svc -dx /var/snotify/service/* /var/snotify/service/*/log

env - PATH=$PATH svscan /var/snotify/service 2>&1 | \ 
env - PATH=$PATH readproctitle service errors: ..................................................................................
.................................................................................................................................
.................................................................................................................................
............................................................
