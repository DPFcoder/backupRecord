#!/bin/bash

#´´½¨¹ÒÔØÄ¿Â¼
[ ! -e /root/backDir/ ] && mkdir /root/backDir/

curPath="/opt/bstar/backupRecord"
cd $curPath

#MOUNTCMD="mount -t cifs //192.168.110.58/Data_2 -o user=Administrator,passwd=test-123 /root/backDir/"
#IF_MOUNT=`df -h |grep backDir`
#if [ ! -n "$IF_MOUNT" ]
#        then $MOUNTCMD
#fi

export LD_LIBRARY_PATH=$curPath/lib:$LD_LIBRARY_PATH
echo 6553600 >/proc/sys/kernel/msgmnb 

nohup $curPath/backupRecord   >> /dev/null 2>&1 &
sleep 1
echo "$curPath backupRecord  is running."
echo ""
ps ax | grep backupRecord  | grep -v grep
