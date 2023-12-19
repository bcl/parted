# GNU libparted API

[TOC]

# INTRODUCTION

GNU Parted is built on top of libparted, which does all of the real work.
libparted provides an API capable of manipulating partition tables, and
the filesystems on them.

The main motivation for separating the back-end into a separate library was
to encourage different GNU/Linux distributions to encorporate their own
customized front-end into the install process.

This documents the API -- not the implementation details of libparted.
Documentation that is not relevant to programs using the API are marked with
INTERNAL.  Apart from this file, a good place to look would be
parted/parted.c, the front-end's source, and the TUTORIAL file (not finished
yet!).

This documentation isn't as complete as it should be.  Feel free to ask
questions on the [mailing list](https://alioth-lists.debian.net/cgi-bin/mailman/listinfo/parted-devel).

## TERMINOLOGY
Some of the terminology is a bit weird, so you might want to read this.

* **CONSTRAINT**		a set of conditions that must be satisfied, for
			a given GEOMETRY of a PARTITION.

* **DEVICE**			a storage device.

* **DISK**			a storage device, with a valid partition table.

* **EXCEPTION**		an event that needs attention.

* **EXTENDED PARTITION**	a **PRIMARY PARTITION**, that may contain **LOGICAL
			PARTITIONS** instead of a file system.  There is at most
			one extended partition.

* **FILE SYSTEM**		any data that resides on a partition.  For the purposes
			for GNU Parted, this includes swap devices.

* **GEOMETRY**		a description of a continuous region on a disk.  eg,
			partitions have a geometry.

* **HIDDEN PARTITION**	a partition that is hidden from MS operating systems.
			Only FAT partitions may be hidden.

* **LOGICAL PARTITION**	like normal partitions, but they lie inside the
			extended partition.

* **PARTITION**		a continuous region on a disk where a file system may
			reside.

* **PRIMARY PARTITION**	a normal, vanilla, partition.

* **PARTITION TABLE**		also, **DISK LABEL**.  A description of where the
			partitions lie, and information about those partitions.
			For example, what type of file system resides on them.
			The partition table is usually at the start of the
			disk.

* **TIMER**			a progress meter.  It is an entity that keeps track
			of time, and who to inform when something interesting
			happens.

## DESIGN
libparted has a fairly object-oriented design.  The most important objects are:

* `_PedArchitecture`		describes support for an "archicture", which is sort
			of like "operating system", but could also be,
			for example, another libparted environment, EVMS, etc.
* `_PedConstraint`		a constraint on the geometry of a partition
* `_PedDevice`		a storage device
* `_PedDisk`			a device + partition table
* `_PedFileSystem`		a filesystem, associated with a `_PedGeometry`, NOT a
			`_PedPartition`.
* `_PedGeometry`		a continious region on a device
* `_PedPartition`		a partition (basically `_PedGeometry` plus some attributes)
* `_PedTimer`		a timer keeps track of progress and time

All functions return 0 (or NULL) on failure and non-zero (or non-NULL) on
success.  If a function fails, an exception is thrown.  This may be handled by
either an exception handler, or the calling function (see the section on
exceptions).

All objects should be considered read-only; they should only be modified by
calls to libparted's API.

# INITIALISING LIBPARTED

Headers for libparted can be included with:

    #include <parted/parted.h>

Parted automatically initialises itself via an `__attribute__ ((constructor))`
function.

However, you might want to set the exception handler with
`ped_exception_set_handler()`.  libparted does come with a default exception
handler, if you're feeling lazy.

Here's a minimal example:

``` c
#include <parted/parted.h>

int
main()
{
	/* automatically initialized */
	ped_exception_set_handler(exception_handler);
	return 0;
	/* automatically cleaned up */
}
```

# PEDPARTITION, PEDPARTITIONTYPE

* interface:		`parted/disk.h`
* implementation:		`libparted/disk.c`

A `_PedPartition` represents a partition (surprise!).  `_PedPartition` has a weird
relationships with `_PedDisk`.  Hence, many functions for manipulating partitions
will be called `ped_disk_*` - so have a look at the `_PedDisk` documentation as well.

Parted creates "imaginary" free space and metadata partitions.  You can't
do any operations on these partitions (like `set_geometry()`, `{set,get}_flag`, etc.)
Partitions that are not free space or metadata partitions are said to
be "active" partitions.  You can use `ped_partition_is_active()` to check.


# PEDCONSTRAINT, PEDALIGNMENT

"Alignments" are restrictions on the location of a sector in the form of:

    sector = offset + X * grain_size

For example, logical partitions on msdos disk labels usually have a constraint
with offset = 63 and grain_size = 16065 (Long story!).  An important
(and non-obvious!) property of alignment restrictions is they are closed
under intersection,  i.e. if you take two constraints, like (offset, grain_size)
= (63, 16065) and (0, 4), then either:
* there are no valid solutions
* all solutions can be expressed in the form of (offset + X * grain_size)

In the example, the intersection of the constraint is (16128, 64260).

For more information on the maths, see the source -- there's a large comment
containing proofs above ped_alignment_intersect() in `libparted/cs/natmath.c`

The restrictions on the location of the start and end are in the form of
`_PedGeometry` objects -- continous regions in which the start and end must lie.
Obviously, these restrictions are also closed under intersection.

The other restriction -- the minimum size -- is also closed under intersection.
(The intersection of 2 minimum size restrictions is the maximum of the
2 values)

FIXME: mention ped_alignment_any
