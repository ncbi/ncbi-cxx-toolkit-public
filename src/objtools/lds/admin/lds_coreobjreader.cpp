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
#include <objtools/lds/admin/lds_coreobjreader.hpp>

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
                                                 size_t stream_offset)
{
    // GetStreamOffset() returns offset of the most recent top level object
    // In case of the Text ASN.1 it can differ from the stream_offset variable
    // because reader first reads the file header and only then calls the main
    // Read function.
    size_t offset = GetStreamOffset();

    m_TopDescr = SObjectParseDescr(&object, offset);
    m_Stack.push(SObjectParseDescr(&object, offset));
}

void CLDS_CoreObjectsReader::OnTopObjectFoundPost(const CObjectInfo& object)
{
    SObjectParseDescr pdescr = m_Stack.top();

    SObjectDetails od(*pdescr.object_info, 
                      pdescr.stream_offset,
                      0,
                      0,
                      true);
    m_Objects.push_back(od);
    m_Stack.pop();
    _ASSERT(m_Stack.empty());
}


void CLDS_CoreObjectsReader::OnObjectFoundPre(const CObjectInfo& object, 
                                              size_t stream_offset)
{
    if (m_Stack.size() == 0) {
        OnTopObjectFoundPre(object, stream_offset);
        return;
    }

    _ASSERT(stream_offset);

    m_Stack.push(SObjectParseDescr(&object, stream_offset));
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
    _ASSERT(object_descr.stream_offset); // non-top object must have an offset

    SObjectDetails od(object, 
                      object_descr.stream_offset,
                      parent_descr.stream_offset,
                      m_TopDescr.stream_offset,
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
CLDS_CoreObjectsReader::FindObjectInfo(size_t stream_offset)
{
    int idx = FindObject(stream_offset);
    if (idx < 0)
        return 0;
    return &m_Objects[idx];
}

int CLDS_CoreObjectsReader::FindObject(size_t stream_offset)
{
    int idx = 0;
    ITERATE(TObjectVector, it, m_Objects) {
        if (it->offset == stream_offset) {
            return idx;
        }
        ++idx;
    }
    return -1;
}

END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2004/05/21 21:42:55  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.5  2003/10/09 16:46:57  kuznets
 * Fixed bug with cleaning objects vector on every OnTopObjectFound call.
 * Caused incorrect ASN.1 binary scan.
 *
 * Revision 1.4  2003/10/07 20:45:23  kuznets
 * Implemented Reset()
 *
 * Revision 1.3  2003/07/14 19:44:40  kuznets
 * Fixed a bug with objects offset in for ASN.1 text files
 *
 * Revision 1.2  2003/06/16 16:24:43  kuznets
 * Fixed #include paths (lds <-> lds_admin separation)
 *
 * Revision 1.1  2003/06/03 14:13:25  kuznets
 * Initial revision
 *
 * Revision 1.1  2003/05/22 18:56:05  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */
