#ifndef UTIL__HPP
#define UTIL__HPP

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
* Author:  Aaron Ucko
*
* File Description:
*   Utilities shared by Genbank and non-Genbank code.
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/09/05 14:44:47  ucko
* Use NStr::IntToString instead of Stringify.
*
* Revision 1.1  2001/09/04 16:20:54  ucko
* Dramatically fleshed out id1_fetch
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <objects/objmgr/seqvector.hpp>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE

BEGIN_SCOPE(objects)
class CSeq_inst;
class CSeq_vector;
END_SCOPE(objects)

USING_SCOPE(objects);

// #define USE_SEQ_VECTOR
#ifdef USE_SEQ_VECTOR
typedef CSeq_vector TASCIISeqData;
#else
typedef string TASCIISeqData;
TASCIISeqData ToASCII(const CSeq_inst& seq_inst);
#endif


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* UTIL__HPP */
