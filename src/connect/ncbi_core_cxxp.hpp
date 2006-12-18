#ifndef CONNECT___NCBI_CORE_CXXP__H
#define CONNECT___NCBI_CORE_CXXP__H

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
 *   CONNECT_InitInternal(): private API
 *
 */

#include <connect/ncbi_core_cxx.hpp>

#ifdef __GNUC__
// Warn at least for GNU compilations :-(
#  warning \
"This header is obsolele and scheduled for removal, please don't use it!!"
#endif /*__GNUC__*/


BEGIN_NCBI_SCOPE


extern NCBI_XCONNECT_EXPORT NCBI_DEPRECATED void CONNECT_InitInternal(void);


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.2  2006/12/18 21:07:41  lavr
 * Mark both CONNECT_InitInternal() and the entire header as obsolete
 *
 * Revision 1.1  2004/09/09 16:44:54  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CONNECT___NCBI_CORE_CXXP__HPP */
