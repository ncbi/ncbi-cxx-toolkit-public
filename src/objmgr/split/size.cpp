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
* Author:  Eugene Vasilchenko
*
* File Description:
*   Application for splitting blobs withing ID1 cache
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objmgr/split/size.hpp>

#include <objmgr/split/asn_sizer.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CSize::CSize(const CAsnSizer& sizer)
    : m_Count(1),
      m_AsnSize(sizer.GetAsnSize()),
      m_ZipSize(sizer.GetCompressedSize())
{
}


CSize::CSize(TDataSize asn_size, double ratio)
    : m_Count(1),
      m_AsnSize(asn_size),
      m_ZipSize(TDataSize(double(asn_size)*ratio + .5))
{
}


int CSize::Compare(const CSize& size) const
{
    {
        TDataSize s1 = GetZipSize();
        TDataSize s2 = size.GetZipSize();
        // larger compressed size first
        if ( s1 > s2 ) {
            return -1;
        }
        if ( s1 < s2 ) {
            return 1;
        }
    }
    {
        TDataSize s1 = GetAsnSize();
        TDataSize s2 = size.GetAsnSize();
        // larger ASN.1 size first
        if ( s1 > s2 ) {
            return -1;
        }
        if ( s1 < s2 ) {
            return 1;
        }
    }
    {
        size_t s1 = GetCount();
        size_t s2 = size.GetCount();
        // fewer object count first
        if ( s1 > s2 ) {
            return 1;
        }
        if ( s1 < s2 ) {
            return -1;
        }
    }
    return 0;
}


CNcbiOstream& CSize::Print(CNcbiOstream& out) const
{
    return out <<
        "Cnt:" << setw(5) << m_Count << ", " <<
        setiosflags(ios::fixed) << setprecision(2) <<
        "Asn:" << setw(8) << (double)GetAsnSize()*(1./1024) << " KB, " <<
        "Zip:" << setw(8) << (double)GetZipSize()*(1./1024) << " KB, " <<
        setprecision(3) <<
        "Ratio: " << GetRatio();
}

END_SCOPE(objects)
END_NCBI_SCOPE
