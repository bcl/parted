#!/bin/sh
# Test exception handling on a bad SUN disklabel

. "${srcdir=.}/init.sh"; path_prepend_ ../parted $srcdir
ss=$sector_size_

n_sectors=2000 # number of sectors
dev=sun-disk-file
# create an empty file as a test disk
dd if=/dev/zero of=$dev bs=$ss count=$n_sectors 2> /dev/null || fail=1

# label the test disk as a sun disk
parted -s $dev mklabel sun > out 2>&1 || fail=1
compare /dev/null out || fail=1

# Mangle the disklabel to have incorrect CHS values, but a valid checksum
sun-badlabel $dev || fail=1

# Check the output (this should return 1, but depend on checking the output for the test)
parted -m -s $dev p > out 2>&1
grep unknown out || { cat out; fail=1; }

Exit $fail
