cp ../cmake-build-debug/FS FS
echo "Creating new filesystem in file 8096.fs"
echo
./FS create 8096.fs 8096
./FS status 8096.fs
./FS tree 8096.fs
echo
echo "Adding 40 1-byte files to filesytem"

x=0
while [ $x -le 39 ] ; do
dd if=/dev/urandom of=dummy$x bs=1 count=1 >> /dev/null
./FS add 8096.fs dummy$x
rm dummy$x
x=$((x+1))
done
echo
./FS status 8096.fs
./FS tree 8096.fs

echo "Removing some files"
echo
for x in 0 13 14 15 16 22 39 40
do
echo "Removing dummy$x"
./FS remove 8096.fs dummy$x
done
echo
./FS status 8096.fs
./FS tree 8096.fs

echo
echo "Adding 92-byte text file: test3"
./FS add 8096.fs test3

echo
./FS status 8096.fs
./FS tree 8096.fs

echo "Adding file with the same name: test3"
echo
./FS add 8096.fs test3

echo
./FS status 8096.fs
./FS tree 8096.fs

echo "Removing all dummies"
echo
x=0
while [ $x -le 39 ] ; do
./FS remove 8096.fs dummy$x
x=$((x+1))
done

echo
./FS status 8096.fs
./FS tree 8096.fs

echo "Removing test3"
echo

./FS remove 8096.fs test3

echo
./FS status 8096.fs
./FS tree 8096.fs


echo "Removing filesystem"
echo
./FS drop 8096.fs
./FS status 8096.fs

echo "Creating new filesystem in file 8096.fs"
echo
./FS create 8096.fs 8096
./FS status 8096.fs
./FS tree 8096.fs
echo
echo "Adding 8096-byte file"
dd if=/dev/urandom of=dummy8096 bs=1 count=8096 >> /dev/null
./FS add 8096.fs dummy8096
echo
rm dummy8096
./FS status 8096.fs
./FS tree 8096.fs
echo
echo "Adding 3 1-byte files to filesytem"
x=0
while [ $x -le 2 ] ; do
dd if=/dev/urandom of=dummy$x bs=1 count=1 >> /dev/null
./FS add 8096.fs dummy$x
rm dummy$x
x=$((x+1))
done
echo
./FS status 8096.fs
./FS tree 8096.fs

echo "Creating new filesystem in file 8096.fs"
echo
./FS create 8096.fs 8096
./FS status 8096.fs
./FS tree 8096.fs

echo
echo "Adding 3 32-byte files to filesytem"
x=0
while [ $x -le 2 ] ; do
dd if=/dev/urandom of=dummy$x bs=1 count=32 >> /dev/null
./FS add 8096.fs dummy$x
rm dummy$x
x=$((x+1))
done

echo
echo "Removing middle file"
./FS remove 8096.fs dummy1

echo
echo "Adding 92-byte text file: test3"
./FS add 8096.fs test3
cat test3
./FS status 8096.fs
./FS tree 8096.fs

echo
echo "Extracting test3 to test3.out"
./FS get 8096.fs test3 test3.out
md5sum test3
md5sum test3.out


cat  test3.out

echo "Creating new filesystem in file 80960.fs"
echo
./FS create 80960.fs 80960
./FS status 80960.fs
./FS tree 80960.fs

echo
echo "Adding 3 32-byte files to filesytem"
x=0
while [ $x -le 2 ] ; do
dd if=/dev/urandom of=dummy$x bs=1 count=32 >> /dev/null
./FS add 80960.fs dummy$x
rm dummy$x
x=$((x+1))
done

echo
echo "Removing middle file"
./FS remove 80960.fs dummy1

echo
echo "Adding 92-byte text file: test3"
./FS add 80960.fs test.png
./FS status 80960.fs
./FS tree 80960.fs

echo
echo "Extracting test.png to test.out.png"
./FS get 80960.fs test.png test.out.png
md5sum test.png
md5sum test.out.png


