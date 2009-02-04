/*
 * Copyright 2009 Freescale Semiconductor, Inc. All rights reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

#include <linux/module.h>
#include <linux/delay.h>

#include <dryice.h>

/*
  ideas for test cases:
	- bad args to APIs
		- NULL callback
		- out of range parameters
	- sync and async version of all APIs
		- all key lengths
		- resource locks
		- all alignments of key buffer
		- key buffer in IRAM
		- release key logic
		- check for busy from all APIs while async
		- measure turn-around
		- validate output values
		- validate return codes
	- try an actual tamper event
		- prompt user to pull a mesh jumper
		- check for status
	- random flogging of test cases
*/

#define PASS  0
#define FAIL  1
#define INVAL 2

int nr = 0;
module_param(nr, int, 0);

#define pr_test(fmt, arg...) \
	pr_info("%s(%d): " fmt, __func__, nr, ##arg)

#define UNLESS(expect, msg, arg...) \
	do { \
		if(rc != expect) { \
			pr_test(msg " (%s)\n", ##arg, di_err(rc)); \
			return FAIL; \
		} \
	} while (0)

char *di_err(di_return_t err)
{
	switch (err) {
	case DI_SUCCESS: return "DI_SUCCESS: operation was successful";
	case DI_ERR_BUSY: return "DI_ERR_BUSY: device or resource busy";
	case DI_ERR_STATE: return "DI_ERR_STATE: dryice is in incompatible state";
	case DI_ERR_INUSE: return "DI_ERR_INUSE: resource is already in use";
	case DI_ERR_UNSET: return "DI_ERR_UNSET: resource has not been initialized";
	case DI_ERR_WRITE: return "DI_ERR_WRITE: error occurred during register write";
	case DI_ERR_INVAL: return "DI_ERR_INVAL: invalid argument";
	case DI_ERR_FAIL: return "DI_ERR_FAIL: operation failed";
	case DI_ERR_HLOCK: return "DI_ERR_HLOCK: resource is hard locked";
	case DI_ERR_SLOCK: return "DI_ERR_SLOCK: resource is soft locked";
	case DI_ERR_NOMEM: return "DI_ERR_NOMEM: out of memory";
	}
	return "unknown DI error code";
}

/*
 * generate 16-bit pseudo-random numbers.
 * cannot return 0, do not seed with 0.
 */
static unsigned short rand16_state = 0x1234;
#define rand16_seed(seed)	rand16_state = (unsigned short)(seed)
static unsigned short rand16(void)
{
	int t1 = (rand16_state & (1<<15)) ? 0 : 1;
	int t2 = (rand16_state & (1<<13)) ? 1 : 0;
	int t3 = (rand16_state & (1<< 7)) ? 0 : 1;
	int t4 = (rand16_state & (1<< 2)) ? 1 : 0;
	rand16_state = (rand16_state << 1) | (t1 ^ t2 ^ t3 ^ t4);
	return rand16_state;
}

static void print_key(const char *tag, const uint8_t *key, int bits)
{
	int bytes = (bits + 7) / 8;

	pr_info("%s", tag);
	while (bytes--)
		printk("%02x", *key++);
	printk("\n");
}

static int setp_callback_state;
static void setp_callback(di_return_t rc, unsigned long cookie)
{
	if (cookie != 0xdead2bad) {
		pr_info("UNEXPECTED COOKIE\n");
	}
	setp_callback_state = (int)rc;
}

static int test_setp(void)
{
	di_return_t rc;
	uint32_t key_store[9] = {0};
	uint8_t *key_aligned = (uint8_t *)key_store;
	uint8_t *key_unaligned = (uint8_t *)key_store + 1;
	uint8_t key_ret[MAX_KEY_BYTES] = {0};
	int bits;

	pr_info("%s: entry\n", __func__);
	switch (nr) {

	case 0:	/* normal set_programmed_key tests */
		/* try releasing just to get to a known state */
		(void)dryice_release_programmed_key();

		pr_info("%s: test(%d) basic test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, 0);
		UNLESS(DI_SUCCESS, "can't set programmed key [locked???]");

		pr_info("%s: test(%d) set a key when already set\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, 0);
		UNLESS(DI_ERR_INUSE, "double set programmed key allowed");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");

		pr_info("%s: test(%d) invalid-args test: NULL key_data test\n", __func__, nr);
		rc = dryice_set_programmed_key(NULL, MAX_KEY_LEN, 0);
		UNLESS(DI_ERR_INVAL, "NULL key_data didn't fail");

		pr_info("%s: test(%d) invalid-args test: key_bits too large test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN+8, 0);
		UNLESS(DI_ERR_INVAL, "NULL key_bits too large didn't fail");

		pr_info("%s: test(%d) invalid-args test: key_bits not mult of 8 test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, 1, 0);
		UNLESS(DI_ERR_INVAL, "key_bits not mult of 8 didn't fail");

		pr_info("%s: test(%d) invalid-args test: key_bits = 0 test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, 0, 0);
		UNLESS(DI_SUCCESS, "key_bits = 0 failed");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");

		pr_info("%s: test(%d) aligned-key tests\n", __func__, nr);
		if (((uint32_t)key_aligned & 0x3) != 0) {
			pr_test("key_aligned is unaligned");
			return FAIL;
		}
		rand16_seed(0x1234);
		for (bits = 8; bits <= 256; bits += 8) {
			int r;
			for (r = 0; r < MAX_KEY_BYTES; r++)
				key_aligned[r] = rand16() & 0xff;
			pr_info("  testing key_len = %d\n", bits);
			rc = dryice_set_programmed_key(key_aligned, bits, 0);
			UNLESS(DI_SUCCESS, "can't set programmed key len=%d", bits);
			memset(key_ret, 0, MAX_KEY_BYTES);
			rc = dryice_get_programmed_key(key_ret, bits);
			UNLESS(DI_SUCCESS, "can't get programmed key len=%d", bits);
			rc = dryice_release_programmed_key();
			UNLESS(DI_SUCCESS, "can't release programmed key");
			rc = memcmp(key_aligned, key_ret, bits/8) ? DI_ERR_FAIL : DI_SUCCESS;
			if (rc == DI_ERR_FAIL) {
				print_key("    set_key=", key_aligned, bits);
				print_key("    get_key=", key_ret, bits);
			}
			UNLESS(DI_SUCCESS, "key mismatch for len=%d", bits);
		}

		pr_info("%s: test(%d) unaligned-key tests\n", __func__, nr);
		if (((uint32_t)key_unaligned & 0x3) == 0) {
			pr_test("key_unaligned is aligned");
			return FAIL;
		}
		rand16_seed(0x4321);
		for (bits = 8; bits <= 256; bits += 8) {
			int r;
			for (r = 0; r < MAX_KEY_BYTES; r++)
				key_unaligned[r] = rand16() & 0xff;
			pr_info("  testing key_len = %d\n", bits);
			rc = dryice_set_programmed_key(key_unaligned, bits, 0);
			UNLESS(DI_SUCCESS, "can't set programmed key len=%d", bits);
			memset(key_ret, 0, MAX_KEY_BYTES);
			rc = dryice_get_programmed_key(key_ret, bits);
			UNLESS(DI_SUCCESS, "can't get programmed key len=%d", bits);
			rc = dryice_release_programmed_key();
			UNLESS(DI_SUCCESS, "can't release programmed key");
			rc = memcmp(key_unaligned, key_ret, bits/8) ? DI_ERR_FAIL : DI_SUCCESS;
			if (rc == DI_ERR_FAIL) {
				print_key("    set_key=", key_aligned, bits);
				print_key("    get_key=", key_ret, bits);
			}
			UNLESS(DI_SUCCESS, "key mismatch for len=%d", bits);

		}
		return PASS;
		break;

	case 1:	/* 32-bit key test */
		/* try releasing just to get to a known state */
		(void)dryice_release_programmed_key();

		pr_info("%s: test(%d) basic test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, 0);
		UNLESS(DI_SUCCESS, "can't set programmed key [locked???]");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");

		key_store[0] = 0x01020304;
		key_store[1] = 0x05060708;
		key_store[2] = 0x09101112;
		key_store[3] = 0x13141516;
		key_store[4] = 0x17181920;
		key_store[5] = 0x21222324;
		key_store[6] = 0x25262728;
		key_store[7] = 0x29303132;
		/* 256-bit key test */
		pr_info("%s: test(%d) 32-bit key test, 256 bits\n", __func__, nr);
		rc = dryice_set_programmed_key(key_store, MAX_KEY_LEN, DI_FUNC_FLAG_WORD_KEY);
		UNLESS(DI_SUCCESS, "32-bit set_programmed_key returned error");
		rc = dryice_get_programmed_key(key_ret, MAX_KEY_LEN);
		UNLESS(DI_SUCCESS, "can't get programmed key");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");
		rc = DI_SUCCESS;
		{int i,j; for (i = 0; i < MAX_KEY_BYTES; i++) {
			j = ((i+1) / 10) * 16 + (i+1) % 10;
			if (key_ret[i] != j) rc = DI_ERR_FAIL;}
		}
		if (rc == DI_ERR_FAIL)
			print_key("    get_key=", key_ret, MAX_KEY_LEN);
		UNLESS(DI_SUCCESS, "key mismatch for 32-bit key");

		/* 32-bit key test */
		pr_info("%s: test(%d) 32-bit key test, 32 bits\n", __func__, nr);
		rc = dryice_set_programmed_key(key_store, 32, DI_FUNC_FLAG_WORD_KEY);
		UNLESS(DI_SUCCESS, "32-bit set_programmed_key returned error");
		rc = dryice_get_programmed_key(key_ret, 32);
		UNLESS(DI_SUCCESS, "can't get programmed key");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");
		rc = DI_SUCCESS;
		{int i,j; for (i = 0; i < 32/8; i++) {
			j = ((i+1) / 10) * 16 + (i+1) % 10;
			if (key_ret[i] != j) rc = DI_ERR_FAIL;}
		}
		if (rc == DI_ERR_FAIL)
			print_key("    get_key=", key_ret, MAX_KEY_LEN);
		UNLESS(DI_SUCCESS, "key mismatch for 32-bit key");

		pr_info("%s: test(%d) 32-bit key, unaligned test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_unaligned, 32, DI_FUNC_FLAG_WORD_KEY);
		UNLESS(DI_ERR_INVAL, "32-bit key misalignment not detected");

		pr_info("%s: test(%d) 32-bit key, odd length test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_unaligned, 33, DI_FUNC_FLAG_WORD_KEY);
		UNLESS(DI_ERR_INVAL, "32-bit key odd length not detected");

		return PASS;
		break;

	case 2:	/* async set_programmed_key test */
		/* try releasing just to get to a known state */
		(void)dryice_release_programmed_key();

		pr_info("%s: test(%d) basic test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, 0);
		UNLESS(DI_SUCCESS, "can't set programmed key [locked???]");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");

		pr_info("%s: test(%d) async test\n", __func__, nr);
		setp_callback_state = -1;
		rc = dryice_register_callback(setp_callback, 0xdead2bad);
		UNLESS(DI_SUCCESS, "register callback failed");
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, DI_FUNC_FLAG_ASYNC);
		UNLESS(DI_SUCCESS, "async set_programmed_key returned error");
		msleep(10);
		if (setp_callback_state == -1) {
			pr_test("callback function was not called");
			return FAIL;
		}
		rc = setp_callback_state;
		UNLESS(DI_SUCCESS, "async set_programmed_key failed");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");
		rc = dryice_register_callback(NULL, 0);
		UNLESS(DI_SUCCESS, "deregister callback failed");

		return PASS;
		break;

	case 3: /* destructive set_programmed_key soft-locking test */
		/* try releasing just to get to a known state */
		(void)dryice_release_programmed_key();

		/* we should be able to set a key to start with */
		pr_info("%s: test(%d) basic test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, 0);
		UNLESS(DI_SUCCESS, "can't set programmed key [locked???]");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");

		/* try read-locking a key */
		pr_info("%s: test(%d) set_programmed_key soft-read lock test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, DI_FUNC_FLAG_READ_LOCK);
		UNLESS(DI_SUCCESS, "can't set programmed key with soft-read lock");
		rc = dryice_get_programmed_key(key_ret, MAX_KEY_LEN);
		UNLESS(DI_ERR_SLOCK, "shouldn't be able to read soft-locked programmed key");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "soft-read locked key could not be released");

		/* try write-locking a key */
		pr_info("%s: test(%d) set_programmed_key soft-write lock test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, DI_FUNC_FLAG_WRITE_LOCK);
		UNLESS(DI_SUCCESS, "can't set programmed key with soft-write lock");
		rc = dryice_release_programmed_key();
		UNLESS(DI_ERR_SLOCK, "shouldn't be able to release a soft-write locked key");

		pr_info("RESET THE PROCESSOR TO CLEAR SOFT LOCKS!\n");

		return PASS;
		break;

	case 4: /* destructive set_programmed_key hard-locking test */
		/* try releasing just to get to a known state */
		(void)dryice_release_programmed_key();

		/* we should be able to set a key to start with */
		pr_info("%s: test(%d) basic test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, 0);
		UNLESS(DI_SUCCESS, "can't set programmed key [locked???]");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");

		/* try read-locking a key */
		pr_info("%s: test(%d) set_programmed_key hard-read lock test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, DI_FUNC_FLAG_READ_LOCK | DI_FUNC_FLAG_HARD_LOCK);
		UNLESS(DI_SUCCESS, "can't set programmed key with hard-read lock");
		rc = dryice_get_programmed_key(key_ret, MAX_KEY_LEN);
		UNLESS(DI_ERR_HLOCK, "shouldn't be able to read hard-locked programmed key");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "hard-read locked key could not be released");

		/* try write-locking a key */
		pr_info("%s: test(%d) set_programmed_key hard-write lock test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, DI_FUNC_FLAG_WRITE_LOCK | DI_FUNC_FLAG_HARD_LOCK);
		UNLESS(DI_SUCCESS, "can't set programmed key with hard-write lock");
		rc = dryice_release_programmed_key();
		UNLESS(DI_ERR_HLOCK, "shouldn't be able to release a hard-write locked key");

		pr_info("POWER-CYCLE THE BOARD TO CLEAR HARD LOCKS!\n");

		return PASS;
		break;

	default:
		return INVAL;
	}
	return PASS;
}

static int test_relp(void)
{
	di_return_t rc;
	uint32_t key_store[9] = {0};
	uint8_t *key_aligned = (uint8_t *)key_store;

	pr_info("%s: entry\n", __func__);
	switch (nr) {

	case 0:	/* normal release_programmed_key tests */
		/* try releasing just to get to a known state */
		(void)dryice_release_programmed_key();

		pr_info("%s: test(%d) basic test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, 0);
		UNLESS(DI_SUCCESS, "can't set programmed key [locked???]");

		pr_info("%s: test(%d) release a set key\n", __func__, nr);
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release a programmed key");

		pr_info("%s: test(%d) release an unset key\n", __func__, nr);
		rc = dryice_release_programmed_key();
		UNLESS(DI_ERR_UNSET, "can't release an unset key");

		pr_info("%s: test(%d) release_programmed_key with locks tested by setp\n", __func__, nr);

		return PASS;
		break;
	default:
		return INVAL;
	}
	return PASS;
}

static int test_getp(void)
{
	di_return_t rc;
	uint32_t key_store[9] = {0};
	uint8_t *key_aligned = (uint8_t *)key_store;
	uint8_t key_ret[MAX_KEY_BYTES] = {0};

	pr_info("%s: entry\n", __func__);
	switch (nr) {

	case 0:	/* normal get_programmed_key tests */
		/* try releasing just to get to a known state */
		(void)dryice_release_programmed_key();

		pr_info("%s: test(%d) get a key when not set\n", __func__, nr);
		rc = dryice_get_programmed_key(key_ret, MAX_KEY_LEN);
		UNLESS(DI_ERR_UNSET, "shouldn't be able to get an unset key");

		pr_info("%s: test(%d) basic test\n", __func__, nr);
		rc = dryice_set_programmed_key(key_aligned, MAX_KEY_LEN, 0);
		UNLESS(DI_SUCCESS, "can't set programmed key [locked???]");
		rc = dryice_get_programmed_key(key_ret, MAX_KEY_LEN);
		UNLESS(DI_SUCCESS, "can't get programmed key");

		pr_info("%s: test(%d) invalid-args test: NULL key_data test\n", __func__, nr);
		rc = dryice_get_programmed_key(NULL, MAX_KEY_LEN);
		UNLESS(DI_ERR_INVAL, "NULL key_data didn't fail");

		pr_info("%s: test(%d) invalid-args test: key_bits too large test\n", __func__, nr);
		rc = dryice_get_programmed_key(key_ret, MAX_KEY_LEN+8);
		UNLESS(DI_ERR_INVAL, "NULL key_bits too large didn't fail");

		pr_info("%s: test(%d) invalid-args test: key_bits not mult of 8 test\n", __func__, nr);
		rc = dryice_get_programmed_key(key_ret, 1);
		UNLESS(DI_ERR_INVAL, "key_bits not mult of 8 didn't fail");

		pr_info("%s: test(%d) invalid-args test: key_bits = 0 test\n", __func__, nr);
		rc = dryice_get_programmed_key(key_ret, 0);
		UNLESS(DI_SUCCESS, "key_bits = 0 failed");
		rc = dryice_release_programmed_key();
		UNLESS(DI_SUCCESS, "can't release programmed key");

		return PASS;
		break;

	default:
		return INVAL;
	}
	return PASS;
}

static int setr_done;
static void setr_callback(di_return_t rc, unsigned long cookie)
{
	setr_done++;
}

static int test_setr(void)
{
	di_return_t rc;

	switch (nr) {
	case 0:
		pr_test("test normal invocation\n");
		rc = dryice_set_random_key(0);
		UNLESS(DI_SUCCESS, "bad result from set_random_key");
		return PASS;
	case 1:
		pr_test("test invalid parameters\n");
		return PASS;
	case 2:
		pr_test("test for BUSY handling\n");
		setr_done = 0;
		rc = dryice_register_callback(setr_callback, 0);
		UNLESS(DI_SUCCESS, "bad result from register_callback");

		rc = dryice_select_key(DI_KEY_FK, DI_FUNC_FLAG_ASYNC);
		UNLESS(DI_SUCCESS, "bad result from select_key");

		rc = dryice_set_random_key(0);
		UNLESS(DI_ERR_BUSY, "expected BUSY error");

		msleep(2);	/* wait for completion */

		if(!setr_done) {
			pr_test("async call incomplete\n");
			return FAIL;
		}
		return PASS;
	case 3:
		pr_test("test async mode\n");

		setr_done = 0;
		rc = dryice_register_callback(setr_callback, 0);
		UNLESS(DI_SUCCESS, "bad result from register_callback");

		rc = dryice_set_random_key(DI_FUNC_FLAG_ASYNC);
		UNLESS(DI_SUCCESS, "bad result from set_random_key");

		msleep(2);	/* wait for completion */

		if(!setr_done) {
			pr_test("async call incomplete\n");
			return FAIL;
		}
		return PASS;
	case 4:
		pr_test("with write lock (hard reset required after)\n");

		rc = dryice_set_random_key(DI_FUNC_FLAG_WRITE_LOCK);
		UNLESS(DI_SUCCESS, "bad result from set_random_key");

		rc = dryice_set_random_key(0);
		UNLESS(DI_ERR_SLOCK, "key should be soft locked");
		pr_info("RESET THE PROCESSOR TO CLEAR SOFT LOCKS!\n");

		return PASS;
	default:
		return INVAL;
	}
	return FAIL;
}

static int sel_done;
static void sel_callback(di_return_t rc, unsigned long cookie)
{
	sel_done++;
}

static int test_sel(void)
{
	di_return_t rc;
	int i;

	switch (nr) {
	case 0:
		pr_test("test normal invocation\n");

		/* try releasing just to get to a known state */
		(void)dryice_release_key_selection();

		rc = dryice_select_key(DI_KEY_FK, 0);
		UNLESS(DI_SUCCESS, "bad result from key selection");

		rc = dryice_release_key_selection();
		UNLESS(DI_SUCCESS, "unable to release key selection");

		return PASS;
	case 1:
		pr_test("test invalid parameters\n");

		/* try releasing just to get to a known state */
		(void)dryice_release_key_selection();

		for(i = -10; i < 20; i++) {
			rc = dryice_select_key(i, 0);
			if (i >= DI_KEY_FK && i <= DI_KEY_FRK) {
				/* in range, expect success */
				UNLESS(DI_SUCCESS, "bad result for in-range key index");
				rc = dryice_release_key_selection();
				UNLESS(DI_SUCCESS, "unable to release key selection");
			} else {
				/* out of range, expect inval */
				UNLESS(DI_ERR_INVAL, "bad result for out-of-range key index");
			}
		}
		return PASS;
	case 2:
		pr_test("test for BUSY handling\n");

		/* try releasing just to get to a known state */
		(void)dryice_release_key_selection();

		sel_done = 0;
		rc = dryice_register_callback(sel_callback, 0);
		UNLESS(DI_SUCCESS, "bad result from register_callback");

		rc = dryice_select_key(DI_KEY_FK, DI_FUNC_FLAG_ASYNC);
		UNLESS(DI_SUCCESS, "bad result from select_key");

		rc = dryice_select_key(DI_KEY_FK, 0);
		UNLESS(DI_ERR_BUSY, "expected BUSY error");

		msleep(2);	/* wait for completion */

		if(!sel_done) {
			pr_test("async call incomplete\n");
			return FAIL;
		}
		return PASS;
	case 3:
		pr_test("test async mode\n");

		/* try releasing just to get to a known state */
		(void)dryice_release_key_selection();

		sel_done = 0;
		rc = dryice_register_callback(sel_callback, 0);
		UNLESS(DI_SUCCESS, "bad result from register_callback");

		rc = dryice_select_key(DI_KEY_FK, DI_FUNC_FLAG_ASYNC);
		UNLESS(DI_SUCCESS, "bad result from select_key");

		msleep(2);	/* wait for completion */

		if(!sel_done) {
			pr_test("async call incomplete\n");
			return FAIL;
		}
		return PASS;
	case 4:
		pr_test("select with write lock (hard reset required after)\n");

		/* try releasing just to get to a known state */
		(void)dryice_release_key_selection();

		rc = dryice_select_key(DI_KEY_FK, DI_FUNC_FLAG_WRITE_LOCK);
		UNLESS(DI_SUCCESS, "bad result from select_key");

		rc = dryice_release_key_selection();
		UNLESS(DI_ERR_SLOCK, "key should be soft locked");

		rc = dryice_select_key(DI_KEY_FK, 0);
		UNLESS(DI_ERR_INUSE, "key should be soft locked");
		pr_info("RESET THE PROCESSOR TO CLEAR SOFT LOCKS!\n");

		return PASS;
	case 5:
		pr_test("select with hard lock (power-cycle required after)\n");

		/* try releasing just to get to a known state */
		(void)dryice_release_key_selection();

		rc = dryice_select_key(DI_KEY_FK, DI_FUNC_FLAG_WRITE_LOCK | DI_FUNC_FLAG_HARD_LOCK);
		UNLESS(DI_SUCCESS, "bad result from select_key");

		rc = dryice_release_key_selection();
		UNLESS(DI_ERR_HLOCK, "key should be hard locked");

		rc = dryice_select_key(DI_KEY_FK, 0);
		UNLESS(DI_ERR_INUSE, "key should be hard locked");
		pr_info("POWER-CYCLE THE BOARD TO CLEAR HARD LOCKS!\n");

		return PASS;
	default:
		return INVAL;
	}
	return FAIL;
}

static int test_rel(void)
{
	pr_info("%s: entry\n", __func__);
	return PASS;
}

static int test_chk(void)
{
	di_return_t rc;
	di_key_t key;

	switch (nr) {
	case 0:
		pr_test("test normal invocation\n");

		/* try releasing just to get to a known state */
		(void)dryice_release_key_selection();

		rc = dryice_select_key(DI_KEY_FK, 0);
		UNLESS(DI_SUCCESS, "bad result from select_key");

		rc = dryice_check_key(&key);
		UNLESS(DI_SUCCESS, "error checking key");

		if (key != DI_KEY_FK) {
			pr_test("expected key not selected\n");
			return FAIL;
		}
		return PASS;
	case 1:
		pr_test("test invalid parameters\n");
		rc = dryice_check_key(NULL);
		UNLESS(DI_ERR_INVAL, "bad result for NULL key pointer");
		return PASS;
	case 2:
		pr_test("test for BUSY handling\n");
		sel_done = 0;

		/* try releasing just to get to a known state */
		(void)dryice_release_key_selection();

		rc = dryice_register_callback(sel_callback, 0);
		UNLESS(DI_SUCCESS, "bad result from register_callback");

		rc = dryice_select_key(DI_KEY_FK, DI_FUNC_FLAG_ASYNC);
		UNLESS(DI_SUCCESS, "bad result from select_key");

		rc = dryice_check_key(&key);
		UNLESS(DI_ERR_BUSY, "expected BUSY error");

		msleep(2);	/* wait for completion */

		if(!sel_done) {
			pr_test("async call incomplete\n");
			return FAIL;
		}

		rc = dryice_release_key_selection();
		UNLESS(DI_SUCCESS, "unable to release key selection");

		return PASS;
	default:
		return INVAL;
	}
	return FAIL;
}

static int test_tamp(void)
{
	uint32_t events;
	uint32_t timestamp;
	di_return_t rc;
	int tries = 0;

	rc = dryice_get_tamper_event(&events, &timestamp, 0);
	if (rc == DI_SUCCESS) {
		pr_test("tamper event=0x%08x timestamp=%d\n", events, timestamp);
		pr_test("Dryice already in failure state.\n");
		pr_test("Your board needs to be power-cycled to run this test.\n");
		return FAIL;
	}

	pr_info("%s: Waiting 10 seconds for you to cause a tamper event...\n", __func__);
	for (;;) {
		rc = dryice_get_tamper_event(&events, &timestamp, 0);
		switch (rc) {
		case DI_SUCCESS:
			if (events == 0) {
				pr_test("dryice in failure state w/out tamper events\n");
				return FAIL;
			}
			pr_test("got tamper event=0x%08x timestamp=%d\n", events, timestamp);
			pr_test("YOU MUST NOW POWER-CYCLE THE BOARD TO RUN OTHER TESTS.\n");
			return PASS;
		case DI_ERR_STATE:
			break;
		default:
			UNLESS(0, "unexpected result from get_tamper_event");
		}
		if (tries++ > 10 * 100) {
			pr_test("timed out waiting for tamper event\n");
			return FAIL;
		}
		msleep(10);
	}

	return PASS;
}

static int busy_done;
static void busy_callback(di_return_t rc, unsigned long cookie)
{
	busy_done++;
}

static int test_busy(void)
{
	di_return_t rc;

	switch (nr) {
	case 0:
		pr_test("test all interfaces for BUSY handling\n");

		/* try releasing just to get to a known state */
		(void)dryice_release_key_selection();

		busy_done = 0;
		rc = dryice_register_callback(busy_callback, 0);
		UNLESS(DI_SUCCESS, "bad result from register_callback");

		rc = dryice_select_key(DI_KEY_FK, DI_FUNC_FLAG_ASYNC);
		UNLESS(DI_SUCCESS, "bad result from select_key");

		rc = dryice_set_programmed_key(NULL, 0, 0);
		UNLESS(DI_ERR_BUSY, "dryice_set_programmed_key: expected BUSY error");

		rc = dryice_get_programmed_key(NULL, 0);
		UNLESS(DI_ERR_BUSY, "dryice_get_programmed_key: expected BUSY error");

		rc = dryice_release_programmed_key();
		UNLESS(DI_ERR_BUSY, "dryice_release_programmed_key: expected BUSY error");

		rc = dryice_set_random_key(0);
		UNLESS(DI_ERR_BUSY, "dryice_set_random_key: expected BUSY error");

		rc = dryice_select_key(0, 0);
		UNLESS(DI_ERR_BUSY, "dryice_select_key: expected BUSY error");

		rc = dryice_check_key(NULL);
		UNLESS(DI_ERR_BUSY, "dryice_check_key: expected BUSY error");

		rc = dryice_release_key_selection();
		UNLESS(DI_ERR_BUSY, "dryice_release_key_selection: expected BUSY error");

		rc = dryice_get_tamper_event(NULL, NULL, 0);
		UNLESS(DI_ERR_BUSY, "dryice_get_tamper_event: expected BUSY error");

		rc = dryice_register_callback(NULL, 0);
		UNLESS(DI_ERR_BUSY, "dryice_register_callback: expected BUSY error");

		msleep(2);	/* wait for completion */

		if(!busy_done) {
			pr_test("async call incomplete\n");
			return FAIL;
		}
		rc = dryice_release_key_selection();
		UNLESS(DI_SUCCESS, "unable to release key selection");

		return PASS;
	default:
		return INVAL;
	}
	return FAIL;
}


static int help(void);

struct test {
	char *name;
	int (*func)(void);
} tests[] = {
	{"help", help},
	{"setp", test_setp},
	{"relp", test_relp},
	{"getp", test_getp},
	{"setr", test_setr},
	{"sel", test_sel},
	{"rel", test_rel},
	{"chk", test_chk},
	{"tamp", test_tamp},
	{"busy", test_busy},
};
#define NR_TESTS ARRAY_SIZE(tests)

static int help(void)
{
	int i;
	struct test *tp = tests;

	pr_info("Available tests:\n   ");
	for (i = 0; i < NR_TESTS; i++) {
		printk(" %s", tp->name);
		tp++;
	}
	printk("\n\n");

	return PASS;
}

static int do_test(struct test *tp)
{
	int rc;

	pr_info("EXECUTE %s(%d)\n", tp->name, nr);
	rc = tp->func();
	switch (rc) {
	case PASS:
		pr_info("RESULT %s(%d): PASS\n", tp->name, nr);
		break;
	case FAIL:
		pr_info("RESULT %s(%d): FAIL\n", tp->name, nr);
		break;
	case INVAL:
		pr_info("INVALID %s(%d)\n", tp->name, nr);
		break;
	}
	return rc;
}

char *test = "help";
module_param(test, charp, 0);
static int __init dryice_test_init(void)
{
	int i;
	struct test *tp = tests;

	pr_info("Good morning from %s\n", __func__);
	pr_info("Test=%s(%d)\n", test, nr);

	for (i = 0; i < NR_TESTS; i++) {
		if(strcmp(test, tp->name) == 0) {
			int rc = do_test(tp);
			return -rc;
		}
		tp++;
	}
	pr_info("Unknown test \"%s\"\n", test);
	help();

	return -EINVAL;
}

static void __exit dryice_test_exit(void)
{
	pr_info("Have a great day from %s\n", __func__);
}

module_init(dryice_test_init);
module_exit(dryice_test_exit);

MODULE_DESCRIPTION("Test Module for DryIce");
MODULE_LICENSE("GPL");
