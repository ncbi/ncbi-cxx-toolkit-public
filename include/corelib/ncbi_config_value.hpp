#ifndef CORELIB___NCBI_CONFIG_VALUE__HPP
#define CORELIB___NCBI_CONFIG_VALUE__HPP

/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Authors:  Eugene Vasilchenko
 *
 */

/// @file ncbi_config_value.hpp
///   Defines GetConfigXxx() functions.
///   These functions check first application configuration file,
///   then runtime environment to retrieve configuration values.

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

/** @addtogroup ModuleConfig
 *
 * @{
 */

/// Get string configuration value.
///
/// @param section
///   Check application configuration named section first if not null.
/// @param variable
///   Variable name within application section.
///   If no value found in configuration file, environment variable with
///   name section_variable or variable will be checked, depending on
///   wether section is null.
/// @param default_value
///   If no value found neither in configuration file nor in environment,
///   this value will be returned, or empty string if this value is null.
/// @return
///   string configuration value.
/// @sa GetConfigInt(), GetConfigFlag()
string NCBI_XNCBI_EXPORT GetConfigString(const char* section,
                                         const char* variable,
                                         const char* default_value = 0);

/// Get integer configuration value.
///
/// @param section
///   Check application configuration named section first if not null.
/// @param variable
///   Variable name within application section.
///   If no value found in configuration file, environment variable with
///   name section_variable or variable will be checked, depending on
///   wether section is null.
/// @param default_value
///   If no value found neither in configuration file nor in environment,
///   this value will be returned.
/// @return
///   integer configuration value.
/// @sa GetConfigString(), GetConfigFlag()
int NCBI_XNCBI_EXPORT GetConfigInt(const char* section,
                                   const char* variable,
                                   int default_value = 0);

/// Get boolean configuration value.
///
/// @param section
///   Check application configuration named section first if not null.
/// @param variable
///   Variable name within application section.
///   If no value found in configuration file, environment variable with
///   name section_variable or variable will be checked, depending on
///   wether section is null.
/// @param default_value
///   If no value found neither in configuration file nor in environment,
///   this value will be returned.
/// @return
///   boolean configuration value.
/// @sa GetConfigString(), GetConfigInt()
bool NCBI_XNCBI_EXPORT GetConfigFlag(const char* section,
                                     const char* variable,
                                     bool default_value = false);

/* @} */


END_NCBI_SCOPE


/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.1  2005/01/05 18:45:29  vasilche
 * Added GetConfigXxx() functions.
 *
 *
 * ===========================================================================
 */

#endif  /* CORELIB___NCBI_CONFIG_VALUE__HPP */
