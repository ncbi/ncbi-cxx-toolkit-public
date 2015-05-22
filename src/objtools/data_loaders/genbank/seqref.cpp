/*  $Id$
 * ===========================================================================
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
 * ===========================================================================
 *
 *  Author:  Eugene Vasilchenko
 *
 *  File Description: Base data reader interface
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/seqref.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CSeqref::CSeqref(void)
    : m_Flags(fBlobHasAllLocal),
      m_Gi(ZERO_GI), m_Sat(0), m_SubSat(eSubSat_main), m_SatKey(0),
      m_Version(0)
{
}


CSeqref::CSeqref(TGi gi, int sat, TIntId satkey)
    : m_Flags(fBlobHasAllLocal),
      m_Gi(gi), m_Sat(sat), m_SubSat(eSubSat_main), m_SatKey(satkey),
      m_Version(0)
{
}


CSeqref::CSeqref(TGi gi, int sat, TIntId satkey, TSubSat subsat, TFlags flags)
    : m_Flags(flags),
      m_Gi(gi), m_Sat(sat), m_SubSat(subsat), m_SatKey(satkey),
      m_Version(0)
{
}


CSeqref::~CSeqref(void)
{
}


const string CSeqref::print(void) const
{
    CNcbiOstrstream ostr;
    ostr << "SeqRef(" << GetSat();
    if ( GetSubSat() != eSubSat_main )
        ostr << '.' << GetSubSat();
    ostr << ',' << GetSatKey() << ',' << GetGi() << ')';
    return CNcbiOstrstreamToString(ostr);
}


const string CSeqref::printTSE(void) const
{
    CNcbiOstrstream ostr;
    ostr << "TSE(" << GetSat();
    if ( GetSubSat() != eSubSat_main )
        ostr << '.' << GetSubSat();
    ostr << ',' << GetSatKey() << ')';
    return CNcbiOstrstreamToString(ostr);
}


const string CSeqref::printTSE(const TKeyByTSE& key)
{
    CNcbiOstrstream ostr;
    ostr << "TSE(" << key.first.first;
    if ( key.first.second != eSubSat_main )
        ostr << '.' << key.first.second;
    ostr << ',' << key.second << ')';
    return CNcbiOstrstreamToString(ostr);
}


END_SCOPE(objects)
END_NCBI_SCOPE
