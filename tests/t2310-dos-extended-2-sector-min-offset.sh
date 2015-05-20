#!/bin/sh
# Ensure that parted allows a single sector between the beginning
# of an extended partition and the first logical partition.

# Copyright (C) 2010-2012 Free Software Foundation, Inc.

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

require_root_
require_scsi_debug_module_

# create memory-backed device
scsi_debug_setup_ dev_size_mb=1 > dev-name ||
  skip_ 'failed to create scsi_debug device'
scsi_dev=$(cat dev-name)
p1=${scsi_dev}1
p5=${scsi_dev}5

cat <<EOF > exp || framework_failure
BYT;
$scsi_dev:2048s:scsi:512:512:msdos:Linux scsi_debug:;
1:64s:128s:65s:::lba;
5:65s:128s:64s:::;
EOF

cat <<EOF > err.exp || framework_failure
Error: Error informing the kernel about modifications to partition $p5 -- Device or resource busy.  This means Linux won't know about any changes you made to $p5 until you reboot -- so you shouldn't mount it or use it in any way before rebooting.
Error: Failed to add partition 5 (Device or resource busy)
EOF

# Create a DOS label with an extended partition starting at sector 64.
parted -s $scsi_dev mklabel msdos || fail=1
parted --align=min -s $scsi_dev mkpart extended 64s 128s> out 2>&1 || fail=1
parted -m -s $scsi_dev u s print
compare /dev/null out || fail=1

# Trying to create a partition that starts just
# one sector after the start of the extended partition.
parted --align=min -s $scsi_dev mkpart logical 65s 128s > out 2>&1 || fail=1
compare /dev/null out || fail=1

parted -m -s $scsi_dev u s print > out 2>&1
compare exp out || fail=1

Exit $fail
