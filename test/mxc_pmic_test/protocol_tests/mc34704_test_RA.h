/*
 * Copyright 2008 Freescale Semiconductor, Inc. All Rights Reserved.
 */

/*
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 */

/*!
 * @file   mc34704_test_RA.h
 * @brief  Test scenario C header PMIC.
 */

#ifndef MC34704_TEST_RA_H
#define MC34704_TEST_RA_H

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
                                         INCLUDE FILES
==============================================================================*/

/*==============================================================================
                                           CONSTANTS
==============================================================================*/

/*==============================================================================
                                       DEFINES AND MACROS
==============================================================================*/

/*==============================================================================
                                             ENUMS
==============================================================================*/

/*==============================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
==============================================================================*/

/*==============================================================================
                                 GLOBAL VARIABLE DECLARATIONS
==============================================================================*/

/*==============================================================================
                                     FUNCTION PROTOTYPES
==============================================================================*/
	int VT_mc34704_RA_setup();
	int VT_mc34704_RA_cleanup();

	int VT_mc34704_test_RA(void);

#ifdef __cplusplus
}
#endif
#endif				// MC34704_TEST_RA_H //
