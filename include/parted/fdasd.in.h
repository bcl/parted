/*
 * File...........: s390-tools/fdasd/fdasd.h
 * Author(s)......: Volker Sameske <sameske@de.ibm.com>
 *                  Horst Hummel   <Horst.Hummel@de.ibm.com>
 * Bugreports.to..: <Linux390@de.ibm.com>
 * (C) IBM Corporation, IBM Deutschland Entwicklung GmbH, 2001-2002
 *
 * History of changes (starts March 2001)
 * version 1.01 - menu entry 's' to show mapping devnode - DS name
 *         1.02 - DS names count now from 0001 instead from 0000
 *         1.03 - volser checks: 'AA AAA' to 'AAAAA '
 *              - removed dependency to kernel headers.
 *         1.04 - added -p option
 *         1.05 - new API policy, set it back to 0
 */
#ifndef FDASD_H
#define FDASD_H

#include <parted/vtoc.h>

/*****************************************************************************
 * SECTION: Definitions needed for DASD-API (see dasd.h)                     *
 *****************************************************************************/

#define DASD_IOCTL_LETTER 'D'

#define DASD_PARTN_BITS 2

#define PARTITION_LINUX_SWAP    0x82
#define PARTITION_LINUX         0x83
#define PARTITION_LINUX_EXT     0x85
#define PARTITION_LINUX_LVM     0x8e
#define PARTITION_LINUX_RAID    0xfd
#define PARTITION_LINUX_LVM_OLD 0xfe

#define PART_TYPE_NATIVE "NATIVE"
#define PART_TYPE_SWAP   "SWAP  "
#define PART_TYPE_RAID   "RAID  "
#define PART_TYPE_LVM    "LVM   "

#ifdef DEBUG_DASD
#define PDEBUG           fprintf(stderr, "%s:%d:%s\n", \
                         __FILE__,                              \
                         __LINE__,                              \
                         __PRETTY_FUNCTION__);
#else
#define PDEBUG
#endif

/*
 * struct dasd_information_t
 * represents any data about the device, which is visible to userspace.
 *  including foramt and featueres.
 */
typedef struct dasd_information_t {
	unsigned int devno;           /* S/390 devno                         */
	unsigned int real_devno;      /* for aliases                         */
	unsigned int schid;           /* S/390 subchannel identifier         */
	unsigned int cu_type  : 16;   /* from SenseID                        */
	unsigned int cu_model :  8;   /* from SenseID                        */
	unsigned int dev_type : 16;   /* from SenseID                        */
	unsigned int dev_model : 8;   /* from SenseID                        */
	unsigned int open_count;
	unsigned int req_queue_len;
	unsigned int chanq_len;       /* length of chanq                     */
	char type[4];                 /* from discipline.name, 'none' for    */
	                              /* unknown                             */
	unsigned int status;          /* current device level                */
	unsigned int label_block;     /* where to find the VOLSER            */
	unsigned int FBA_layout;      /* fixed block size (like AIXVOL)      */
	unsigned int characteristics_size;
	unsigned int confdata_size;
	char characteristics[64];     /* from read_device_characteristics    */
	char configuration_data[256]; /* from read_configuration_data        */
} dasd_information_t;

struct dasd_eckd_characteristics {
	unsigned short cu_type;
	struct {
		unsigned char support:2;
		unsigned char async:1;
		unsigned char reserved:1;
		unsigned char cache_info:1;
		unsigned char model:3;
	} __attribute__ ((packed)) cu_model;
	unsigned short dev_type;
	unsigned char dev_model;
	struct {
		unsigned char mult_burst:1;
		unsigned char RT_in_LR:1;
		unsigned char reserved1:1;
		unsigned char RD_IN_LR:1;
		unsigned char reserved2:4;
		unsigned char reserved3:8;
		unsigned char defect_wr:1;
		unsigned char XRC_supported:1;
		unsigned char reserved4:1;
		unsigned char striping:1;
		unsigned char reserved5:4;
		unsigned char cfw:1;
		unsigned char reserved6:2;
		unsigned char cache:1;
		unsigned char dual_copy:1;
		unsigned char dfw:1;
		unsigned char reset_alleg:1;
		unsigned char sense_down:1;
	} __attribute__ ((packed)) facilities;
	unsigned char dev_class;
	unsigned char unit_type;
	unsigned short no_cyl;
	unsigned short trk_per_cyl;
	unsigned char sec_per_trk;
	unsigned char byte_per_track[3];
	unsigned short home_bytes;
	unsigned char formula;
	union {
		struct {
			unsigned char f1;
			unsigned short f2;
			unsigned short f3;
		} __attribute__ ((packed)) f_0x01;
		struct {
			unsigned char f1;
			unsigned char f2;
			unsigned char f3;
			unsigned char f4;
			unsigned char f5;
		} __attribute__ ((packed)) f_0x02;
	} __attribute__ ((packed)) factors;
	unsigned short first_alt_trk;
	unsigned short no_alt_trk;
	unsigned short first_dia_trk;
	unsigned short no_dia_trk;
	unsigned short first_sup_trk;
	unsigned short no_sup_trk;
	unsigned char MDR_ID;
	unsigned char OBR_ID;
	unsigned char director;
	unsigned char rd_trk_set;
	unsigned short max_rec_zero;
	unsigned char reserved1;
	unsigned char RWANY_in_LR;
	unsigned char factor6;
	unsigned char factor7;
	unsigned char factor8;
	unsigned char reserved2[3];
	unsigned char reserved3[6];
	unsigned int long_no_cyl;
} __attribute__ ((packed));

