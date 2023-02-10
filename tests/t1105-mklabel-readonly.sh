#!/bin/sh
# Test printing the partition table in readonly mode

# Copyright (C) 2023 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

. "${srcdir=.}/init.sh"; path_prepend_ ../parted

dev=loop-file

# create device
truncate --size 10MiB "$dev" || fail=1

parted --readonly --script "$dev" mklabel msdos > out 2>&1; test $? = 1 || fail=1

# create expected output file
echo "Error: Can't write to $PWD/$dev, because it is opened read-only." > exp
compare exp out || fail=1


Exit $fail
