#ifndef NCBI_OBJMGR_SPLIT_SIZE__HPP
#define NCBI_OBJMGR_SPLIT_SIZE__HPP

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


#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CAsnSizer;

class CSize
{
public:
    typedef size_t TDataSize;

    CSize(void)
        {
            clear();
        }
    CSize(const CAsnSizer& sizer);
    CSize(TDataSize asn_size, double ratio);

    void clear(void)
        {
            m_Count = 0;
            m_AsnSize = 0;
            m_ZipSize = 0;
        }

    CSize& operator+=(const CSize& size)
        {
            m_Count += size.m_Count;
            m_AsnSize += size.m_AsnSize;
            m_ZipSize += size.m_ZipSize;
            return *this;
        }
    CSize& operator-=(const CSize& size)
        {
            m_Count -= size.m_Count;
            m_AsnSize -= size.m_AsnSize;
            m_ZipSize -= size.m_ZipSize;
            return *this;
        }
    CSize operator+(const CSize& size) const
        {
            CSize ret(*this);
            ret += size;
            return ret;
        }

    size_t GetCount(void) const
        {
            return m_Count;
        }
    TDataSize GetAsnSize(void) const
        {
            return m_AsnSize;
        }
    TDataSize GetZipSize(void) const
        {
            return m_ZipSize;
        }
    double GetRatio(void) const
        {
            return (double)m_ZipSize/(double)m_AsnSize;
        }

    CNcbiOstream& Print(CNcbiOstream& out) const;

    DECLARE_OPERATOR_BOOL(m_Count != 0);

    bool operator>(const CSize& size) const
        {
            return m_ZipSize > size.m_ZipSize;
        }

    int Compare(const CSize& size) const;
    
private:
    size_t m_Count;
    TDataSize m_AsnSize;
    TDataSize m_ZipSize;
};

inline
CNcbiOstream& operator<<(CNcbiOstream& out, const CSize& size)
{
    return size.Print(out);
}

END_SCOPE(objects)
END_NCBI_SCOPE

#endif//NCBI_OBJMGR_SPLIT_SIZE__HPP
