#!/bin/bash


killall backupRecord

echo "/opt/bstar/backupRecord/backupRecord is over."

sleep 1
ps ax | grep backupRecord | grep -v grep
echo ""

