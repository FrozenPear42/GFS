#!/usr/bin/env bash

x=0
while [ ${x} -le $2 ] ; do
dd if=/dev/urandom of=dummy${x} bs=1 count=1
./FS add $1 dummy${x}
rm dummy${x}
x=$((x+1))
done
