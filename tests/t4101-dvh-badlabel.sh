#!/bin/sh
# Test exception handling on a bad DVH disklabel

. "${srcdir=.}/init.sh"; path_prepend_ ../parted $srcdir
ss=$sector_size_

n_sectors=2000 # number of sectors
dev=sun-disk-file
# create an empty file as a test disk
dd if=/dev/zero of=$dev bs=$ss count=$n_sectors 2> /dev/null || fail=1

# label the test disk as a dvh disk
parted -s $dev mklabel dvh > out 2>&1 || fail=1
compare /dev/null out || fail=1

# Mangle the disklabel to have incorrect checksum
dd if=/dev/zero of=$dev conv=notrunc bs=4 count=1 seek=1

# Check the output (this should return 1, but depend on checking the output for the test)
parted -m -s $dev p > out 2>&1
grep unknown out || { cat out; fail=1; }

Exit $fail
