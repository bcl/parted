#!/bin/sh
# Ensure parted changes GUID back to match its FS.

# Copyright (C) 2021-2022 Free Software Foundation, Inc.

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

grep '^#define USE_BLKID 1' "$CONFIG_HEADER" > /dev/null ||
  skip_ 'this system lacks a new-enough libblkid'

# What filesystem tools are present?
FSTYPES=""

# Is mkfs.hfsplus available?
mkfs.hfsplus 2>&1 | grep '^usage:' && FSTYPES="hfs+"

# Is mkfs.vfat available?
mkfs.vfat 2>&1 | grep '^Usage:' && FSTYPES="$FSTYPES fat32"

# Is mkswap available?
mkswap -V 2>&1 | grep '^mkswap' && FSTYPES="$FSTYPES linux-swap"

[ -n "$FSTYPES" ] || skip_ "No supported filesystem tools (vfat, hfs+, swap) installed"


# create memory-backed device
scsi_debug_setup_ dev_size_mb=25 > dev-name ||
  skip_ 'failed to create scsi_debug device'
scsi_dev=$(cat dev-name)

# Create a formatted partition.
# Set a different partition type on it, eg. lvm, then unset it.
# The partition flag should return to the detected filesystem type.

for fs_type in $FSTYPES; do
  echo "fs_type=$fs_type"


  parted -s $scsi_dev mklabel gpt mkpart first $fs_type 1MB 25MB > out 2>&1 || fail=1
  # expect no output
  compare /dev/null out || fail=1

  p1=${scsi_dev}1
  wait_for_dev_to_appear_ $p1 || fail=1

  case $fs_type in
    fat32) mkfs.vfat $p1 || fail=1 ;;
    hfs*) mkfs.hfsplus $p1 || fail=1;;
    linux-swap) mkswap $p1 || fail=1;;
    *) error "internal error: unhandled fs type: $fs_type";;
  esac

  # Confirm the filesystem and flags are as expected
  parted -s $scsi_dev u s p > out || fail=1
  case $fs_type in
    fat32) grep 'fat16.*msftdata$' out || { fail=1; cat out; } ;;
    hfs*) grep 'hfs+.*first$' out || { fail=1; cat out; } ;;
    linux-swap) grep 'linux-swap.*swap$' out || { fail=1; cat out; } ;;
    *) error "internal error: unhandled fs type: $fs_type";;
  esac

  # Set the lvm GUID on the partition
  parted -s $scsi_dev set 1 lvm on > out 2>&1 || fail=1
  # expect no output
  compare /dev/null out || fail=1

  # Confirm filesystem probe is the same, but flags are now lvm
  parted -s $scsi_dev u s p > out || fail=1
  case $fs_type in
    fat32) grep 'fat16.*lvm$' out || { fail=1; cat out; } ;;
    hfs*) grep 'hfs+.*lvm$' out || { fail=1; cat out; } ;;
    linux-swap) grep 'linux-swap.*lvm$' out || { fail=1; cat out; } ;;
    *) error "internal error: unhandled fs type: $fs_type";;
  esac

  # Unset the lvm GUID on both partitions
  parted -s $scsi_dev set 1 lvm off > out 2>&1 || fail=1
  # expect no output
  compare /dev/null out || fail=1

  # Confirm the filesystem and flags are as expected
  parted -s $scsi_dev u s p > out || fail=1
  case $fs_type in
    fat32) grep 'fat16.*msftdata$' out || { fail=1; cat out; } ;;
    hfs*) grep 'hfs+.*first$' out || { fail=1; cat out; } ;;
    linux-swap) grep 'linux-swap.*swap$' out || { fail=1; cat out; } ;;
    *) error "internal error: unhandled fs type: $fs_type";;
  esac
done

Exit $fail
