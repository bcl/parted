#!/bin/sh
# Exercise partition flags.

# Copyright (C) 2010-2014 Free Software Foundation, Inc.

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

. "${srcdir=.}/init.sh"; path_prepend_ ../parted .
ss=$sector_size_
dev=dev-file

extract_flags()
{
  perl -nle '/^1:2048s:4095s:2048s::(?:PTNNAME)?:(.+);$/ and print $1' "$@"
}

for table_type in bsd gpt msdos; do

  case $table_type in
    bsd)   primary_or_name=''
           ;;
    gpt)   primary_or_name='PTNNAME'
           ;;
    msdos) primary_or_name='primary'
           ;;
  esac

  n_sectors=5000
  dd if=/dev/null of=$dev bs=$ss seek=$n_sectors || fail=1

  parted -s $dev mklabel $table_type \
    mkpart $primary_or_name ext2 $((1*2048))s $((2*2048-1))s \
      > out 2> err || fail=1
  compare /dev/null out || fail=1

  # Query libparted for the available flags for this test partition.
  flags=`print-flags $dev` \
    || { warn_ "$ME_: $table_type: failed to get available flags";
         fail=1; continue; }

  for mode in on_only on_and_off ; do
    for flag in $flags; do
      # Turn on each flag, one at a time.
      parted -m -s $dev set 1 $flag on unit s print > raw 2> err || fail=1
      extract_flags raw > out
      grep -w "$flag" out \
        || { warn_ "$ME_: $table_type: flag '$flag' not turned on: $(cat out)"; fail=1; }
      compare /dev/null err || fail=1

      if test $mode = on_and_off; then
        # Turn it off
        parted -m -s $dev set 1 $flag off unit s print > raw 2> err || fail=1
        extract_flags raw > out
        grep -w "$flag" out \
          && { warn_ "$ME_: $table_type: flag '$flag' not turned off: $(cat out)"; fail=1; }
        compare /dev/null err || fail=1
      fi
    done
  done
done

Exit $fail
