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
* Author: Aleksey Grichenko
*
* File Description:
*   Seq-id handle for Object Manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2002/03/15 18:10:08  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.3  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.2  2002/02/12 19:41:42  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
*
* Revision 1.1  2002/01/23 21:57:22  grichenk
* Splitted id_handles.hpp
*
*
* ===========================================================================
*/

#include <objects/objmgr1/seq_id_handle.hpp>
#include "seq_id_mapper.hpp"
#include <serial/typeinfo.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Handle::
//


CSeq_id_Handle::CSeq_id_Handle(const CSeq_id_Handle& handle)
    : m_Mapper(handle.m_Mapper),
      m_Value(handle.m_Value)
{
    if ( m_Mapper )
        m_Mapper->AddHandleReference(*this);
}


CSeq_id_Handle::CSeq_id_Handle(CSeq_id_Mapper& mapper,
                               TSeq_id_Key key)
    : m_Mapper(&mapper),
      m_Value(key)
{
    m_Mapper->AddHandleReference(*this);
}


CSeq_id_Handle::~CSeq_id_Handle(void)
{
    Reset();
}


CSeq_id_Handle& CSeq_id_Handle::operator= (const CSeq_id_Handle& handle)
{
    Reset();
    m_Mapper = handle.m_Mapper;
    m_Value = handle.m_Value;
    if ( m_Mapper )
        m_Mapper->AddHandleReference(*this);
    return *this;
}


void CSeq_id_Handle::Reset(void)
{
    if ( m_Mapper )
        m_Mapper->ReleaseHandleReference(*this);
    m_Mapper = 0;
    m_Value = 0;
}

bool CSeq_id_Handle::x_Equal(const CSeq_id_Handle& handle) const
{
    // Different mappers -- handle can not be equal
    if (m_Mapper != handle.m_Mapper)
        return false;
    return m_Value == handle.m_Value;
}


bool CSeq_id_Handle::x_Match(const CSeq_id_Handle& handle) const
{
    // Different mappers -- handle can not be equal
    if (m_Mapper != handle.m_Mapper)
        return false;
    return x_GetSeqId()->Match(*handle.x_GetSeqId());
}


bool CSeq_id_Handle::IsBetter(const CSeq_id_Handle& h) const
{
    if (m_Mapper != h.m_Mapper  ||  !m_Mapper)
        return false;
    return m_Mapper->IsBetter(*this, h);
}


const CSeq_id* CSeq_id_Handle::x_GetSeqId(void) const
{
    return m_Mapper ? m_Mapper->x_GetSeq_id(m_Value) : 0;
};

END_SCOPE(objects)
END_NCBI_SCOPE
