#!/bin/sh
# Test creating 1s partitions in 1s free space

# Copyright (C) 2022-2023 Free Software Foundation, Inc.

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

# create msdos label and some partitions with 1s free space between
parted --script "$dev" mklabel gpt > out 2>&1 || fail=1
parted --script "$dev" mkpart p1 ext4 64s 128s > out 2>&1 || fail=1
parted --script "$dev" mkpart p2 ext4 130s 200s > out 2>&1 || fail=1
parted --script "$dev" u s p free

# Free space is at 129s
parted --script "$dev" mkpart p3 ext4 129s 129s > out 2>&1 || fail=1
parted --script "$dev" u s p free

Exit $fail
