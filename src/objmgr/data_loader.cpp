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
*   Data loader base class for object manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2002/03/18 17:26:35  grichenk
* +CDataLoader::x_GetSeq_id(), x_GetSeq_id_Key(), x_GetSeq_id_Handle()
*
* Revision 1.1  2002/01/11 19:06:17  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <objects/objmgr1/data_loader.hpp>
#include "seq_id_mapper.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CDataLoader::CDataLoader(void)
{
    m_Name = NStr::PtrToString(this);
    return;
}


CDataLoader::CDataLoader(const string& loader_name)
    : m_Name(loader_name)
{
    if (loader_name.empty())
    {
        m_Name = NStr::PtrToString(this);
    }
}


CDataLoader::~CDataLoader(void)
{
    return;
}


void CDataLoader::SetTargetDataSource(CDataSource& data_source)
{
    m_DataSource = &data_source;
}


CDataSource* CDataLoader::GetDataSource(void)
{
    return m_DataSource;
}


void CDataLoader::SetName(const string& loader_name)
{
    m_Name = loader_name;
}


string CDataLoader::GetName(void) const
{
    return m_Name;
}


TSeq_id_Key CDataLoader::x_GetSeq_id_Key(const CSeq_id_Handle& handle)
{
    if ( !m_Mapper ) {
        m_Mapper = handle.m_Mapper;
    }
    _ASSERT(m_Mapper.GetPointer() == handle.m_Mapper);
    return handle.m_Value;
}


CSeq_id_Handle CDataLoader::x_GetSeq_id_Handle(TSeq_id_Key key)
{
    _ASSERT( m_Mapper );
    return CSeq_id_Handle(*m_Mapper, key);
}



END_SCOPE(objects)
END_NCBI_SCOPE
