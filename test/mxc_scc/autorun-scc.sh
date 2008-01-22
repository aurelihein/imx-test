#!/bin/sh

PATH=$PATH:/unit_tests/modules:$PWD
source /unit_tests/test-utils.sh

#
# Exit status is 0 for PASS, nonzero for FAIL
#
STATUS=0

# Inserting the SCC Module.
insmod_test /lib/modules/$(kernel_version)/test/scc_test_driver.ko

run_testcase ./encrypt_decrypt_tests.sh
run_testcase ./key_slot_tests.sh
run_testcase ./fail_mode_tests.sh

print_status
exit $STATUS
