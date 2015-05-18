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
* Author: Clifford Clausen
*
* File Description: Functions for creating CSeq_locs from CRegexps
*
* ===========================================================================*/

#include <ncbi_pch.hpp>
#include <algo/sequence/regexp_loc.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CRegexp_loc::CRegexp_loc (const string &pat, CRegexp::TCompile flags)
    : m_regexp(new CRegexp(pat, flags))
{
}

CRegexp_loc::~CRegexp_loc()
{
}

void CRegexp_loc::Set(const string &pat, CRegexp::TCompile flags)
{
    m_regexp->Set(pat, flags);
}

TSeqPos CRegexp_loc::GetLoc
(const char *seq,
 CSeq_loc *loc,
 TSeqPos offset,
 CRegexp::TMatch flags)
{
    // Reset loc to type CPacked_seqint
    CSeq_loc::TPacked_int &packed = loc->SetPacked_int();

    // Get list of CSeq_interval
    CPacked_seqint::Tdata &lst = packed.Set();
    lst.clear();

    // Match the regular expression to the sequence
    m_regexp->GetMatch(seq, offset, 0, flags, true);

    // Create a CSeq_interval for whole pattern match
    // and each sub-pattern match and push into list
    for (int i = 0; i < m_regexp->NumFound(); i++) {
        CRef<CSeq_interval> si(new CSeq_interval);
        si->SetFrom(m_regexp->GetResults(i)[0]);
        si->SetTo(m_regexp->GetResults(i)[1] - 1);
        lst.push_back(si);
    }
    if (m_regexp->NumFound() > 0) {
        return m_regexp->GetResults(0)[0];
    } else {
        return kInvalidSeqPos;
    }
}

END_NCBI_SCOPE
