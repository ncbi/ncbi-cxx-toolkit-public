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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Bioseq info for data source
*
*/


#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/seq_id_handle.hpp>
#include <objmgr/seq_map.hpp>
#include <objects/seq/Bioseq.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


////////////////////////////////////////////////////////////////////
//
//  CBioseq_Info::
//
//    Structure to keep bioseq's parent seq-entry along with the list
//    of seq-id synonyms for the bioseq.
//


CBioseq_Info::CBioseq_Info(CBioseq& seq, CSeq_entry_Info& entry_info)
    : m_Seq_entry_Info(&entry_info), m_Bioseq(&seq)
{
    _ASSERT(entry_info.m_Children.empty());
    _ASSERT(!entry_info.m_Bioseq);
    entry_info.m_Bioseq.Reset(this);
}


CBioseq_Info::~CBioseq_Info(void)
{
}


CDataSource& CBioseq_Info::GetDataSource(void) const
{
    return m_Seq_entry_Info->GetDataSource();
}


const CSeq_entry& CBioseq_Info::GetTSE(void) const
{
    return m_Seq_entry_Info->GetTSE();
}


const CTSE_Info& CBioseq_Info::GetTSE_Info(void) const
{
    return m_Seq_entry_Info->GetTSE_Info();
}


CTSE_Info& CBioseq_Info::GetTSE_Info(void)
{
    return m_Seq_entry_Info->GetTSE_Info();
}


const CSeq_entry& CBioseq_Info::GetSeq_entry(void) const
{
    return m_Seq_entry_Info->GetSeq_entry();
}


CSeq_entry& CBioseq_Info::GetSeq_entry(void)
{
    return m_Seq_entry_Info->GetSeq_entry();
}


void CBioseq_Info::DebugDump(CDebugDumpContext ddc, unsigned int depth) const
{
    ddc.SetFrame("CBioseq_Info");
    CObject::DebugDump( ddc, depth);

    ddc.Log("m_Bioseq", m_Bioseq.GetPointer(),0);
    if (depth == 0) {
        DebugDumpValue(ddc, "m_Synonyms.size()", m_Synonyms.size());
    } else {
        DebugDumpValue(ddc, "m_Synonyms.type",
            "set<CSeq_id_Handle>");
        CDebugDumpContext ddc2(ddc,"m_Synonyms");
        TSynonyms::const_iterator it;
        int n;
        for (n=0, it=m_Synonyms.begin(); it!=m_Synonyms.end(); ++n, ++it) {
            string member_name = "m_Synonyms[ " +
                NStr::IntToString(n) +" ]";
            ddc2.Log(member_name, (*it).AsString());
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.11  2003/06/02 16:06:37  dicuccio
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
* Revision 1.10  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.9  2003/03/14 19:10:41  grichenk
* + SAnnotSelector::EIdResolving; fixed operator=() for several classes
*
* Revision 1.8  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.7  2003/02/05 17:59:17  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.6  2002/12/26 20:55:17  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.5  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.4  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.3  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.2  2002/02/21 19:27:05  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/02/07 21:25:05  grichenk
* Initial revision
*
*
* ===========================================================================
*/
