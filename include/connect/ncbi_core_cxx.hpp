#ifndef NCBI_CORE_CXX__H
#define NCBI_CORE_CXX__H

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
 *   C++->C conversion functions for basic corelib stuff:
 *     Registry
 *     Logging
 *     Locking
 *
 * ---------------------------------------------------------------------------
 * $Log$
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

#include <connect/ncbi_core.h>
#include <corelib/ncbidiag.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/ncbithr.hpp>

BEGIN_NCBI_SCOPE


extern REG         REG_cxx2c(CNcbiRegistry* reg, bool pass_ownership = false);
extern LOG         LOG_cxx2c(void);
extern MT_LOCK MT_LOCK_cxx2c(CRWLock* lock = 0, bool pass_ownership = false);

END_NCBI_SCOPE

#endif  /* NCBI_CORE_CXX__HPP */
