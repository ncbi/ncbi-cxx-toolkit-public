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


SAnnotObjectsIndex::SAnnotObjectsIndex(void)
    : m_Indexed(false)
{
}


SAnnotObjectsIndex::SAnnotObjectsIndex(const CAnnotName& name)
    : m_Name(name), m_Indexed(false)
{
}


SAnnotObjectsIndex::SAnnotObjectsIndex(const SAnnotObjectsIndex& info)
    : m_Name(info.m_Name), m_Indexed(false)
{
}


SAnnotObjectsIndex::~SAnnotObjectsIndex(void)
{
}


void SAnnotObjectsIndex::SetName(const CAnnotName& name)
{
    _ASSERT(!IsIndexed());
    m_Name = name;
}


void SAnnotObjectsIndex::Clear(void)
{
    m_Keys.clear();
    m_Indexed = false;
}


void SAnnotObjectsIndex::ReserveMapSize(size_t size)
{
    _ASSERT(m_Keys.empty());
    m_Keys.reserve(size);
}


void SAnnotObjectsIndex::AddInfo(const CAnnotObject_Info& info)
{
    m_Infos.push_back(info);
}


void SAnnotObjectsIndex::AddMap(const SAnnotObject_Key& key,
                                const SAnnotObject_Index& index)
{
    m_Keys.push_back(key);
}


void SAnnotObjectsIndex::RemoveLastMap(void)
{
    m_Keys.pop_back();
}


void SAnnotObjectsIndex::PackKeys(void)
{
    TObjectKeys keys(m_Keys.begin(), m_Keys.end());
    keys.swap(m_Keys);
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.7  2006/09/18 14:29:29  vasilche
* Store annots indexing information to allow reindexing after modification.
*
* Revision 1.6  2005/09/20 15:45:36  vasilche
* Feature editing API.
* Annotation handles remember annotations by index.
*
* Revision 1.5  2005/08/23 17:02:55  vasilche
* Used typedefs for integral members of SAnnotObject_Index.
* Moved multi id flags from CAnnotObject_Info to SAnnotObject_Index.
* Used deque<> instead of vector<> for storing CAnnotObject_Info set.
*
* Revision 1.4  2004/12/22 15:56:20  vasilche
* Renamed SAnnotObjects_Info -> SAnnotObjectsIndex.
* Allow reuse of annotation indices for several TSEs.
*
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
