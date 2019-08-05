/* -*- Mode: c; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-

    libparted - a library for manipulating disk partitions
    atari.c - libparted module to manipulate Atari partition tables.
    Copyright (C) 2000-2001, 2004, 2007-2014 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contributor:  Guillaume Knispel <k_guillaume@libertysurf.fr>
                  John Paul Adrian Glaubitz <glaubitz@physik.fu-berlin.de>
*/

/*
	Documentation :
		README file of atari-fdisk
		atari-fdisk source code
		Linux atari partitions parser source code
			( fs/partitions/atari.[ch] )
*/

#include <config.h>

#include <string.h>
#include <parted/parted.h>
#include <parted/debug.h>
#include <parted/endian.h>
#include <string.h>
#include <locale.h>
#include <stdint.h>
#include <ctype.h>
#include <stddef.h>

#include "pt-tools.h"

#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */


/********************** Atari data and structure stuff **********************/

#define BOOTABLE_CKSUM		0x1234
#define NONBOOT_CKSUM		0x4321

#define GEM_MAX			((32*1024*1024)/PED_SECTOR_SIZE_DEFAULT)

#define PART_FLAG_USED		0x01
#define PART_FLAG_BOOT_GEM	0x80	/* GEMDOS	   */
#define PART_FLAG_BOOT_ASV	0x40	/* Atari System V  */
#define PART_FLAG_BOOT_BSD	0x20	/* Net(?)BSD	   */
#define PART_FLAG_BOOT_LNX	0x10	/* Linux	   */
#define PART_FLAG_BOOT_UNK	0x08	/* unknown / other */

#define N_AHDI			4
#define N_ICD			8

#define MAXIMUM_PARTS		64

/* what we put instead of id, start and size in empty */
/* partition tables, to be able to detect it */
#define SIGNATURE_EMPTY_TABLE	"PARTEDATARI"
#define SIGNATURE_EMPTY_SIZE	11

/* to be compared to the last two bytes of 1st sector (Big Endian) */
static const uint16_t atr_forbidden_sign[] = {
	0x55AA,
	0
};

static const char *atr_known_icd_pid[] = {
	"BGM", "GEM", "LNX", "SWP", "RAW", NULL
};

/* static const char *atr_known_pid[] = { */
/* 	"BGM", "GEM", "LNX", "MAC", "MIX", "MNX", "RAW", "SWP", "UNX", */
/* 	"F32", "SV4", NULL */
/* }; */

struct _AtariPartID2BootFlag {
	const char	pid[4];
	uint8_t		flag;
};
typedef struct _AtariPartID2BootFlag AtariPartID2BootFlag;

static AtariPartID2BootFlag atr_pid2bf[] = {
	{ "GEM", PART_FLAG_BOOT_GEM },
	{ "BGM", PART_FLAG_BOOT_GEM },
	{ "UNX", PART_FLAG_BOOT_ASV },
	{ "LNX", PART_FLAG_BOOT_LNX },
	{ "",    PART_FLAG_BOOT_UNK },
};

struct _AtariFS2PartId {
	const char*	fs;
	const char	pid[4];
	PedSector	max_sectors;
};
typedef struct _AtariFS2PartId AtariFS2PartId;

static AtariFS2PartId atr_fs2pid[] = {
/* Other ID are available : MIX MNX <= minix
				UNX <= Atari SysV Unix
				SV4 <= Univ System 4   */
	{ "ext2",	"LNX", INT32_MAX },
	{ "ext3",	"LNX", INT32_MAX },
	{ "fat16",	"GEM",   GEM_MAX },	/* small partitions */
	{ "fat16",	"BGM", INT32_MAX },	/* big partitions */
	{ "fat32",	"F32", INT32_MAX },
	{ "hfs",	"MAC", INT32_MAX },
	{ "hfs+",	"MAC", INT32_MAX },
	{ "hfsx",	"MAC", INT32_MAX },
	{ "jfs",	"LNX", INT32_MAX },
	{ "linux-swap", "SWP", INT32_MAX },
	{ "reiserfs",	"LNX", INT32_MAX },
	{ "hp-ufs",	"LNX", INT32_MAX },
	{ "sun-ufs",	"LNX", INT32_MAX },
	{ "xfs",	"LNX", INT32_MAX },
	{ "ntfs",	"RAW", INT32_MAX },
	{ "",		"RAW", INT32_MAX },	/* default entry */
	{ NULL,		 ""  ,         0 }	/* end of list */
};

struct __attribute__ ((packed)) _AtariRawPartition {
	uint8_t		flag;	/* bit 0: active; bit 7: bootable */
	uint8_t		id[3];	/* "GEM", "BGM", "XGM", ... */
	uint32_t	start;	/* start of partition */
	uint32_t	size;	/* length of partition */
};
typedef struct _AtariRawPartition AtariRawPartition;

struct __attribute__ ((packed,aligned(2))) _AtariRawTable {
	uint8_t		  boot_code[0x156]; /* room for boot code */
	AtariRawPartition icd_part[N_ICD];  /* info for ICD-partitions 5..12 */
	uint8_t		  unused[0xc];
	uint32_t	  hd_size;	/* size of disk in blocks */
	AtariRawPartition part[N_AHDI];	/* the four primary partitions */
	uint32_t	  bsl_start;	/* start of bad sector list */
	uint32_t	  bsl_count;	/* length of bad sector list */
	uint16_t	  checksum;	/* checksum for bootable disks */
};
typedef struct _AtariRawTable AtariRawTable;

typedef enum {
	FMT_AHDI = 0,	/* AHDI v1 compatible, no ICD and no XGM */
	FMT_XGM  = 1,	/* AHDI v3 with XGM / this disable ICD */
	FMT_ICD  = 2	/* ICD detected / requested because more than 4 prim */
			/* no XGM allowed */
} AtrFmt;

struct _AtariDisk {
	AtrFmt	format;
	int	has_been_read;	/* actually means has been read or written... */
	uint32_t bsl_start;	/* first sector of the Bad Sectors List */
	uint32_t bsl_count;	/* number of sectors of the BSL */
	uint8_t	HDX_comp;	/* if set to one, atari_write will initialize */
				/* the bsl area */
};
typedef struct _AtariDisk AtariDisk;

struct _AtariPart {
	char	part_id[4];	/* ASCIIZ */
	char	icd_id[4];	/* Linux only parse a limited set of ID */
				/* in ICD (why???), so everything else  */
				/* is translated to RAW.		*/
	uint8_t	flag;		/* without bit 0 (entry used) */
};
typedef struct _AtariPart AtariPart;

/* set by initialisation code to C locale */
static locale_t atr_c_locale;

static PedDiskType atari_disk_type;



/******************************** Atari Code ********************************/

#define ATARI_DISK(disk)	((AtariDisk*)((disk)->disk_specific))
#define ATARI_PART(part)	((AtariPart*)((part)->disk_specific))

#define atr_pid_eq(a,b)		(!memcmp( (a), (b), 3 ))

#define atr_pid_assign(a, b)	(memcpy ( (a), (b), 3 ))

#define atr_part_used(part)	(((part)->flag) & PART_FLAG_USED)

static int
atr_start_size_correct (uint32_t start, uint32_t size, uint32_t hd_size)
{
	uint32_t end = start + size;

	return  end >= start
		&& 0 < start && start <= hd_size
		&& 0 < size && size <= hd_size
		&& 0 < end && end <= hd_size;
}

static int
atr_part_correct (AtariRawPartition* part, uint32_t hd_size)
{
	uint32_t start, size;

	start = PED_BE32_TO_CPU (part->start);
	size = PED_BE32_TO_CPU (part->size);

	return isalnum_l(part->id[0], atr_c_locale)
		&& isalnum_l(part->id[1], atr_c_locale)
		&& isalnum_l(part->id[2], atr_c_locale)
		&& atr_start_size_correct (start, size, hd_size);
}

