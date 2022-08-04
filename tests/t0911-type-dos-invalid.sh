#!/bin/sh

# Test type command with MS-DOS label

# Copyright (C) 2022 SUSE LLC

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
require_512_byte_sector_size_

dev=loop-file

# create device
truncate --size 50MiB "$dev" || fail=1

# create msdos label and one partition
parted --script "$dev" mklabel msdos > out 2>&1 || fail=1
parted --script "$dev" mkpart primary "linux-swap" 10% 20% > out 2>&1 || fail=1

# set type-id
parted --script "$dev" type 1 "0x101" && fail=1

Exit $fail
