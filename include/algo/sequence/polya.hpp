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
* Author: Philip Johnson
*
* File Description: finds mRNA 3' modification (poly-A tails)
*
* ---------------------------------------------------------------------------
*/
#ifndef ALGO_SEQUENCE___POLYA__HPP
#define ALGO_SEQUENCE___POLYA__HPP

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

enum EPolyTail {
    ePolyTail_None,
    ePolyTail_A3, //> 3' poly-A tail
    ePolyTail_T5  //> 5' poly-T head (submitted to db reversed?)
};


///////////////////////////////////////////////////////////////////////////////
/// PRE : null-terminated string containing sequence; possible cleavage site
/// POST: poly-A tail cleavage site, if any (-1 if not)
TSignedSeqPos FindPolyA(const char* seq, TSignedSeqPos possCleavageSite = -1);

///////////////////////////////////////////////////////////////////////////////
/// PRE : null-terminated string containing sequence; possible cleavage site
/// POST: cleavageSite (if any) and whether we found a poly-A tail, a poly-T
/// head, or neither
EPolyTail FindPolyTail(const char* seq, TSignedSeqPos &cleavageSite,
                       TSignedSeqPos possCleavageSite = -1);

END_NCBI_SCOPE

#endif /*ALGO_SEQUENCE___POLYA__HPP*/