static int _GL_ATTRIBUTE_PURE
atr_pid_known (const char* pid, const char** pid_list)
{
	for (; *pid_list; pid_list++) {
		if (atr_pid_eq(pid, *pid_list))
			return 1;
	}

	return 0;
}

/* Recognize Parted signature in an AHDI entry, used to
 * identify empty Atari partition tables */
static int
atr_is_signature_entry (AtariRawPartition* part)
{
	return part->flag == 0
		&& !memcmp (part->id, SIGNATURE_EMPTY_TABLE,
				      SIGNATURE_EMPTY_SIZE );
}

/* Set Parted signature in an AHDI entry */
static void
atr_put_signature_entry (AtariRawPartition* part)
{
	part->flag = 0;
	memcpy (part->id, SIGNATURE_EMPTY_TABLE, SIGNATURE_EMPTY_SIZE);
}

#define atr_part_known(part, pid_list) (atr_pid_known ((part)->id, pid_list))

#define atr_part_valid(part, sz) (atr_part_used(part)\
				  && atr_part_correct((part), (sz)))
#define atr_part_trash(part, sz) (atr_part_used(part)\
				  && !atr_part_correct((part), (sz)))

/* Check if this device can be used with an Atari label */
static int
atr_can_use_dev (const PedDevice *dev)
{
	/* i really don't know how atari behave with non 512 bytes */
	/* sectors... */
	if (dev->sector_size != PED_SECTOR_SIZE_DEFAULT) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("Can't use Atari partition tables on disks with a "
			  "sector size not equal to %d bytes."),
			(int)PED_SECTOR_SIZE_DEFAULT );
		return 0;
	}

	/* the format isn't well defined enough to support > 0x7FFFFFFF */
	/* sectors */
	if (dev->length > INT32_MAX) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("Can't use Atari partition tables on disks with more "
			  "than %d sectors."),
			INT32_MAX );
		return 0;
	}

	return 1;
}

/*
 * The Atari disk label doesn't have any magic id
 * so we must completely parse the layout to be sure
 * we are really dealing with it.
 */
static int
atari_probe (const PedDevice *dev)
{
	AtariRawTable	table;
	uint32_t	rs_hd_size, parts, exts;
	int		valid_count, xgm_part, xgm_num, i;
	int		num_sign, total_count = 0;

	PED_ASSERT (dev != NULL);
	PED_ASSERT (sizeof(table) == 512);

	/* Device Spec ok for Atari label? */
	if (!atr_can_use_dev (dev))
		return 0;

	/* read the root sector */
	if (!ped_device_read (dev, &table, 0, 1))
		return 0;

	/* number of sectors stored in the root sector > device length ? */
	/* => just reject the Atari disk label */
	rs_hd_size = PED_BE32_TO_CPU (table.hd_size);
	if (rs_hd_size > dev->length
	    || rs_hd_size < 2)
		return 0;

	/* check the BSL fields */
	if ((table.bsl_start || table.bsl_count)
	    && !atr_start_size_correct (PED_BE32_TO_CPU (table.bsl_start),
					PED_BE32_TO_CPU (table.bsl_count),
					rs_hd_size ) )
		return 0;

	/* scan the main AHDI fields */
	num_sign = 0; xgm_num = 0;
	valid_count = 0; xgm_part = 0;
	for (i = 0; i < N_AHDI; i++) {
		if (atr_part_valid (&table.part[i], rs_hd_size)) {
			valid_count++;
			total_count++;
			if (atr_pid_eq(table.part[i].id, "XGM")) {
				xgm_part++;
				xgm_num = i;
			}
		} else if (atr_part_trash (&table.part[i], rs_hd_size)) {
			return 0;
		}
		if (atr_is_signature_entry (&table.part[i]))
			num_sign++;
	}

	/* no way to reliably detect empty Atari disk labels if
	 *    they aren't using parted signature in 4 prim fields
	 * && reject multi XGM labels because Parted can't handle
	 *    multiple extended partitions
	 * && reject if xgm partition in slot 0 because not allowed */
	if ((!valid_count && num_sign != N_AHDI)
	    || xgm_part > 1
	    || (xgm_part == 1 && xgm_num == 0) )
		return 0;

	/* check coherency of each logical partitions and ARS */
	if (xgm_part) { /* ! WARNING ! reuses "table" */
		/* we must allow empty ext partition even if they're   */
		/* not valid because parted write the layout to the HD */
		/* at each operation, and we can't create ext and log  */
		/* at the same time */
		int	empty_ars_allowed = 1;

		parts = exts = PED_BE32_TO_CPU (table.part[xgm_num].start);
		while (1) {
			if (!ped_device_read (dev, &table, parts, 1))
				return 0;

			for (i = 0; i < N_AHDI-1; ++i) {
				if (atr_part_used (&table.part[i]))
					break;
			}

			/* we allow the ext part to be empty (see above) */
			if (i == N_AHDI-1 && empty_ars_allowed)
				break;

			/* data partition must be in slot 0, 1 or 2 */
			if (i == N_AHDI-1
			    || !atr_part_correct (&table.part[i], rs_hd_size
								  - parts )
			    || atr_pid_eq (table.part[i].id, "XGM"))
				return 0;

			/* If there is at least one logical partition */
			/* then next ARS should not be empty */
			empty_ars_allowed = 0;

			total_count++;
			if (total_count > MAXIMUM_PARTS) {
				ped_exception_throw (
					PED_EXCEPTION_ERROR,
					PED_EXCEPTION_CANCEL,
					_("Too many Atari partitions detected. "
					" Maybe there is a loop in the XGM "
					"linked list.  Aborting.") );
				return 0;
			}

			/* end of logical partitions? */
			if (!atr_part_used (&table.part[i+1]))
				break;

			/* is this really the descriptor of the next ARS? */
			if (!atr_part_correct (&table.part[i+1], rs_hd_size
								  - exts )
			    || !atr_pid_eq (table.part[i+1].id, "XGM"))
				return 0;

			parts = exts + PED_BE32_TO_CPU (table.part[i+1].start);
		}
	} /* no XGM so try ICD */
	  else if (atr_part_valid (&table.icd_part[0], rs_hd_size)
		   && atr_part_known (&table.icd_part[0], atr_known_icd_pid)) {
		for (i = 1; i < N_ICD; i++) {
			if (atr_part_trash (&table.icd_part[i], rs_hd_size))
				return 0;
		}
	}

	return 1;
}

static void
atr_disk_reset (AtariDisk* atr_disk)
{
	/* Empty partition table => only AHDI needed right now */
	atr_disk->format = FMT_AHDI;
	/* The disk is not in sync with the actual content of the label */
	atr_disk->has_been_read = 0;
	/* Create an empty BSL for HDX compatibility */
	atr_disk->bsl_start = 1;
	atr_disk->bsl_count = 1;
	atr_disk->HDX_comp = 1;
}

/*
 * Must set up the PedDisk and the associated AtariDisk as if
 * the user is doing mklabel, since in this case atari_alloc
 * is called alone whereas when reading an existing partition
 * table atari_read is called after atari_alloc and can overwrite
 * the settings.
 */
static PedDisk*
atari_alloc (const PedDevice* dev)
{
	PedDisk*	disk;
	AtariDisk*	atr_disk;

	PED_ASSERT (dev != NULL);

	if (!atr_can_use_dev (dev)
	    || !(disk = _ped_disk_alloc (dev, &atari_disk_type)))
		return NULL;

	if (!(disk->disk_specific = atr_disk = ped_malloc (sizeof (AtariDisk))))
		goto error_free_disk;

	atr_disk_reset (atr_disk);

	return disk;

error_free_disk:
	free (disk);
	return NULL;
}

