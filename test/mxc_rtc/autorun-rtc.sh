#!/bin/bash

source /unit_tests/test-utils.sh

#
# Exit status is 0 for PASS, nonzero for FAIL
#
STATUS=0

echo "$(platform)"
if [ "$(platform)" != IMX31ADS ] && [ "$(platform)" != IMX32ADS ] \
    && [ "$(platform)" != IMX25_3STACK ] && [ "$(platform)" != IMX35_3STACK ]; then
	rtc_test_param=--no-periodic
	# For kernel version 2.6.38 and higher, number of interrupts expected will be 11
	VER=`echo $(kernel_version) | cut -d. -f3 | cut -d- -f1`
	REF_VER=38
	if [ $VER -ge $REF_VER ]; then
		RTC_IRQS_EXPECTED=11
	else
		RTC_IRQS_EXPECTED=1
	fi
else
	rtc_test_param=--full
	RTC_IRQS_EXPECTED=131
fi

# devnode test
check_devnode "/dev/rtc0"

# check rtc interrupts
RTC_IRQS_BEFORE=$( cat /proc/interrupts |grep rtc|sed -r 's,.*: *([0-9]*) .*,\1,' )

# RTC test cases
run_testcase "./rtctest.out $rtc_test_param"

RTC_IRQS_AFTER=$( cat /proc/interrupts |grep rtc|sed -r 's,.*: *([0-9]*) .*,\1,' )

RTC_IRQS=$(( $RTC_IRQS_AFTER - $RTC_IRQS_BEFORE ))

echo "rtc irqs before running unit test: $RTC_IRQS_BEFORE"
echo "rtc irqs after running unit test:  $RTC_IRQS_AFTER"
echo "so rtc irqs during test was:       $RTC_IRQS"

if [ "$RTC_IRQS" != "$RTC_IRQS_EXPECTED" ]; then
	echo "checking rtc interrupts FAIL, expected $RTC_IRQS_EXPECTED interrupts, got $RTC_IRQS"
	STATUS=1
else
	echo "checking rtc interrupts PASS"
       #RTC wait for time set notification test
	if [ "$(platform)" == IMX50 ] || [ "$(platform)" == IMX51 ] || [ "$(platform)" == IMX53 ]; then
          run_testcase "./rtc_timesetnotification_test.out"
      fi
fi

print_status
exit $STATUS
