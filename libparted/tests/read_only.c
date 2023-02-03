#include <config.h>
#include <unistd.h>

#include <check.h>

#include <parted/parted.h>

#include "common.h"
#include "progname.h"

#define STREQ(a, b) (strcmp (a, b) == 0)

static char* temporary_disk;

static void
create_disk (void)
{
        temporary_disk = _create_disk (80 * 1024 * 1024);
        ck_assert_msg(temporary_disk != NULL, "Failed to create temporary disk");
}

static void
destroy_disk (void)
{
        unlink (temporary_disk);
        free (temporary_disk);
}

/* TEST: Test reading gpt disk partitin in read_only mode */
START_TEST (test_read_only_gpt)
{
        PedDevice* dev = ped_device_get (temporary_disk);
        if (dev == NULL)
                return;

        PedDisk* disk = ped_disk_new_fresh (dev, ped_disk_type_get ("gpt"));
        PedConstraint *constraint = ped_constraint_any (dev);
        PedPartition *part = ped_partition_new (disk, PED_PARTITION_NORMAL,
            ped_file_system_type_get("ext4"), 2048, 4096);
        ped_partition_set_name(part, "read-only-test");
        ped_disk_add_partition (disk, part, constraint);
        ped_disk_commit (disk);
        ped_constraint_destroy (constraint);

        ped_disk_destroy (disk);
        ped_device_destroy (dev);

        // Now open it with read only mode
        dev = ped_device_get (temporary_disk);
        if (dev == NULL)
                return;

        dev->read_only = 1;
        disk = ped_disk_new(dev);
        ck_assert_ptr_nonnull(disk);
        part = ped_disk_get_partition(disk, 1);
        ck_assert_ptr_nonnull(disk);
        ck_assert_str_eq(ped_partition_get_name(part), "read-only-test");
        ck_assert_int_eq(part->geom.start, 2048);
        ck_assert_int_eq(part->geom.length, 2049);
        ck_assert_int_eq(part->geom.end, 4096);
        ped_disk_destroy (disk);
        ped_device_destroy (dev);
}
END_TEST

/* TEST: Test reading msdos disk partitin in read_only mode */
START_TEST (test_read_only_msdos)
{
        PedDevice* dev = ped_device_get (temporary_disk);
        if (dev == NULL)
                return;

        PedDisk* disk = ped_disk_new_fresh (dev, ped_disk_type_get ("msdos"));
        PedConstraint *constraint = ped_constraint_any (dev);
        PedPartition *part = ped_partition_new (disk, PED_PARTITION_NORMAL,
            ped_file_system_type_get("ext4"), 2048, 4096);
        ped_disk_add_partition (disk, part, constraint);
        ped_disk_commit (disk);
        ped_constraint_destroy (constraint);
        ped_disk_destroy (disk);
        ped_device_destroy (dev);

        // Now open it with read only mode
        dev = ped_device_get (temporary_disk);
        if (dev == NULL)
                return;

        dev->read_only = 1;
        disk = ped_disk_new(dev);
        ck_assert_ptr_nonnull(disk);
        part = ped_disk_get_partition(disk, 1);
        ck_assert_ptr_nonnull(disk);
        ck_assert_int_eq(part->geom.start, 2048);
        ck_assert_int_eq(part->geom.length, 2049);
        ck_assert_int_eq(part->geom.end, 4096);
        ped_disk_destroy (disk);
        ped_device_destroy (dev);
}
END_TEST

int
main (int argc, char **argv)
{
        set_program_name (argv[0]);
        int number_failed;
        Suite* suite = suite_create ("Partition Flags");
        TCase* tcase_gpt = tcase_create ("GPT");
        TCase* tcase_msdos = tcase_create ("MSDOS");

        /* Fail when an exception is raised */
        ped_exception_set_handler (_test_exception_handler);

        tcase_add_checked_fixture (tcase_gpt, create_disk, destroy_disk);
        tcase_add_test (tcase_gpt, test_read_only_gpt);
        /* Disable timeout for this test */
        tcase_set_timeout (tcase_gpt, 0);
        suite_add_tcase (suite, tcase_gpt);

        tcase_add_checked_fixture (tcase_msdos, create_disk, destroy_disk);
        tcase_add_test (tcase_msdos, test_read_only_msdos);
        /* Disable timeout for this test */
        tcase_set_timeout (tcase_msdos, 0);
        suite_add_tcase (suite, tcase_msdos);

        SRunner* srunner = srunner_create (suite);
        srunner_run_all (srunner, CK_VERBOSE);

        number_failed = srunner_ntests_failed (srunner);
        srunner_free (srunner);

        return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