static PedDisk*
atari_duplicate (const PedDisk* disk)
{
	PedDisk*	new_disk;
	AtariDisk*	old_atr_dsk;
	AtariDisk*	new_atr_dsk;

	PED_ASSERT (disk != NULL);
	PED_ASSERT (disk->dev != NULL);
	PED_ASSERT (disk->disk_specific != NULL);

	old_atr_dsk = ATARI_DISK (disk);
	if (!(new_disk = ped_disk_new_fresh (disk->dev, &atari_disk_type)))
		return NULL;
	new_atr_dsk = ATARI_DISK (new_disk);

	memcpy (new_atr_dsk, old_atr_dsk, sizeof(*old_atr_dsk));

	return new_disk;
}

static void
atari_free (PedDisk* disk)
{
	AtariDisk* atr_disk;
	PED_ASSERT (disk != NULL);
	PED_ASSERT (disk->disk_specific != NULL);
	atr_disk = ATARI_DISK (disk);

	_ped_disk_free (disk);
	free (atr_disk);
}

/* Warning : ID not ASCIIZ but 3 chars long */
static void
atr_part_sysraw (PedPartition* part, const char* id, uint8_t flag)
{
	AtariPart* atr_part = ATARI_PART (part);

	atr_part->flag = flag & ~PART_FLAG_USED;

	atr_pid_assign (atr_part->part_id, id);
	atr_part->part_id[3] = 0;

	if (atr_pid_known (id, atr_known_icd_pid)) {
		atr_pid_assign (atr_part->icd_id, id);
		atr_part->icd_id[3] = 0;
	} else {
		atr_pid_assign (atr_part->icd_id, "RAW");
		atr_part->icd_id[3] = 0;
	}
}

static int
atr_parse_add_rawpart (PedDisk* disk, PedPartitionType type, PedSector st_off,
		       int num, const AtariRawPartition* rawpart )
{
	PedSector	start, end;
	PedPartition* 	part;
	PedConstraint*	const_exact;
	int		added;

	start = st_off + PED_BE32_TO_CPU (rawpart->start);
	end = start + PED_BE32_TO_CPU (rawpart->size) - 1;

	part = ped_partition_new (disk, type, NULL, start, end);
	if (!part)
		return 0;

	/*part->num = num;*/	/* Enumeration will take care of that */
	part->num = -1;		/* Indeed we can't enumerate here
				 * because the enumerate function uses
				 * -1 do detect new partition being
				 * inserted and update the atrdisk->format */
	if (type != PED_PARTITION_EXTENDED)
		part->fs_type = ped_file_system_probe (&part->geom);
	else
		part->fs_type = NULL;
	atr_part_sysraw (part, rawpart->id, rawpart->flag);

	const_exact = ped_constraint_exact (&part->geom);
	added = ped_disk_add_partition (disk, part, const_exact);
	ped_constraint_destroy (const_exact);
	if (!added) {
		ped_partition_destroy (part);
		return 0;
	}

	PED_ASSERT (part->num == num);
	return 1;
}

/*
 * Read the chained list of logical partitions.
 * exts points to the first Auxiliary Root Sector, at the start
 * of the extended partition.
 * In each ARS one partition entry describes to the logical partition
 * (start relative to the ARS position) and the next entry with ID "XGM"
 * points to the next ARS (start relative to exts).
 */
static int
atr_read_logicals (PedDisk* disk, PedSector exts, int* pnum)
{
	AtariRawTable	table;
	PedSector	parts = exts;
	int		i, empty_ars_allowed = 1;

	while (1) {
		if (!ped_device_read (disk->dev, &table, parts, 1))
			return 0;

		for (i = 0; i < N_AHDI-1; ++i)
			if (atr_part_used (&table.part[i]))
				break;

		if (i == N_AHDI-1 && empty_ars_allowed)
			break;

		/* data partition must be in slot 0, 1 or 2 */
		if (i == N_AHDI-1
		    || atr_pid_eq (table.part[i].id, "XGM")) {
			ped_exception_throw (
				PED_EXCEPTION_ERROR,
				PED_EXCEPTION_CANCEL,
				_("No data partition found in the ARS at "
				  "sector %lli."), parts );
			return 0;
		}

		empty_ars_allowed = 0;

		if (!atr_parse_add_rawpart (disk, PED_PARTITION_LOGICAL,
					    parts, *pnum, &table.part[i] ) )
			return 0;

		(*pnum)++;

		/* end of logical partitions? */
		if (!atr_part_used (&table.part[i+1]))
			break;

		if (!atr_pid_eq (table.part[i+1].id, "XGM")) {
			ped_exception_throw (
				PED_EXCEPTION_ERROR,
				PED_EXCEPTION_CANCEL,
				_("The entry of the next logical ARS is not of "
				  "type XGM in ARS at sector %lli."), parts );
			return 0;
		}

		parts = exts + PED_BE32_TO_CPU (table.part[i+1].start);
	}

	return 1;
}

static int
atari_read (PedDisk* disk)
{
	AtariRawTable	table;
	AtariDisk*	atr_disk;
	uint32_t	rs_hd_size;
	int		i, pnum, xgm, pcount;

	PED_ASSERT (disk != NULL);
	PED_ASSERT (disk->dev != NULL);
	PED_ASSERT (disk->disk_specific != NULL);
	atr_disk = ATARI_DISK (disk);

	ped_disk_delete_all (disk);
	atr_disk_reset (atr_disk);

	if (!atari_probe (disk->dev)) {
		if (ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_IGNORE_CANCEL,
			_("There doesn't seem to be an Atari partition table "
			  "on this disk (%s), or it is corrupted."),
			disk->dev->path )
				!= PED_EXCEPTION_IGNORE)
			return 0;
	}

	if (!ped_device_read (disk->dev, (void*) &table, 0, 1))
		goto error;

	/* We are sure that the layout looks coherent so we
	   don't need to check too much */

	rs_hd_size = PED_BE32_TO_CPU (table.hd_size);
	atr_disk->bsl_start = PED_BE32_TO_CPU (table.bsl_start);
	atr_disk->bsl_count = PED_BE32_TO_CPU (table.bsl_count);
	atr_disk->HDX_comp = 0;

	/* AHDI primary partitions */
	pnum = 1; xgm = 0; pcount = 0;
	for (i = 0; i < N_AHDI; i++) {
		if (!atr_part_used (&table.part[i]))
			continue;

		pcount++;

		if (atr_pid_eq (table.part[i].id, "XGM")) {

			atr_disk->format = FMT_XGM;
			xgm = 1;
			if (!atr_parse_add_rawpart(disk, PED_PARTITION_EXTENDED,
						   0, 0, &table.part[i] )
			    || !atr_read_logicals (
					disk,
					PED_BE32_TO_CPU (table.part[i].start),
					&pnum ) )
				goto error;

		} else {

			if (!atr_parse_add_rawpart (disk, PED_PARTITION_NORMAL,
						    0, pnum, &table.part[i] ) )
				goto error;
			pnum++;
		}
	}

	/* If no XGM partition has been found, the AHDI table is not empty,  */
	/* the first entry is valid and its ID ok for ICD, then we parse the */
	/* ICD table. */
	if (!xgm && pcount != 0
		 && atr_part_valid (&table.icd_part[0], rs_hd_size)
		 && atr_part_known (&table.icd_part[0], atr_known_icd_pid))
	for (i = 0; i < N_ICD; i++) {

		if (!atr_part_known (&table.icd_part[i], atr_known_icd_pid)
		    || !atr_part_used (&table.icd_part[i]))
			continue;
		atr_disk->format = FMT_ICD;

		if (!atr_parse_add_rawpart (disk, PED_PARTITION_NORMAL,
					    0, pnum, &table.icd_part[i] ) )
			goto error;
		pnum++;
	}

	atr_disk->has_been_read = 1;
	return 1;