/*
 * struct format_data_t
 * represents all data necessary to format a dasd
 */
typedef struct format_data_t {
	int start_unit; /* from track */
	int stop_unit;  /* to track */
	int blksize;    /* sectorsize */
	int intensity;
} format_data_t;

/*
 * values to be used for format_data_t.intensity
 * 0/8: normal format
 * 1/9: also write record zero
 * 3/11: also write home address
 * 4/12: invalidate track
 */
#define DASD_FMT_INT_FMT_R0 1 /* write record zero */
#define DASD_FMT_INT_FMT_HA 2 /* write home address, also set FMT_R0 ! */
#define DASD_FMT_INT_INVAL  4 /* invalidate tracks */
#define DASD_FMT_INT_COMPAT 8 /* use OS/390 compatible disk layout */


/* Disable the volume (for Linux) */
#define BIODASDDISABLE _IO(DASD_IOCTL_LETTER,0)
/* Enable the volume (for Linux) */
#define BIODASDENABLE  _IO(DASD_IOCTL_LETTER,1)

/* retrieve API version number */
#define DASDAPIVER     _IOR(DASD_IOCTL_LETTER,0,int)
/* Get information on a dasd device (enhanced) */
#define BIODASDINFO   _IOR(DASD_IOCTL_LETTER,1,dasd_information_t)


/*****************************************************************************
 * SECTION: Further IOCTL Definitions  (see fs.h)                            *
 *****************************************************************************/
#define BLKROGET   _IO(0x12,94) /* get read-only status (0 = read_write) */
/* re-read partition table */
#define BLKRRPART  _IO(0x12,95)
/* get block device sector size */
#define BLKSSZGET  _IO(0x12,104)
/* device size in bytes (u64 *arg)*/
#define BLKGETSIZE64 _IOR(0x12,114,size_t)
/* get device geometry */
#define HDIO_GETGEO		0x0301

/*****************************************************************************
 * SECTION: FDASD internal types                                             *
 *****************************************************************************/
#define PARTN_MASK ((1 << DASD_PARTN_BITS) - 1)
#define USABLE_PARTITIONS ((1 << DASD_PARTN_BITS) - 1)

#define DASD_MIN_API_VERSION 0

#define DEFAULT_FDASD_CONF "/etc/fdasd.conf" /* default config file */

#define FDASD_VERSION "1.32.0"
#define DEVICE "device"
#define DISC   "disc"
#define PART   "part"

#define ALTERNATE_CYLINDERS_USED 0x10

typedef struct partition_info {
	u_int8_t used;
	unsigned long start_trk;
	unsigned long end_trk;
	unsigned long len_trk;
	unsigned long fspace_trk;
	format1_label_t *f1;
	struct partition_info *next;
	struct partition_info *prev;
	u_int8_t type;
} partition_info_t;

typedef struct config_data {
	unsigned long start;
	unsigned long stop;
} config_data_t;

typedef struct fdasd_anchor {
	int vlabel_changed;
	int vtoc_changed;
	int auto_partition;
	int print_table;
	int big_disk;
	int silent;
	int verbose;
	int devno;
	int option_reuse;
	int option_recreate;
	int partno[USABLE_PARTITIONS];
	u_int16_t dev_type;
	unsigned int used_partitions;
	unsigned long label_pos;
	unsigned int  blksize;
	unsigned long fspace_trk;
	format4_label_t  *f4;
	format5_label_t  *f5;
	format7_label_t  *f7;
	format9_label_t  *f9; /* template for all f9 labels */
	partition_info_t *first;
	partition_info_t *last;
	volume_label_t   *vlabel;
	config_data_t confdata[USABLE_PARTITIONS];
	u_int32_t hw_cylinders;
	u_int32_t formatted_cylinders;
	struct fdasd_hd_geometry geo;
	unsigned int label_block;
	unsigned int FBA_layout;
	bool is_file;
} fdasd_anchor_t;

enum offset {lower, upper};

enum fdasd_failure {
	unable_to_open_disk,
	unable_to_seek_disk,
	unable_to_read_disk,
	read_only_disk,
	unable_to_ioctl,
	api_version_mismatch,
	wrong_disk_type,
	wrong_disk_format,
	disk_in_use,
	config_syntax_error,
	vlabel_corrupted,
	dsname_corrupted,
	malloc_failed,
	device_verification_failed,
	volser_not_found
};

void fdasd_cleanup (fdasd_anchor_t *anchor);
void fdasd_initialize_anchor (fdasd_anchor_t * anc);
int fdasd_get_geometry (const PedDevice *dev, fdasd_anchor_t *anc, int fd);
void fdasd_check_api_version (fdasd_anchor_t *anc, int fd);
int fdasd_check_volume (fdasd_anchor_t *anc, int fd);
int fdasd_write_labels (fdasd_anchor_t *anc, int fd);
void fdasd_recreate_vtoc(fdasd_anchor_t *anc);
partition_info_t * fdasd_add_partition (fdasd_anchor_t *anc,
                                        unsigned int start, unsigned int stop);
int fdasd_prepare_labels (fdasd_anchor_t *anc, int fd) ;
void fdasd_check_volser(char *volser, int devno);
int fdasd_get_volser(fdasd_anchor_t *anc, char *volser, int fd);
void fdasd_change_volser(fdasd_anchor_t *anc, char *str);
void fdasd_reuse_vtoc(fdasd_anchor_t *anc);

#endif /* FDASD_H */
