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
*   Object manager iterators
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_descr_ci.hpp>
#include <objmgr/bioseq_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_descr_CI::CSeq_descr_CI(void)
{
    return;
}


CSeq_descr_CI::CSeq_descr_CI(const CBioseq_Handle& handle,
                             size_t search_depth)
    : m_NextEntry(handle.GetParentEntry()),
      m_MaxCount(search_depth)
{
    x_Next(); // Skip entries without descriptions
}


CSeq_descr_CI::CSeq_descr_CI(const CSeq_entry_Handle& entry,
                             size_t search_depth)
    : m_NextEntry(entry),
      m_MaxCount(search_depth)
{
    x_Next(); // Skip entries without descriptions
}

    
CSeq_descr_CI::CSeq_descr_CI(const CSeq_descr_CI& iter)
    : m_NextEntry(iter.m_NextEntry),
      m_CurrentEntry(iter.m_CurrentEntry),
      m_MaxCount(iter.m_MaxCount)
{
    return;
}


CSeq_descr_CI::~CSeq_descr_CI(void)
{
    return;
}


CSeq_descr_CI& CSeq_descr_CI::operator= (const CSeq_descr_CI& iter)
{
    if (this != &iter) {
        m_NextEntry = iter.m_NextEntry;
        m_CurrentEntry = iter.m_CurrentEntry;
        m_MaxCount = iter.m_MaxCount;
    }
    return *this;
}


void CSeq_descr_CI::x_Next(void)
{
    if ( !m_NextEntry ) {
        m_CurrentEntry = CSeq_entry_Handle();
        return;
    }
    // Find an entry with seq-descr member set
    while (m_NextEntry  &&  !m_NextEntry.IsSetDescr()) {
        m_NextEntry = m_NextEntry.GetParentEntry();
    }
    if ( m_NextEntry ) {
        m_CurrentEntry = m_NextEntry;
        m_NextEntry = m_NextEntry.GetParentEntry();
        if (m_MaxCount) {
            --m_MaxCount;
            if ( !m_MaxCount ) {
                m_NextEntry = CSeq_entry_Handle();
            }
        }
    }
    else {
        m_CurrentEntry = CSeq_entry_Handle();
    }
}


CSeq_descr_CI& CSeq_descr_CI::operator++(void)
{
    x_Next();
    return *this;
}


CSeq_descr_CI::operator bool (void) const
{
    return m_CurrentEntry  &&  m_CurrentEntry.IsSetDescr();
}


const CSeq_descr& CSeq_descr_CI::operator* (void) const
{
    _ASSERT(m_CurrentEntry  &&  m_CurrentEntry.IsSetDescr());
    return m_CurrentEntry.GetDescr();
}


const CSeq_descr* CSeq_descr_CI::operator-> (void) const
{
    _ASSERT(m_CurrentEntry  &&  m_CurrentEntry.IsSetDescr());
    return &m_CurrentEntry.GetDescr();
}


CSeq_entry_Handle CSeq_descr_CI::GetSeq_entry_Handle(void) const
{
    return m_CurrentEntry;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.13  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.12  2004/03/16 15:47:28  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.11  2004/02/09 19:18:54  grichenk
* Renamed CDesc_CI to CSeq_descr_CI. Redesigned CSeq_descr_CI
* and CSeqdesc_CI to avoid using data directly.
*
* Revision 1.10  2003/06/02 16:06:37  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.9  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.8  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.7  2003/03/13 13:02:36  dicuccio
* Added #include for annot_object.hpp - fixes obtuse error for Win32 builds
*
* Revision 1.6  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.5  2002/05/06 03:28:47  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:18  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/