error:
	ped_disk_delete_all (disk);
	atr_disk_reset (atr_disk);
	return 0;
}

/* Returns the number of the first logical partition or -1 if not found */
static int
atr_find_first_log (const PedDisk* disk)
{
	PedPartition*	part;
	int		first_log, last;

	last = ped_disk_get_last_partition_num (disk);

	for (first_log = 1; first_log <= last; first_log++) {
		if ((part = ped_disk_get_partition (disk, first_log))
		     && (part->type & PED_PARTITION_LOGICAL))
			break;
	}

	return first_log > last ? -1 : first_log;
}

#ifndef DISCOVER_ONLY
static int
atari_clobber (PedDevice* dev)
{
	AtariRawTable table;

	PED_ASSERT (dev != NULL);
	PED_ASSERT (atari_probe (dev));

	if (!ped_device_read (dev, &table, 0, 1))
		return 0;

	/* clear anything but the boot code and the optional ICD table */
	memset (table.boot_code + offsetof (AtariRawTable, hd_size),
		0,
		PED_SECTOR_SIZE_DEFAULT - offsetof (AtariRawTable, hd_size));

	return ped_device_write (dev, &table, 0, 1);
}

/* Computes the checksum of the root sector */
static uint16_t
atr_calc_rs_sum (const AtariRawTable* table)
{
	const uint16_t* word = (uint16_t*)(table);
	const uint16_t* end  = (uint16_t*)(table + 1);
	uint16_t sum;

	for (sum = 0; word < end; word++)
		sum += PED_BE16_TO_CPU(*word);

	return sum;
}

/* Returns 1 if the root sector is bootable, else returns 0 */
static int
atr_is_boot_table (const AtariRawTable* table)
{
	return atr_calc_rs_sum (table) == BOOTABLE_CKSUM;
}

/*
 * Returns 1 if sign belongs to a set of `forbidden' signatures.
 * (e.g.: 55AA which is the MSDOS siganture...)
 * Only used for non bootable root sector since the signature of
 * a bootable one is unique.
 */
static int _GL_ATTRIBUTE_PURE
atr_sign_is_forbidden (uint16_t sign)
{
	const uint16_t* forbidden;

	for (forbidden = atr_forbidden_sign; *forbidden; forbidden++) {
		if (sign == *forbidden)
			return 1;
	}

	return 0;
}

/* Updates table->checksum so the RS will be considered bootable (or not) */
static void
atr_table_set_boot (AtariRawTable* table, int boot)
{
	uint16_t boot_cksum, noboot_cksum;
	uint16_t sum;

	table->checksum = 0;
	sum = atr_calc_rs_sum (table);
	boot_cksum = BOOTABLE_CKSUM - sum;

	if (boot) {
		table->checksum = PED_CPU_TO_BE16 (boot_cksum);
		return;
	}

	noboot_cksum = NONBOOT_CKSUM - sum;

	while (atr_sign_is_forbidden (noboot_cksum)
	       || noboot_cksum == boot_cksum)
		noboot_cksum++;

	table->checksum = PED_CPU_TO_BE16 (noboot_cksum);
}

/* Fill an used partition entry */
static void
atr_fill_raw_entry (AtariRawPartition* rawpart, uint8_t flag, const char* id,
		    uint32_t start, uint32_t size )
{
	rawpart->flag = PART_FLAG_USED | flag;
	atr_pid_assign (rawpart->id, id);
	rawpart->start = PED_CPU_TO_BE32 (start);
	rawpart->size = PED_CPU_TO_BE32 (size);
}

static int
atr_write_logicals (const PedDisk* disk)
{
	AtariRawTable	table;
	PedPartition*	log_curr;
	PedPartition*	log_next;
	PedPartition*	ext;
	PedPartition*	part;
	PedSector	exts;
	PedSector	parts;
	AtariPart*	atr_part;
	int		first_log, pnum, i;

	PED_ASSERT (disk != NULL);

	ext = ped_disk_extended_partition (disk);
	exts = parts = ext->geom.start;

	pnum = first_log = atr_find_first_log (disk);

	while (1) {
		if (pnum != -1) {
			log_curr = ped_disk_get_partition (disk, pnum);
			log_next = ped_disk_get_partition (disk, pnum + 1);
		} else {
			log_curr = log_next = NULL;
		}

		if (log_curr && !(log_curr->type & PED_PARTITION_LOGICAL))
			log_curr = NULL;
		if (log_next && !(log_next->type & PED_PARTITION_LOGICAL))
			log_next = NULL;

		PED_ASSERT (pnum == first_log || log_curr);

		part = ped_disk_get_partition_by_sector (disk, parts);
		if (part && ped_partition_is_active (part)) {
			if (log_curr)
				ped_exception_throw (
					PED_EXCEPTION_ERROR,
					PED_EXCEPTION_CANCEL,
					_("No room at sector %lli to store ARS "
					  "of logical partition %d."),
					parts, pnum );
			else
				ped_exception_throw (
					PED_EXCEPTION_ERROR,
					PED_EXCEPTION_CANCEL,
				      _("No room at sector %lli to store ARS."),
					parts );
			return 0;
		}

		if (!ped_device_read (disk->dev, &table, parts, 1))
			return 0;

		if (!log_curr) {
			PED_ASSERT (!log_next);

			for (i = 0; i < N_AHDI; i++)
				table.part[i].flag &= ~PART_FLAG_USED;
		} else {
			atr_part = ATARI_PART (log_curr);
			atr_fill_raw_entry (&table.part[0], atr_part->flag,
					    atr_part->part_id,
					    log_curr->geom.start - parts,
					    log_curr->geom.length );

			for (i = 1; i < N_AHDI; i++)
				table.part[i].flag &= ~PART_FLAG_USED;

			if (log_next) {
				atr_fill_raw_entry (&table.part[1], 0, "XGM",
					log_next->geom.start - 1 - exts,
					log_next->geom.length + 1 );
			}
		}

		/* TODO: check if we can set that bootable, and when */
		atr_table_set_boot (&table, 0);

		if (!ped_device_write (disk->dev, &table, parts, 1))
			return 0;

		if (!log_next)
			break;

		parts = log_next->geom.start - 1;
		pnum++;
	}

	return 1;
}

static int _GL_ATTRIBUTE_PURE
_disk_logical_partition_count (const PedDisk* disk)
{
	PedPartition*	walk;

	int		count = 0;

	PED_ASSERT (disk != NULL);
	for (walk = disk->part_list; walk;
	     walk = ped_disk_next_partition (disk, walk)) {
		if (ped_partition_is_active (walk)
		    && (walk->type & PED_PARTITION_LOGICAL))
			count++;
	}

	return count;
}

/* Load the HD size from the table and ask to fix it if != device size. */
static int
atr_load_fix_hdsize (const PedDisk* disk, uint32_t* rs_hd_size, AtariRawTable* table)
{
	AtariDisk*	atr_disk = ATARI_DISK (disk);
	int		result = PED_EXCEPTION_UNHANDLED;

	*rs_hd_size = PED_BE32_TO_CPU (table->hd_size);
	if (*rs_hd_size != disk->dev->length) {
		if (atr_disk->has_been_read) {
			result = ped_exception_throw (
				PED_EXCEPTION_WARNING,
				PED_EXCEPTION_FIX | PED_EXCEPTION_IGNORE_CANCEL,
				_("The sector count that is stored in the "
				  "partition table does not correspond "
				  "to the size of your device.  Do you "
				  "want to fix the partition table?") );
			if (result == PED_EXCEPTION_CANCEL)
				return 0;
		}

		if (result == PED_EXCEPTION_UNHANDLED)
			result = PED_EXCEPTION_FIX;

		if (result == PED_EXCEPTION_FIX) {
			*rs_hd_size = disk->dev->length;
			table->hd_size = PED_CPU_TO_BE32(*rs_hd_size);
		}
	}
	return 1;
}

