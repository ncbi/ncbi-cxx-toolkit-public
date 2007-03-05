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
* Authors:  Chris Lanczycki
*
* File Description:
*      Some definitions to handle toolkit diagnostic streams (taken from Paul's
*      struct_util/su_private.hpp)
*
* ===========================================================================
*/

#ifndef AR_DIAGNOSTIC_DEFS__HPP
#define AR_DIAGNOSTIC_DEFS__HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_SCOPE(align_refine)

#define ERROR_MESSAGE_CL(s) ERR_POST(ncbi::Error << "align_refine: " << s << '!')
#define WARNING_MESSAGE_CL(s) ERR_POST(ncbi::Warning << "align_refine: " << s)
#define LOG_MESSAGE_CL(s) LOG_POST(s)
#define TRACE_MESSAGE_CL(s) ERR_POST(ncbi::Trace << "align_refine: " << s)

#define TERSE_INFO_MESSAGE_CL(s) \
    ( NCBI_NS_NCBI::CNcbiDiag(eDiag_Info, eDPF_Log) \
      << s \
      << NCBI_NS_NCBI::Endm )


#define THROW_MESSAGE_CL(str) throw ncbi::CException(__FILE__, __LINE__, NULL, ncbi::CException::eUnknown, (str))

END_SCOPE(align_refine)

#endif // AR_DIAGNOSTIC_DEFS__HPP
