# Customize maint.mk                           -*- makefile -*-
# Copyright (C) 2003-2014 Free Software Foundation, Inc.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Use the direct link.  This is guaranteed to work immediately, while
# it can take a while for the faster mirror links to become usable.
url_dir_list = http://ftp.gnu.org/gnu/$(PACKAGE)

# Used in maint.mk's web-manual rule
manual_title = Parted User's Manual

# Use the direct link.  This is guaranteed to work immediately, while
# it can take a while for the faster mirror links to become usable.
url_dir_list = http://ftp.gnu.org/gnu/$(PACKAGE)

# Tests not to run as part of "make distcheck".
# Exclude changelog-check here so that there's less churn in ChangeLog
# files -- otherwise, you'd need to have the upcoming version number
# at the top of the file for each `make distcheck' run.
local-checks-to-skip = \
  sc_error_message_uppercase \
  sc_error_message_period \
  sc_file_system \
  sc_prohibit_strcmp \
  sc_prohibit_strncpy \
  sc_prohibit_atoi_atof \
  sc_require_test_exit_idiom \
  sc_space_tab \
  sc_texinfo_acronym \
  sc_prohibit_gnu_make_extensions

# Now that we have better (check.mk) tests, make this the default.
export VERBOSE = yes

# Hash of lines 42-208 for release 3.2
old_NEWS_hash = 9d4b517dcd788dce25d05612cb0ac03c

include $(srcdir)/dist-check.mk

useless_free_options = \
  --name=pth_free

# Tools used to bootstrap this package, used for "announcement".
bootstrap-tools = autoconf,automake,gettext,gnulib,gperf

update-copyright-env = \
  UPDATE_COPYRIGHT_USE_INTERVALS=1 \
  UPDATE_COPYRIGHT_MAX_LINE_LENGTH=79

#==> .j/.x-sc_GPL_version <==
#build-aux/vc-list-files

exclude_file_name_regexp--sc_bindtextdomain = ^(libparted/)?tests/.*\.c$$

exclude_file_name_regexp--sc_cross_check_PATH_usage_in_tests = \
  ^libparted/tests/t.*\.sh$$

exclude_file_name_regexp--sc_prohibit_always-defined_macros = \
  ^parted/(strlist|table)\.h$$

exclude_file_name_regexp--sc_prohibit_path_max_allocation = \
  ^libparted/arch/beos\.c$$

exclude_file_name_regexp--sc_unmarked_diagnostics = ^tests/print-max\.c$$