/* Try to init the HDX compatibility Bad Sectors List. */
static int
atr_empty_init_bsl (const PedDisk* disk)
{
	uint8_t		zeros[PED_SECTOR_SIZE_DEFAULT];
	PedSector	sec;
	PedPartition*	part;
	AtariDisk*	atr_disk = ATARI_DISK (disk);

	memset (zeros, 0, PED_SECTOR_SIZE_DEFAULT);
	for (sec = atr_disk->bsl_start;
	     sec < atr_disk->bsl_start + atr_disk->bsl_count;
	     sec++ ) {
		if (sec == atr_disk->bsl_start)
			zeros[3] = 0xA5;
		else
			zeros[3] = 0;
		part = ped_disk_get_partition_by_sector (disk, sec);
		if (part && ped_partition_is_active (part)) {
			ped_exception_throw (
				PED_EXCEPTION_ERROR,
				PED_EXCEPTION_CANCEL,
				_("No room at sector %lli to store BSL."),
				sec );
			return 0;
		}
		ped_device_write (disk->dev, zeros, sec, 1);
	}
	atr_disk->HDX_comp = 0;
	return 1;
}

static int
atari_write (const PedDisk* disk)
{
	AtariRawTable	table;
	AtariDisk*	atr_disk;
	AtariPart*	atr_part;
	PedPartition*	log;
	PedPartition*	ext_part;
	PedPartition*	part = NULL;
	uint32_t	rs_hd_size;
	int		i, xgm_begin, pnum, append_ext;
	int		put_sign, boot, prim_count, last_num;
	PED_ASSERT (disk != NULL);
	PED_ASSERT (disk->dev != NULL);
	atr_disk = ATARI_DISK (disk);
	PED_ASSERT (atr_disk != NULL);

	prim_count = ped_disk_get_primary_partition_count (disk);
	last_num = ped_disk_get_last_partition_num (disk);
	ext_part = ped_disk_extended_partition (disk);

	/* WARNING: similar/related code in atari_enumerate */
	xgm_begin = ((log = ped_disk_get_partition (disk, 1))
		      && (log->type & PED_PARTITION_LOGICAL));
	PED_ASSERT (atr_disk->format != FMT_ICD || ext_part == NULL);
	PED_ASSERT (atr_disk->format != FMT_XGM || prim_count + xgm_begin <= N_AHDI);
	PED_ASSERT (atr_disk->format != FMT_AHDI || (ext_part == NULL && prim_count + xgm_begin <= N_AHDI));

	/* Device Spec ok for Atari label? */
	if (!atr_can_use_dev (disk->dev))
		goto error;

	if (!ped_device_read (disk->dev, (void*) &table, 0, 1))
		goto error;

	boot = atr_is_boot_table (&table);

	table.bsl_start = PED_CPU_TO_BE32 (atr_disk->bsl_start);
	table.bsl_count = PED_CPU_TO_BE32 (atr_disk->bsl_count);

	/* Before anything else check the sector count and */
	/* fix it if necessary */
	if (!atr_load_fix_hdsize (disk, &rs_hd_size, &table))
		goto error;

	append_ext =    (ext_part != NULL)
		     && (_disk_logical_partition_count (disk) == 0);

	/* Fill the AHDI table */
	put_sign = (prim_count == 0);
	pnum = 1;
	for (i = 0; i < N_AHDI; i++) {
		if (pnum > last_num)
			part = NULL;
		else while (pnum <= last_num
			    && !(part = ped_disk_get_partition (disk, pnum)))
			pnum++;

		if (put_sign) {
			atr_put_signature_entry (&table.part[i]);
			continue;
		}

		if (!part && i != 0 && append_ext) {
			part = ext_part;
			append_ext = 0;
		}

		if (!part || (i == 0 && xgm_begin)) {
			table.part[i].flag &= ~PART_FLAG_USED;
			continue;
		}

		if (part->type & PED_PARTITION_LOGICAL)
			part = ext_part;

		PED_ASSERT (part != NULL);

		atr_part = ATARI_PART (part);
		atr_fill_raw_entry (&table.part[i], atr_part->flag,
				    atr_part->part_id, part->geom.start,
				    part->geom.length );

		if (part->type & PED_PARTITION_EXTENDED) {
			while (pnum <= last_num) {
				part = ped_disk_get_partition (disk, pnum);
				if (part &&
				    !(part->type & PED_PARTITION_LOGICAL))
					break;
				pnum++;
			}
		} else
			pnum++;
	}

	if ((ext_part != NULL || atr_disk->format == FMT_AHDI)
	    && pnum <= last_num) {
		ped_exception_throw (PED_EXCEPTION_BUG, PED_EXCEPTION_CANCEL,
			_("There were remaining partitions after filling "
			  "the main AHDI table.") );
		goto error;
	}

	/* Leave XGM or ICD mode if uneeded */
	if (pnum > last_num
	    && (atr_disk->format == FMT_ICD || ext_part == NULL))
		atr_disk->format = FMT_AHDI;

	/* If AHDI mode, check that no ICD will be detected */
	/* and propose to fix */
	if (atr_disk->format == FMT_AHDI
	    && atr_part_valid (&table.icd_part[0], rs_hd_size)
	    && atr_part_known (&table.icd_part[0], atr_known_icd_pid)) {
		int result = PED_EXCEPTION_UNHANDLED;
		result = ped_exception_throw (
			PED_EXCEPTION_WARNING,
			PED_EXCEPTION_YES_NO_CANCEL,
			_("The main AHDI table has been filled with all "
			  "partitions but the ICD table is not empty "
			  "so more partitions of unknown size and position "
			  "will be detected by ICD compatible software.  Do "
			  "you want to invalidate the ICD table?") );
		if (result == PED_EXCEPTION_YES
		    || result == PED_EXCEPTION_UNHANDLED)
			table.icd_part[0].flag &= ~PART_FLAG_USED;
		else if (result == PED_EXCEPTION_CANCEL)
			goto error;
	}

	if (put_sign)
		goto write_to_dev;

	/* Fill the ICD table */
	if (atr_disk->format == FMT_ICD)
	for (i = 0; i < N_ICD; i++) {
		if (pnum > last_num)
			part = NULL;
		else while (pnum <= last_num
			    && !(part = ped_disk_get_partition (disk, pnum)))
			pnum++;

		if (!part) {
			table.icd_part[i].flag &= ~PART_FLAG_USED;
			continue;
		}

		if (part->type & PED_PARTITION_EXTENDED
		    || part->type & PED_PARTITION_LOGICAL) {
			ped_exception_throw (
				PED_EXCEPTION_BUG,
				PED_EXCEPTION_CANCEL,
				_("ICD entries can't contain extended or "
				  "logical partitions.") );
			goto error;
		}

		atr_part = ATARI_PART (part);
		atr_fill_raw_entry (&table.icd_part[i], atr_part->flag,
				    atr_part->icd_id, part->geom.start,
				    part->geom.length );

		pnum++;
	}

	/* Write the chained list of logical partitions */
	if (atr_disk->format == FMT_XGM) {
		if (!atr_write_logicals (disk))
			goto error;
	}

write_to_dev:
	if (pnum <= last_num) {
		ped_exception_throw (PED_EXCEPTION_BUG, PED_EXCEPTION_CANCEL,
			_("There were remaining partitions after filling "
			  "the tables.") );
		goto error;
	}

	/* Do we need to do that in case of failure too??? */
	atr_table_set_boot (&table, boot);

	/* Commit the root sector... */
	if (!ped_device_write (disk->dev, (void*) &table, 0, 1)
	    || !ped_device_sync (disk->dev))
		goto error;

	/* Try to init the HDX compatibility Bad Sectors List if needed. */
	if (atr_disk->HDX_comp && !atr_empty_init_bsl (disk))
		goto error;

	atr_disk->has_been_read = 1;
	return ped_device_sync (disk->dev);

error:
	atr_disk->has_been_read = 0;
	return 0;
}
#endif

