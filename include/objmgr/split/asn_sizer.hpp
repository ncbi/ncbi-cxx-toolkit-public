#ifndef NCBI_OBJMGR_SPLIT_ASN_SIZER__HPP
#define NCBI_OBJMGR_SPLIT_ASN_SIZER__HPP

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
#include <corelib/ncbiobj.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>

#include <vector>

BEGIN_NCBI_SCOPE

class CObjectOStream;

BEGIN_SCOPE(objects)

struct SSplitterParams;

class CAsnSizer
{
public:
    CAsnSizer(void);
    ~CAsnSizer(void);

    CObjectOStream& OpenDataStream(void);
    void CloseDataStream(void);

    size_t GetAsnSize(void) const
        {
            return m_AsnData.size();
        }
    const char* GetAsnData(void) const
        {
            return &m_AsnData.front();
        }
    size_t GetCompressedSize(void) const
        {
            return m_CompressedData.size();
        }
    const char* GetCompressedData(void) const
        {
            return &m_CompressedData.front();
        }
    size_t GetCompressedSize(const SSplitterParams& params);

    template<class C>
    void Set(const C& obj)
        {
            OpenDataStream() << obj;
            CloseDataStream();
        }
    template<class C>
    void Set(const C& obj, const SSplitterParams& params)
        {
            Set(obj);
            GetCompressedSize(params);
        }

    template<class C>
    size_t GetAsnSize(const C& obj)
        {
            Set(obj);
            return GetAsnSize();
        }

    template<class C>
    size_t GetCompressedSize(const C& obj, const SSplitterParams& params)
        {
            Set(obj, params);
            return GetCompressedSize();
        }

    // stream utility
    vector<char> m_AsnData;
    vector<char> m_CompressedData;
    AutoPtr<CNcbiOstrstream> m_MStream;
    AutoPtr<CObjectOStream> m_OStream;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif//NCBI_OBJMGR_SPLIT_ASN_SIZER__HPP
