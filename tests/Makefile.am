TEST_EXTENSIONS = .sh
SH_LOG_COMPILER = $(SHELL)

TESTS = \
  help-version.sh \
  t0000-basic.sh \
  t0001-tiny.sh \
  t0010-script-no-ctrl-chars.sh \
  t0100-print.sh \
  t0101-print-empty.sh \
  t0200-gpt.sh \
  t0201-gpt.sh \
  t0202-gpt-pmbr.sh \
  t0203-gpt-tiny-device-abort.sh \
  t0203-gpt-shortened-device-primary-valid.sh \
  t0203-gpt-create-on-min-sized-device.sh \
  t0205-gpt-list-clobbers-pmbr.sh \
  t0206-gpt-print-with-corrupt-primary-clobbers-pmbr.sh \
  t0207-IEC-binary-notation.sh \
  t0208-mkpart-end-in-IEC.sh \
  t0209-gpt-pmbr_boot.sh \
  t0210-gpt-resized-partition-entry-array.sh \
  t0211-gpt-rewrite-header.sh \
  t0212-gpt-many-partitions.sh \
  t0213-mkpart-start-negative.sh \
  t0215-gpt-attrs.sh \
  t0220-gpt-msftres.sh \
  t0250-gpt.sh \
  t0251-gpt-unicode.sh \
  t0280-gpt-corrupt.sh \
  t0281-gpt-grow.sh \
  t0282-gpt-move-backup.sh \
  t0283-overlap-partitions.sh \
  t0290-gpt-name.sh \
  t0300-dos-on-gpt.sh \
  t0301-overwrite-gpt-pmbr.sh \
  t0350-mac-PT-increases-sector-size.sh \
  t0400-loop-clobber-infloop.sh \
  t0500-dup-clobber.sh \
  t0501-duplicate.sh \
  t0800-json-gpt.sh \
  t0801-json-msdos.sh \
  t0900-type-gpt.sh \
  t0901-type-gpt-invalid.sh \
  t0910-type-dos.sh \
  t0911-type-dos-invalid.sh \
  t1100-busy-label.sh \
  t1101-busy-partition.sh \
  t1102-loop-label.sh \
  t1104-remove-and-add-partition.sh \
  t1700-probe-fs.sh \
  t1701-rescue-fs.sh \
  t2200-dos-label-recog.sh \
  t2201-pc98-label-recog.sh \
  t2300-dos-label-extended-bootcode.sh \
  t2310-dos-extended-2-sector-min-offset.sh \
  t2320-dos-extended-noclobber.sh \
  t2400-dos-hfs-partition-type.sh \
  t2410-dos-udf-partition-type.sh \
  t2500-probe-corrupt-hfs.sh \
  t3000-resize-fs.sh \
  t3200-resize-partition.sh \
  t3200-type-change.sh \
  t3210-gpt-type-change.sh \
  t3300-palo-prep.sh \
  t3310-flags.sh \
  t3400-whole-disk-FAT-partition.sh \
  t4000-sun-raid-type.sh \
  t4001-sun-vtoc.sh \
  t4002-sun-badlabel.sh \
  t4100-msdos-partition-limits.sh \
  t4100-dvh-partition-limits.sh \
  t4100-msdos-starting-sector.sh \
  t4101-dvh-badlabel.sh \
  t4200-partprobe.sh \
  t4300-nilfs2-tiny.sh \
  t4301-nilfs2-badsb2.sh \
  t4302-nilfs2-lessbadsb2.sh \
  t5000-tags.sh \
  t6000-dm.sh \
  t6001-psep.sh \
  t6002-dm-busy.sh \
  t6003-dm-hide.sh \
  t6004-dm-many-partitions.sh \
  t6005-dm-uuid.sh \
  t6006-dm-512b-sectors.sh \
  t6100-mdraid-partitions.sh \
  t7000-scripting.sh \
  t8000-loop.sh \
  t8001-loop-blkpg.sh \
  t9010-big-sector.sh \
  t9020-alignment.sh \
  t9021-maxima.sh \
  t9022-one-unit-snap.sh \
  t9023-value-lt-one.sh \
  t9024-msdos-1s-partition.sh \
  t9025-gpt-1s-partition.sh \
  t9030-align-check.sh \
  t9040-many-partitions.sh \
  t9041-undetected-in-use-16th-partition.sh \
  t9042-dos-partition-limit.sh \
  t9050-partition-table-types.sh \
  t9060-gpt-grow-script-fix.sh

EXTRA_DIST = \
  $(TESTS) t-local.sh t-lvm.sh \
  init.cfg init.sh t-lib-helpers.sh gpt-header-munge \
  gpt-header-move msdos-overlap gpt-attrs sun-badlabel

check_PROGRAMS = print-align print-flags print-max dup-clobber duplicate \
  fs-resize
fs_resize_LDADD = \
  $(top_builddir)/libparted/fs/libparted-fs-resize.la \
  $(top_builddir)/libparted/libparted.la

LDADD = \
  $(top_builddir)/libparted/libparted.la
AM_CPPFLAGS = \
  -I$(top_srcdir)/lib \
  -I$(top_builddir)/include \
  -I$(top_srcdir)/include
AM_CFLAGS = $(WARN_CFLAGS) $(WERROR_CFLAGS)

parted_dir = $(abs_top_builddir)/parted
pp_dir = $(abs_top_builddir)/partprobe
sep = $(PATH_SEPARATOR)

# Note that the first lines are statements.  They ensure that environment
# variables that can perturb tests are unset or set to expected values.
# The rest are envvar settings that propagate build-related Makefile
# variables to test scripts.
TESTS_ENVIRONMENT =				\
  tmp__=$$TMPDIR; test -d "$$tmp__" || tmp__=.;	\
  TMPDIR=$$tmp__; export TMPDIR;		\
  export					\
  abs_top_builddir='$(abs_top_builddir)'	\
  abs_top_srcdir='$(abs_top_srcdir)'		\
  abs_srcdir='$(abs_srcdir)'			\
  built_programs=parted				\
  srcdir='$(srcdir)'				\
  top_srcdir='$(top_srcdir)'			\
  AWK='$(AWK)'					\
  PACKAGE_BUGREPORT='$(PACKAGE_BUGREPORT)'	\
  PACKAGE_VERSION=$(PACKAGE_VERSION)		\
  CONFIG_HEADER='$(abs_top_builddir)/lib/config.h' \
  ENABLE_DEVICE_MAPPER=$(ENABLE_DEVICE_MAPPER)	\
  PARTED_TEST_NAME=`basename '$(abs_srcdir)'`,`echo $$tst|sed 's,^\./,,;s,/,-,g'`\
  PATH='$(abs_top_builddir)/parted$(PATH_SEPARATOR)'"$$PATH" \
  VERSION=$(VERSION)				\
  ; 9>&2

VERBOSE = yes