/* If extended partition in ICD mode, generate an error and returns 1 */
/* else returns 0 */
static int
atr_xgm_in_icd (const PedDisk* disk, PedPartitionType part_type)
{
	AtariDisk* atrdisk;

	PED_ASSERT (disk != NULL);

	if (part_type & PED_PARTITION_EXTENDED) {
		atrdisk = ATARI_DISK (disk);
		if (atrdisk->format == FMT_ICD) {
			ped_exception_throw (
			      PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			      _("You can't use an extended XGM partition in "
				"ICD mode (more than %d primary partitions, if "
				"XGM is the first one it counts for two)."),
				N_AHDI );
			return 1;
		}
	}

	return 0;
}

static PedPartition*
atari_partition_new (const PedDisk* disk, PedPartitionType part_type,
		     const PedFileSystemType* fs_type,
		     PedSector start, PedSector end)
{
	PedPartition*	part;
	AtariPart*	atrpart;

	if (atr_xgm_in_icd(disk, part_type))
		return 0;

	part = _ped_partition_alloc (disk, part_type, fs_type, start, end);
	if (!part)
		goto error;
	if (ped_partition_is_active (part)) {
		part->disk_specific = atrpart = ped_malloc (sizeof (AtariPart));
		if (!atrpart)
			goto error_free_part;
		memset (atrpart, 0, sizeof (AtariPart));
	} else {
		part->disk_specific = NULL;
	}
	return part;

error_free_part:
	_ped_partition_free (part);
error:
	return NULL;
}

static PedPartition*
atari_partition_duplicate (const PedPartition* part)
{
	PedPartition*	new_part;

	new_part = ped_partition_new (part->disk, part->type,
				      part->fs_type, part->geom.start,
				      part->geom.end);
	if (!new_part)
		return NULL;
	new_part->num = part->num;
	if (ped_partition_is_active (part))
		memcpy (new_part->disk_specific, part->disk_specific,
			sizeof (AtariPart));

	return new_part;
}

static void
atari_partition_destroy (PedPartition* part)
{
	PED_ASSERT (part != NULL);

	if (ped_partition_is_active (part)) {
		PED_ASSERT (part->disk_specific != NULL);
		free (part->disk_specific);
	}
	_ped_partition_free (part);
}

/* Note: fs_type is NULL for extended partitions */
static int
atari_partition_set_system (PedPartition* part,
			    const PedFileSystemType* fs_type)
{
	AtariPart*	atrpart;
	AtariFS2PartId* fs2id;
	PED_ASSERT (part != NULL);
	atrpart = ATARI_PART (part);
	PED_ASSERT (atrpart != NULL);

	part->fs_type = fs_type;

	if (atr_xgm_in_icd(part->disk, part->type))
		return 0;

	if (part->type & PED_PARTITION_EXTENDED) {
		strcpy (atrpart->part_id, "XGM");
		strcpy (atrpart->icd_id,  "XGM");
		return 1;
	}

	if (!fs_type) {
		strcpy (atrpart->part_id, "RAW");
		strcpy (atrpart->icd_id,  "RAW");
		return 1;
	}

	for (fs2id = atr_fs2pid; fs2id->fs; fs2id++) {
		if (!*fs2id->fs    /* default entry */
		    || ((!strcmp (fs_type->name, fs2id->fs)
		        && part->geom.length < fs2id->max_sectors))) {

			strcpy (atrpart->part_id, fs2id->pid);
			if (atr_pid_known (fs2id->pid, atr_known_icd_pid))
				strcpy (atrpart->icd_id, fs2id->pid);
			else
				strcpy (atrpart->icd_id, "RAW");

			break;
		}
	}
	PED_ASSERT (fs2id->fs != NULL);

	return 1;
}

static int
atari_partition_set_flag (PedPartition* part, PedPartitionFlag flag, int state)
{
	AtariPart* atr_part;
	AtariPartID2BootFlag* bf;

	PED_ASSERT (part != NULL);
	atr_part = ATARI_PART (part);
	PED_ASSERT (atr_part != NULL);

	if (flag != PED_PARTITION_BOOT)
		return 0;

	if (state == 0) {
		atr_part->flag = 0;
	} else {
		for (bf = atr_pid2bf; *bf->pid; bf++) {
			if (atr_pid_eq (bf->pid, atr_part->part_id))
				break;
		}
		atr_part->flag = bf->flag;
	}

	return 1;
}

static int _GL_ATTRIBUTE_PURE
atari_partition_get_flag (const PedPartition* part, PedPartitionFlag flag)
{
	AtariPart* atr_part;

	PED_ASSERT (part != NULL);
	atr_part = ATARI_PART (part);
	PED_ASSERT (atr_part != NULL);

	if (flag != PED_PARTITION_BOOT)
		return 0;

	return (atr_part->flag != 0);
}

static int
atari_partition_is_flag_available (const PedPartition* part,
				   PedPartitionFlag flag)
{
	if (flag == PED_PARTITION_BOOT)
		return 1;

	return 0;
}

/* Adapted from disk_dos */
static PedConstraint*
atr_log_constraint (const PedPartition* part)
{
	const PedGeometry*	geom = &part->geom;
	PedGeometry	safe_space;
	PedSector	min_start;
	PedSector	max_end;
	PedDisk*	disk;
	PedDevice*	dev;
	PedPartition*	ext_part;
	PedPartition*	walk;
	int		first_log, not_first;

	PED_ASSERT (part->disk != NULL);
	PED_ASSERT (part->disk->dev != NULL);
	ext_part = ped_disk_extended_partition (part->disk);
	PED_ASSERT (ext_part != NULL);

	dev = (disk = part->disk) -> dev;

	first_log = atr_find_first_log (disk);
	if (first_log == -1)
		first_log = part->num;

	not_first = (part->num != first_log);

	walk = ext_part->part_list;

	min_start = ext_part->geom.start + 1 + not_first;
	max_end = ext_part->geom.end;

	while (walk != NULL
		&& (   walk->geom.start - (walk->num != first_log)
						< geom->start - not_first
		    || walk->geom.start - (walk->num != first_log)
						< min_start ) ) {
		if (walk != part && ped_partition_is_active (walk))
			min_start = walk->geom.end + 1 + not_first;
		walk = walk->next;
	}

	while (walk && (walk == part || !ped_partition_is_active (walk)))
		walk = walk->next;

	if (walk)
		max_end = walk->geom.start - 1 - (walk->num != first_log);

	if (min_start >= max_end)
		return NULL;

	ped_geometry_init (&safe_space, dev, min_start,
			   max_end - min_start + 1);
	return ped_constraint_new_from_max (&safe_space);
}

