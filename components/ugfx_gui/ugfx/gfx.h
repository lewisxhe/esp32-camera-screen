/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

/**
 * @file    gfx.h
 * @brief   GFX system header file.
 *
 * @addtogroup GFX
 *
 * @brief	Main module to glue all the others together
 *
 * @{
 */

#ifndef _GFX_H
#define _GFX_H

// Everything here is C, not C++
#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------ Initial preparation ---------------------------------

// ------------------------------ Load the user configuration ---------------------------------

// Definitions for option configuration
#define GFXOFF		(0)
#define GFXON		(-1)

// gfxconf.h is the user's project configuration for the GFX system.
#include "gfxconf.h"

// ------------------------------ Determine build environment info - COMPILER, CPU etc ---------------------------------

/**
 * @name    GFX compatibility options
 * @{
 */
	/**
	 * @brief   Include the uGFX V2.x API
	 * @details	Defaults to GFXON
	 */
	#ifndef GFX_COMPAT_V2
		#define GFX_COMPAT_V2				GFXON
	#endif
/** @} */

#if GFX_COMPAT_V2
	// These need to be defined here for compatibility with V2.x user config files
	#if !defined(FALSE)
		#define FALSE       0
	#endif
	#if !defined(TRUE)
		#define TRUE        -1
	#endif
#endif

// Macro concatination and strify - not API documented
#define GFXCAT(a, b)	a ## b
#define GFXCATX(a, b)	GFXCAT(a, b)
#define GFXSTR(a)		#a
#define GFXSTRX(a)		GFXSTR(a)

// Include Compiler and CPU support
#include "src/gfx_compilers.h"

// ------------------------------ Enumerate all options ---------------------------------
/**
 * @name    GFX sub-systems that can be turned on
 * @{
 */
	/**
	 * @brief   GFX Driver API
	 * @details	Defaults to GFXON
	 * @note	Not much useful can be done without a driver
	 */
	#ifndef GFX_USE_GDRIVER
		#define GFX_USE_GDRIVER	GFXON
	#endif
	/**
	 * @brief   GFX Graphics Display Basic API
	 * @details	Defaults to GFXOFF
	 * @note	Also add the specific hardware driver to your makefile.
	 * 			Eg.  include $(GFXLIB)/drivers/gdisp/Nokia6610/driver.mk
	 */
	#ifndef GFX_USE_GDISP
		#define GFX_USE_GDISP	GFXOFF
	#endif
	/**
	 * @brief   GFX Graphics Windowing API
	 * @details	Defaults to GFXOFF
	 * @details	Extends the GDISP API to add the concept of graphic windows.
	 * @note	Also supports high-level "window" objects such as console windows,
	 * 			buttons, graphing etc
	 */
	#ifndef GFX_USE_GWIN
		#define GFX_USE_GWIN	GFXOFF
	#endif
	/**
	 * @brief   GFX Event API
	 * @details	Defaults to GFXOFF
	 * @details	Defines the concept of a "Source" that can send "Events" to "Listeners".
	 */
	#ifndef GFX_USE_GEVENT
		#define GFX_USE_GEVENT	GFXOFF
	#endif
	/**
	 * @brief   GFX Timer API
	 * @details	Defaults to GFXOFF
	 * @details	Provides thread context timers - both one-shot and periodic.
	 */
	#ifndef GFX_USE_GTIMER
		#define GFX_USE_GTIMER	GFXOFF
	#endif
	/**
	 * @brief   GFX Queue API
	 * @details	Defaults to GFXOFF
	 * @details	Provides queue management.
	 */
	#ifndef GFX_USE_GQUEUE
		#define GFX_USE_GQUEUE	GFXOFF
	#endif
	/**
	 * @brief   GFX Input Device API
	 * @details	Defaults to GFXOFF
	 * @note	Also add the specific hardware drivers to your makefile.
	 * 			Eg.
	 * 				include $(GFXLIB)/drivers/ginput/toggle/Pal/driver.mk
	 * 			and...
	 * 				include $(GFXLIB)/drivers/ginput/touch/MCU/driver.mk
	 */
	#ifndef GFX_USE_GINPUT
		#define GFX_USE_GINPUT	GFXOFF
	#endif
	/**
	 * @brief   GFX Generic Periodic ADC API
	 * @details	Defaults to GFXOFF
	 */
	#ifndef GFX_USE_GADC
		#define GFX_USE_GADC	GFXOFF
	#endif
	/**
	 * @brief   GFX Audio API
	 * @details	Defaults to GFXOFF
	 * @note	Also add the specific hardware drivers to your makefile.
	 * 			Eg.
	 * 				include $(GFXLIB)/drivers/gaudio/GADC/driver.mk
	 */
	#ifndef GFX_USE_GAUDIO
		#define GFX_USE_GAUDIO	GFXOFF
	#endif
	/**
	 * @brief   GFX Miscellaneous Routines API
	 * @details	Defaults to GFXOFF
	 * @note	Turning this on without turning on any GMISC_NEED_xxx macros will result
	 * 			in no extra code being compiled in. GMISC is made up from the sum of its
	 * 			parts.
	 */
	#ifndef GFX_USE_GMISC
		#define GFX_USE_GMISC	GFXOFF
	#endif
	/**
	 * @brief   GFX File API
	 * @details	Defaults to GFXOFF
	 */
	#ifndef GFX_USE_GFILE
		#define GFX_USE_GFILE	GFXOFF
	#endif
	/**
	 * @brief   GFX Translation Support API
	 * @details	Defaults to GFXOFF
	 */
	#ifndef GFX_USE_GTRANS
		#define GFX_USE_GTRANS	GFXOFF
	#endif
