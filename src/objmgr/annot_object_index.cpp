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
* Author: Eugene Vasilchenko
*
* File Description:
*   AnnotObject indexes structures
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/impl/annot_object_index.hpp>
#include <objmgr/impl/annot_object.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


SAnnotObjects_Info::SAnnotObjects_Info(void)
{
}


SAnnotObjects_Info::SAnnotObjects_Info(const CAnnotName& name)
    : m_Name(name)
{
}


SAnnotObjects_Info::SAnnotObjects_Info(const SAnnotObjects_Info& info)
    : m_Name(info.m_Name)
{
    _ASSERT(info.m_Keys.empty() && info.m_Infos.empty());
}


SAnnotObjects_Info::~SAnnotObjects_Info(void)
{
}


void SAnnotObjects_Info::SetName(const CAnnotName& name)
{
    m_Name = name;
}


void SAnnotObjects_Info::Reserve(size_t size, double keys_factor)
{
    _ASSERT(m_Keys.empty() && m_Infos.empty());
    m_Keys.reserve(size_t(keys_factor*size));
    m_Infos.reserve(size);
}


void SAnnotObjects_Info::Clear(void)
{
    m_Keys.clear();
    m_Infos.clear();
}


void SAnnotObjects_Info::AddKey(const SAnnotObject_Key& key)
{
    m_Keys.push_back(key);
}


CAnnotObject_Info* SAnnotObjects_Info::AddInfo(const CAnnotObject_Info& info)
{
    _ASSERT(m_Infos.capacity() > m_Infos.size());
    m_Infos.push_back(info);
    return &m_Infos.back();
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2004/05/21 21:42:12  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.2  2003/11/26 17:55:56  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.1  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* ===========================================================================
*/