/* Adapted from disk_dos */
static PedGeometry*
art_min_extended_geom (const PedPartition* ext_part)
{
	PedDisk*	disk = ext_part->disk;
	PedPartition*	walk;
	PedGeometry*	min_geom;
	int		first_log;

	first_log = atr_find_first_log (disk);
	if (first_log == -1)
		return NULL;

	walk = ped_disk_get_partition (disk, first_log);
	PED_ASSERT (walk->type & PED_PARTITION_LOGICAL);
	min_geom = ped_geometry_duplicate (&walk->geom);
	if (!min_geom)
		return NULL;
	ped_geometry_set_start (min_geom, walk->geom.start - 1);

	for (walk = ext_part->part_list; walk; walk = walk->next) {
		if (!ped_partition_is_active (walk) || walk->num == first_log)
			continue;
		if (walk->geom.start < min_geom->start)
			ped_geometry_set_start (min_geom, walk->geom.start - 2);
		if (walk->geom.end > min_geom->end)
			ped_geometry_set_end (min_geom, walk->geom.end);
	}

	return min_geom;
}

/* Adapted from disk_dos */
static PedConstraint*
atr_ext_constraint (const PedPartition* part)
{
	PedGeometry	start_range;
	PedGeometry	end_range;
	PedConstraint*	constraint;
	PedDevice*	dev;
	PedDisk*	disk;
	PedGeometry*	min;

	PED_ASSERT (part->disk != NULL);
	PED_ASSERT (part->disk->dev != NULL);

	dev = (disk = part->disk) -> dev;
	min = art_min_extended_geom (part);

	if (min) {
		ped_geometry_init (&start_range, dev, 1, min->start);
		ped_geometry_init (&end_range, dev, min->end,
				   dev->length - min->end);
		ped_geometry_destroy (min);
	} else {
		ped_geometry_init (&start_range, dev, 1, dev->length - 1);
		ped_geometry_init (&end_range, dev, 1, dev->length - 1);
	}

	constraint = ped_constraint_new (ped_alignment_any, ped_alignment_any,
				&start_range, &end_range, 1, dev->length);
	return constraint;
}

static PedConstraint*
atr_prim_constraint (const PedPartition* part)
{
	PedDevice*	dev;
	PedGeometry	max;

	PED_ASSERT (part->disk != NULL);
	PED_ASSERT (part->disk->dev != NULL);

	dev = part->disk->dev;

	ped_geometry_init (&max, dev, 1, dev->length - 1);
	return ped_constraint_new_from_max (&max);
}

/* inspiration from disk_dos */
static PedGeometry*
_best_solution (PedGeometry* a, PedGeometry* b)
{
	if (!a)
		return b;
	if (!b)
		return a;

	if (a->length < b->length)
		goto choose_b;

	ped_geometry_destroy (b);
	return a;

choose_b:
	ped_geometry_destroy (a);
	return b;
}

/* copied from disk_dos */
static PedGeometry*
_try_constraint (const PedPartition* part, const PedConstraint* external,
		 PedConstraint* internal)
{
	PedConstraint*		intersection;
	PedGeometry*		solution;

	intersection = ped_constraint_intersect (external, internal);
	ped_constraint_destroy (internal);
	if (!intersection)
		return NULL;

	solution = ped_constraint_solve_nearest (intersection, &part->geom);
	ped_constraint_destroy (intersection);
	return solution;
}

/*
 * internal is either the primary or extented constraint.
 * If there's no BSL, the is the only internal constraint considered.
 * If there's a BSL, try to fit the partition before or after (and
 * choose the best fit, the one which results in the greatest size...)
 */
static int
atr_prim_align (PedPartition* part, const PedConstraint* constraint,
		PedConstraint* internal)
{
	PedDevice*	dev;
	AtariDisk*	atr_disk;
	PedConstraint*	cut;
	PedGeometry*	solution = NULL;
	PedGeometry	max;
	PedSector	bsl_end;

	PED_ASSERT (part->disk != NULL);
	PED_ASSERT (part->disk->dev != NULL);
	dev = part->disk->dev;
	atr_disk = ATARI_DISK (part->disk);
	PED_ASSERT (atr_disk != NULL);

	/* No BSL */
	if (!atr_disk->bsl_start && !atr_disk->bsl_count) {
		/* Note: _ped_partition_attempt_align will destroy internal */
		return _ped_partition_attempt_align(part, constraint, internal);
	}

	/* BSL, try to fit before */
	if (atr_disk->bsl_start > 1) {
		ped_geometry_init (&max, dev, 1, atr_disk->bsl_start - 1);
		cut = ped_constraint_new_from_max (&max);
		solution = _best_solution (solution,
				_try_constraint (part, constraint,
				     ped_constraint_intersect (internal, cut)));
		ped_constraint_destroy (cut);
	}

	/* BSL, try to fit after, take the best solution */
	bsl_end = atr_disk->bsl_start + atr_disk->bsl_count;
	if (bsl_end < dev->length) {
		ped_geometry_init (&max, dev, bsl_end, dev->length - bsl_end);
		cut = ped_constraint_new_from_max (&max);
		solution = _best_solution (solution,
				_try_constraint (part, constraint,
				     ped_constraint_intersect (internal, cut)));
		ped_constraint_destroy (cut);
	}

	ped_constraint_destroy (internal);

	if (solution) {
		ped_geometry_set (&part->geom, solution->start,
				  solution->length);
		ped_geometry_destroy (solution);
		return 1;
	}

	return 0;
}

static int
atari_partition_align (PedPartition* part, const PedConstraint* constraint)
{
	PED_ASSERT (part != NULL);

	switch (part->type) {
	    case PED_PARTITION_LOGICAL:
		if (_ped_partition_attempt_align (part, constraint,
						  atr_log_constraint (part) ) )
			return 1;
		break;
	    case PED_PARTITION_EXTENDED:
		if (atr_prim_align (part, constraint,
				    atr_ext_constraint (part) ) )
			return 1;
		break;
	    default:
		if (atr_prim_align (part, constraint,
				    atr_prim_constraint (part) ) )
			return 1;
		break;
	}

#ifndef DISCOVER_ONLY
	ped_exception_throw (
		PED_EXCEPTION_ERROR,
		PED_EXCEPTION_CANCEL,
		_("Unable to satisfy all constraints on the partition."));
#endif
	return 0;
}

/* increment numbers of any non logical partition found after the last */
/* logical one, to make room for a new logical partition */
static int
art_room_for_logic (PedDisk* disk)
{
	PedPartition*	part;
	int		num, last_logic, last;

	/* too many partitions ? */
	last = ped_disk_get_last_partition_num (disk);
	if (last >= MAXIMUM_PARTS)
		return 0;

	/* find the last logical partition */
	last_logic = 0;
	for (num = 1; num <= last; num++) {
		part = ped_disk_get_partition (disk, num);
		if (part && ped_partition_is_active (part)
		         && (part->type & PED_PARTITION_LOGICAL))
			last_logic = num;
	}

	if (!last_logic)
		return 1;

	/* increment */
	for (num = last; num > last_logic; num--) {
		part = ped_disk_get_partition (disk, num);
		if (part && ped_partition_is_active (part)
		         && !(part->type & ( PED_PARTITION_LOGICAL
					   | PED_PARTITION_EXTENDED))
			 && part->num > 0 )
			part->num++;
	}

	return 1;
}