/** @} */

/**
 * @name    GFX compatibility options
 * @{
 */
	/**
	 * @brief   Include the uGFX V2.x Old Colors
	 * @details	Defaults to GFXON
	 * @pre		Requires GFX_COMPAT_V2 to be GFXON
	 * @note	The old color definitions (particularly Red, Green & Blue) can
	 *			cause symbol conflicts with many platforms eg Win32, STM32 HAL etc.
	 *			Although officially these symbols are part of the V2.x API, this
	 *			option allows them to be turned off even when the rest of the V2.x
	 *			API is turned on.
	 */
	#ifndef GFX_COMPAT_OLDCOLORS
		#define GFX_COMPAT_OLDCOLORS		GFXON
	#endif
/** @} */

/**
 * Get all the options for each sub-system.
 *
 */
#include "src/gos/gos_options.h"
#include "src/gdriver/gdriver_options.h"
#include "src/gfile/gfile_options.h"
#include "src/gmisc/gmisc_options.h"
#include "src/gtrans/gtrans_options.h"
#include "src/gqueue/gqueue_options.h"
#include "src/gevent/gevent_options.h"
#include "src/gtimer/gtimer_options.h"
#include "src/gdisp/gdisp_options.h"
#include "src/gwin/gwin_options.h"
#include "src/ginput/ginput_options.h"
#include "src/gadc/gadc_options.h"
#include "src/gaudio/gaudio_options.h"

// ------------------------------ Load driver configuration ---------------------------------

// ------------------------------ Apply configuration rules ---------------------------------

/**
 * Interdependency safety checks on the sub-systems.
 * These must be in dependency order.
 *
 */
#ifndef GFX_DISPLAY_RULE_WARNINGS
	#define GFX_DISPLAY_RULE_WARNINGS	GFXOFF
#endif
#include "src/gwin/gwin_rules.h"
#include "src/ginput/ginput_rules.h"
#include "src/gdisp/gdisp_rules.h"
#include "src/gaudio/gaudio_rules.h"
#include "src/gadc/gadc_rules.h"
#include "src/gevent/gevent_rules.h"
#include "src/gtimer/gtimer_rules.h"
#include "src/gqueue/gqueue_rules.h"
#include "src/gmisc/gmisc_rules.h"
#include "src/gtrans/gtrans_rules.h"
#include "src/gfile/gfile_rules.h"
#include "src/gdriver/gdriver_rules.h"
#include "src/gos/gos_rules.h"

// ------------------------------ Define API definitions ---------------------------------

/**
 *  Include the sub-system header files
 */
#include "src/gos/gos.h"
//#include "src/gdriver/gdriver.h"			// This module is only included by source that needs it.
#include "src/gfile/gfile.h"
#include "src/gmisc/gmisc.h"
#include "src/gtrans/gtrans.h"
#include "src/gqueue/gqueue.h"
#include "src/gevent/gevent.h"
#include "src/gtimer/gtimer.h"
#include "src/gdisp/gdisp.h"
#include "src/gwin/gwin.h"
#include "src/ginput/ginput.h"
#include "src/gadc/gadc.h"
#include "src/gaudio/gaudio.h"

/**
 * @brief	The one call to start it all
 *
 * @note	This will initialise each sub-system that has been turned on.
 * 			For example, if GFX_USE_GDISP is defined then display will be initialised
 * 			and cleared to black.
 * @note	If you define GFX_OS_NO_INIT as GFXON in your gfxconf.h file then ugfx doesn't try to
 * 			initialise the operating system for you when you call @p gfxInit().
 * @note	If you define GFX_OS_EXTRA_INIT_FUNCTION in your gfxconf.h file the macro is the
 * 			name of a void function with no parameters that is called immediately after
 * 			operating system initialisation (whether or not GFX_OS_NO_INIT is set).
 * @note	If you define GFX_OS_EXTRA_DEINIT_FUNCTION in your gfxconf.h file the macro is the
 * 			name of a void function with no parameters that is called immediately before
 * 			operating system de-initialisation (as ugfx is exiting).
 * @note	If GFX_OS_CALL_UGFXMAIN is set uGFXMain() is called after all initialisation is complete.
 *
 * @api
 */
void gfxInit(void);

/**
 * @brief	The one call to end it all
 *
 * @note	This will de-initialise each sub-system that has been turned on.
 *
 * @api
 */
void gfxDeinit(void);

#if GFX_OS_CALL_UGFXMAIN || defined(__DOXYGEN__)
	/**
	 * @brief	The function containing all the user uGFX application code.
	 *
	 * @note	This is called by gfxInit() and is expected to never return.
	 * 			It is defined by the user.
	 *
	 * @pre		GFX_OS_CALL_UGFXMAIN is GFXON
	 */
	void uGFXMain(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* _GFX_H */
/** @} */

