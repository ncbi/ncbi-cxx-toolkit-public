#ifndef CONNECT___NCBI_CORE_CXX__H
#define CONNECT___NCBI_CORE_CXX__H

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
 * Author:  Anton Lavrentiev
 *
 * File description:
 *   C++->C conversion functions for basic CORE connect stuff:
 *     Registry
 *     Logging
 *     Locking
 *
 */

#include <connect/ncbi_core.h>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbithr.hpp>


/** @addtogroup UtilityFunc
 *
 * @{
 */


BEGIN_NCBI_SCOPE


extern NCBI_XCONNECT_EXPORT REG     REG_cxx2c
(CNcbiRegistry* reg,
 bool           pass_ownership = false
 );


extern NCBI_XCONNECT_EXPORT LOG     LOG_cxx2c(void);


extern NCBI_XCONNECT_EXPORT MT_LOCK MT_LOCK_cxx2c
(CRWLock*       lock = 0,
 bool           pass_ownership = false
 );


typedef enum {
    eConnectInit_OwnNothing  = 0,
    eConnectInit_OwnRegistry = 0x01,
    eConnectInit_OwnLock     = 0x02
} EConnectInitFlags;

typedef unsigned int FConnectInitFlags;  /* bitwise OR of EConnectInitFlags*/


extern NCBI_XCONNECT_EXPORT void CONNECT_Init(CNcbiRegistry*    reg  = 0,
                                              CRWLock*          lock = 0,
                                              FConnectInitFlags flag = 0);


END_NCBI_SCOPE


/* @} */


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.13  2004/07/14 15:45:52  lavr
 * +flags (=0) for CONNECT_Init()
 *
 * Revision 6.12  2004/07/08 14:27:39  lavr
 * Additional parameter lock=0 for CONNET_Init()
 *
 * Revision 6.11  2003/04/09 17:58:49  siyan
 * Added doxygen support
 *
 * Revision 6.10  2003/01/08 01:59:32  lavr
 * DLL-ize CONNECT library for MSVC (add NCBI_XCONNECT_EXPORT)
 *
 * Revision 6.9  2002/12/19 14:51:48  dicuccio
 * Added export specifier for Win32 DLL builds.
 *
 * Revision 6.8  2002/06/12 19:19:37  lavr
 * Guard macro name standardized
 *
 * Revision 6.7  2002/06/10 19:49:32  lavr
 * +CONNECT_Init()
 *
 * Revision 6.6  2002/05/07 18:20:19  lavr
 * -#include <ncbidiag.hpp>: not needed for API definition (moved into .cpp)
 *
 * Revision 6.5  2002/01/15 21:28:34  lavr
 * +MT_LOCK_cxx2c()
 *
 * Revision 6.4  2001/03/02 20:06:41  lavr
 * Typo fixed
 *
 * Revision 6.3  2001/01/23 23:08:05  lavr
 * LOG_cxx2c introduced
 *
 * Revision 6.2  2001/01/12 16:48:32  lavr
 * Ownership passage is set to 'false' by default
 *
 * Revision 6.1  2001/01/11 23:08:14  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CONNECT___NCBI_CORE_CXX__HPP */
