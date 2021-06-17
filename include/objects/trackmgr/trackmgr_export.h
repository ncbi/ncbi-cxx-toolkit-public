#ifndef TRACKMGR__TRACKMGR_EXPORT__H
#define TRACKMGR__TRACKMGR_EXPORT__H

/* 
 * $Id$
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
 * Author: Peter Meric
 *
 */


/// @file trackmgr_export.h
///   Defines to provide correct exporting from DLLs in Windows.
///   These are necessary to compile DLLs with Visual C++ - exports must be
///   explicitly labeled as such.


#include <common/ncbi_export.h>


/*
 * -------------------------------------------------
 * DLL clusters
 */


/*
 * Definitions for TRACKMGRASN.DLL
 */
#ifdef NCBI_TRACKMGRASN_EXPORTS
#  define NCBI_TRACKMGR_EXPORTS
#endif



/* ------------------------------------------------- */
/*
 * Individual Library Definitions
 * Please keep alphabetized!
 */

/*
 * Export specifier for library ideo
 */
#ifdef NCBI_TRACKMGR_EXPORTS
#  define NCBI_TRACKMGR_EXPORT NCBI_DLL_EXPORT
#else
#  define NCBI_TRACKMGR_EXPORT NCBI_DLL_IMPORT
#endif


#endif // TRACKMGR__TRACKMGR_EXPORT__H