static int
atari_partition_enumerate (PedPartition* part)
{
	AtariDisk*	atrdisk;
	PedPartition*	ext_part;
	PedPartition*	log;
	int		i, want_icd, want_xgm, num_max, xgm_begin, prim_count;

	PED_ASSERT (part != NULL);
	PED_ASSERT (part->disk != NULL);
	atrdisk = ATARI_DISK (part->disk);
	PED_ASSERT (atrdisk != NULL);

	/* WARNING: some similar/related code in atari_write */
	/* This is quite a <hack> : this function is probably the only way   */
	/* to know something has been / is going to be modified in the table.*/
	/* So we detect the current operation mode (AHDI/XGM/ICD) and report */
	/* errors (in which case we refuse to operate...) */

	prim_count = ped_disk_get_primary_partition_count (part->disk);
	ext_part = ped_disk_extended_partition (part->disk);

	/* <hack in the hack> : we can't reorder (yet) , so if we begin with */
	/* XGM the first slot must be empty */
	xgm_begin = ((log = ped_disk_get_partition (part->disk, 1))
		      && (log->type & PED_PARTITION_LOGICAL))
		    || ((part->num == -1)
		        && (part->type & PED_PARTITION_LOGICAL)
			&& !ped_disk_get_partition (part->disk, 1));
	/* </hack in the hack> */

	PED_ASSERT (atrdisk->format != FMT_ICD || ext_part == NULL);
	PED_ASSERT (atrdisk->format != FMT_XGM
		    || prim_count + xgm_begin <= N_AHDI);
	PED_ASSERT (atrdisk->format != FMT_AHDI
		    || (ext_part == NULL && prim_count + xgm_begin <= N_AHDI));

	want_icd = ( ( prim_count
			+ xgm_begin
			+ ( (part->num == -1)
			    && !(part->type & PED_PARTITION_LOGICAL) ) )
		      > N_AHDI );
	want_xgm = ( (part->type & PED_PARTITION_EXTENDED)
	             || ext_part != NULL );

	if (!want_xgm && !want_icd)
		atrdisk->format = FMT_AHDI;
	else if (want_xgm && !want_icd)
		atrdisk->format = FMT_XGM;
	else if (!want_xgm && want_icd)
		atrdisk->format = FMT_ICD;
	else {
		if (atr_xgm_in_icd (part->disk, PED_PARTITION_EXTENDED))
			return 0;
		else {
			ped_exception_throw (
				PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
			      _("You can't use more than %d primary partitions "
			        "(ICD mode) if you use an extended XGM "
				"partition.  If XGM is the first partition "
				"it counts for two."),
				N_AHDI );
			return 0;
		}
	}
	/* End of </hack> */


	/* Ext will be numbered 0 and will stay 0... */
	if (part->num == 0)
		return 1;

	if (part->num == -1) {

		/* Linux don't show the ext part itself for Atari disk labels */
		/* so we use number 0 (could use a big number too, but that   */
		/* would be less cute ;) */
		if (part->type & PED_PARTITION_EXTENDED) {
			part->num = 0;
			return 1;
		}

		switch (atrdisk->format) {
		    case FMT_AHDI:
		    case FMT_ICD:
			num_max = N_ICD + N_AHDI;
			break;
		    case FMT_XGM:
			num_max = MAXIMUM_PARTS;
			break;
		    default:
			num_max = 0;
			PED_ASSERT (0);
		}

		/* make room for logical partitions */
		if (part->type & PED_PARTITION_LOGICAL) {
			if (!art_room_for_logic (part->disk))
				goto error_alloc_failed;
		}

		/* find an unused number */
		for (i = 1; i <= num_max; i++) {
			if (!ped_disk_get_partition (part->disk, i)) {
				part->num = i;
				return 1;
			}
		}

	} else {
		/* find an unused number before or don't re-number */
		for (i = 1; i < part->num; i++) {
			if (!ped_disk_get_partition (part->disk, i)) {
				part->num = i;
			}
		}
		return 1;
	}

	/* failed to allocate a number */
error_alloc_failed:
#ifndef DISCOVER_ONLY
	ped_exception_throw (PED_EXCEPTION_ERROR, PED_EXCEPTION_CANCEL,
		_("Unable to allocate a partition number."));
#endif
	return 0;
}

static int
atr_creat_add_metadata (PedDisk* disk, PedSector start, PedSector end,
			PedPartitionType type )
{
	PedPartition*	new_part;
	PedConstraint*	const_exact;
	int		added;

	type |= PED_PARTITION_METADATA;
	new_part = ped_partition_new (disk, type, NULL, start, end);
	if (!new_part)
		goto error;

	const_exact = ped_constraint_exact (&new_part->geom);
	added = ped_disk_add_partition (disk, new_part, const_exact);
	ped_constraint_destroy (const_exact);
	if (!added)
		goto error_destroy_part;

	return 1;

error_destroy_part:
	ped_partition_destroy (new_part);
error:
	return 0;
}

static int
atari_alloc_metadata (PedDisk* disk)
{
	PedPartition*	ext;
	PedPartition*	log;
	AtariDisk*	atr_disk;
	int		i;

	PED_ASSERT (disk != NULL);
	PED_ASSERT (disk->dev != NULL);
	atr_disk = ATARI_DISK (disk);
	PED_ASSERT (atr_disk != NULL);

	/* allocate 1 sector for the disk label at the start */
	if (!atr_creat_add_metadata (disk, 0, 0, 0))
		return 0;

	/* allocate the sectors containing the BSL */
	if (atr_disk->bsl_start || atr_disk->bsl_count) {
		if (!atr_creat_add_metadata (disk, atr_disk->bsl_start,
					     atr_disk->bsl_start
					      + atr_disk->bsl_count - 1, 0 ) )
			return 0;
	}

	ext = ped_disk_extended_partition (disk);
	if (ext) {
		if (!atr_creat_add_metadata (disk, ext->geom.start,
					     ext->geom.start,
					     PED_PARTITION_LOGICAL ) )
			return 0;

		/* Find the first logical part */
		for (i = 1; i <= ped_disk_get_last_partition_num (disk); i++)
			if ((log = ped_disk_get_partition (disk, i))
			    && (log->type & PED_PARTITION_LOGICAL))
				break;

		for (log = ext->part_list; log; log = log->next) {
			if ((log->type & ( PED_PARTITION_METADATA
					 | PED_PARTITION_FREESPACE))
			    || log->num == i)
				continue;

			if (!atr_creat_add_metadata (disk, log->geom.start-1,
						     log->geom.start-1,
						     PED_PARTITION_LOGICAL ) )
				return 0;
		}
	}

	return 1;
}

static int _GL_ATTRIBUTE_PURE
atari_get_max_primary_partition_count (const PedDisk* disk)
{
	AtariDisk*	atr_disk;

	PED_ASSERT (disk != NULL);
	atr_disk = ATARI_DISK (disk);
	PED_ASSERT (atr_disk != NULL);

	return atr_disk->format == FMT_XGM ? N_AHDI : N_AHDI + N_ICD;
}

static bool
atari_get_max_supported_partition_count (const PedDisk* disk, int *max_n)
{
	AtariDisk*	atr_disk;

	PED_ASSERT (disk != NULL);
	atr_disk = ATARI_DISK (disk);
	PED_ASSERT (atr_disk != NULL);

	*max_n = atr_disk->format == FMT_XGM ? N_AHDI : N_AHDI + N_ICD;
        return true;
}

#include "pt-common.h"
PT_define_limit_functions(atari)

static PedDiskOps atari_disk_ops = {
	clobber:	        NULL_IF_DISCOVER_ONLY (atari_clobber),
	write:			NULL_IF_DISCOVER_ONLY (atari_write),

	partition_set_name:	NULL,
	partition_get_name:	NULL,

        PT_op_function_initializers (atari)
};

static PedDiskType atari_disk_type = {
	next:		NULL,
	name:		"atari",
	ops:		&atari_disk_ops,
	features:	PED_DISK_TYPE_EXTENDED
};

void
ped_disk_atari_init ()
{
	PED_ASSERT (sizeof (AtariRawPartition) == 12);
	PED_ASSERT (sizeof (AtariRawTable) == 512);
	/* GNU Libc doesn't support NULL instead of the locale name */
	PED_ASSERT ((atr_c_locale = newlocale(LC_ALL_MASK, "C", NULL)) != NULL);

	ped_disk_type_register (&atari_disk_type);
}

void
ped_disk_atari_done ()
{
	ped_disk_type_unregister (&atari_disk_type);
	freelocale(atr_c_locale);
}
