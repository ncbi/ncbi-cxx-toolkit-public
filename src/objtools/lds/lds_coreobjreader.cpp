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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Core objects reader implementations.
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/lds/lds_coreobjreader.hpp>

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqalign/Seq_align.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CLDS_CoreObjectsReader::CLDS_CoreObjectsReader(void)
{
    AddCandidate(CObjectTypeInfo(CType<CSeq_entry>()));
    AddCandidate(CObjectTypeInfo(CType<CBioseq>()));
    AddCandidate(CObjectTypeInfo(CType<CBioseq_set>()));
    AddCandidate(CObjectTypeInfo(CType<CSeq_annot>()));
    AddCandidate(CObjectTypeInfo(CType<CSeq_align>()));
}


void CLDS_CoreObjectsReader::OnTopObjectFoundPre(const CObjectInfo& object,
                                                 CNcbiStreampos     stream_pos)
{
    // GetStreamPos() returns offset of the most recent top level object
    // In case of the Text ASN.1 it can differ from the stream_offset variable
    // because reader first reads the file header and only then calls the main
    // Read function.
    CNcbiStreampos pos = GetStreamPos();

    m_TopDescr = SObjectParseDescr(&object, pos);
    m_Stack.push(SObjectParseDescr(&object, pos));
}

void CLDS_CoreObjectsReader::OnTopObjectFoundPost(const CObjectInfo& object)
{
    SObjectParseDescr pdescr = m_Stack.top();

    SObjectDetails od(*pdescr.object_info, 
                      pdescr.stream_pos,
                      0,
                      0,
                      true);
    m_Objects.push_back(od);
    m_Stack.pop();
    _ASSERT(m_Stack.empty());
}


void CLDS_CoreObjectsReader::OnObjectFoundPre(const CObjectInfo& object, 
                                              CNcbiStreampos     stream_pos)
{
    if (m_Stack.size() == 0) {
        OnTopObjectFoundPre(object, stream_pos);
        return;
    }

    _ASSERT(stream_pos);

    m_Stack.push(SObjectParseDescr(&object, stream_pos));
}


void CLDS_CoreObjectsReader::OnObjectFoundPost(const CObjectInfo& object)
{
    if (m_Stack.size() == 1) {
        OnTopObjectFoundPost(object);
        return;
    }
    SObjectParseDescr object_descr = m_Stack.top();
    m_Stack.pop();
    _ASSERT(!m_Stack.empty());

    SObjectParseDescr parent_descr = m_Stack.top();
    _ASSERT(object_descr.stream_pos); // non-top object must have an offset

    SObjectDetails od(object, 
                      object_descr.stream_pos,
                      parent_descr.stream_pos,
                      m_TopDescr.stream_pos,
                      false);
    m_Objects.push_back(od);
}


void CLDS_CoreObjectsReader::Reset()
{
    while (!m_Stack.empty()) {
        m_Stack.pop();
    }
}


CLDS_CoreObjectsReader::SObjectDetails* 
CLDS_CoreObjectsReader::FindObjectInfo(CNcbiStreampos pos)
{
    if ( m_ObjectIndex.empty() ) {
        NON_CONST_ITERATE ( TObjectVector, it, m_Objects ) {
            SObjectDetails& obj = *it;
            m_ObjectIndex.insert(TObjectIndex::value_type(obj.offset, &obj));
        }
    }

    TObjectIndex::const_iterator it = m_ObjectIndex.find(pos);
    if ( it != m_ObjectIndex.end() ) {
        return it->second;
    }
    return 0;
}

void CLDS_CoreObjectsReader::ClearObjectsVector()
{
    m_Objects.clear();
    m_ObjectIndex.clear();
}

END_SCOPE(objects)
END_NCBI_SCOPE
