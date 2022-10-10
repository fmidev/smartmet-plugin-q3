/*
 * CONFIG.H                       Copyright (c) 2008-2010, Ilmatieteen laitos
 *
 * Compile-time configurations
 */
#ifndef CONFIG_H
#define CONFIG_H

//---
// Configure behaviour so that 'validtime', 'projection' and 'gridsize' globals
// are used also in the functional API ('.grid{ ... }'), if similarily named
// fields are missing.
//
// Even without this, they are used for variable API s.a. 'HIR.T'
//
#define GLOBAL_DEFAULTS_IN_GRID

//---
// Enable 'Z' param (in SQD data) to first look for param #2 (GeopHeight) and if
// not there, #3 (GeomHeight).
//
// Note that these two parameters actually carry different heights; #2 height
// from geo potential (the round-ish globe), #3 height from ground level (though
// this has not been fully confirmed - who knows exactly?). Maybe #3 should not
// ever be used, anyways? TBD
//
// 18-Oct-2011 PKi: The primary/secondary parameter is defined by
// SQD_PRIMARY_Z/SQD_SECONDARY_Z
//
#define SQD_CASCADE_Z_ENABLED
#define SQD_PRIMARY_Z kFmiGeopHeight
#define SQD_SECONDARY_Z kFmiGeomHeight

//--- 18-Oct-2011 PKi
//
// Enable 'W' param (in SQD data) to first look for param #43
// (kFmiVerticalVelocityMMS) and if not there, #39 (kFmiVelocityPotential).
//
// The primary/secondary parameter is defined by SQD_PRIMARY_W/SQD_SECONDARY_W
//
#define SQD_CASCADE_W_ENABLED
#define SQD_PRIMARY_W kFmiVerticalVelocityMMS
#define SQD_SECONDARY_W kFmiVelocityPotential

//---
// Enable 'testraw' for testing purposes (creates certain pre-known data and
// allows testing of time interpolations).
//
#ifndef METQU
#define USE_TESTRAW
#endif

//---
// Demand Z parameter for data requested with 'height=true'
//
#define HEIGHT_TRUE_PULLS_IN_Z

//---
// true:  Tracks only return a 'raw' data entry, guaranteed to be non-nil.
//        If search criteria is not met, an error is produced.
//          <<
//              local r= HIR{ hpa=900 }
//          <<
//
// false: Tracks return 'track, err_str', letting the caller handle errors:
//          <<
//              local r,err= HIR{ hpa=900 }
//              assert(r,err)
//          <<
//
// Reason to always generate errors is easier coding (no need to test for
// missing data) and that no other part of q3 API uses the 'val, err' return
// method (otherwise commonly used idiom in Lua).
//
// Reason to let the calling script handle errors is giving it more control.
// Being able to test if certain criteria is met and do something else if not.
//
// Note: We could have a calling param (i.e. 'error=false') to allow also the
// calling
//      handling of errors (best of both worlds). Or the calling script can use
//      'pcall()' wrapping around the call to catch the errors (standard Lua
//      construct).
//
// NOTE: THIS CONFIG CURRENTLY ONLY AFFECTS THE SERVER. Metqu command line tool
// uses
//      the 'nil,err' return idiom. --AKa 9-Dec-2010
//
#define CONFIG_TRACK_ERRORS_ENABLED

//---
// Is flight level handling taken completely outside of the 'q3' API or do
// functions present 'flight=true' and 'flight=uint' API?
//
// Reason to have the handling outside is that flight levels are merely pressure
// levels presented with a function. That's it.
//
// Reason to have the handling inside is convenience.
//
#define CONFIG_FLIGHT_LEVELS_API

//---
// Enable binary output of scalar matrices?
//
// This is a compatibility feature with q2 server.
//
// Note: DO NOT DISABLE THIS. Smartmet editor uses binary fetches and would
// seize to work
//      if this is disabled.
//
// Note: This only affects the server product (not the command line tool).
//
#define CONFIG_BINARY_OUTPUT_ENABLED

//---
// METQU only:
//
// When Raw data is created from scratch, with standard param names (i.e. "T",
// "N"), shall the standard names be copied to the data (as native names, i.e.
// "T:4") or the native name parts left empty (i.e. ":4").
//
#if (defined METQU) && (defined USE_NEWBASE)
#define CONFIG_APPLY_STANDARD_NAMES_TO_SQD
#endif

#endif
// CONFIG_H
