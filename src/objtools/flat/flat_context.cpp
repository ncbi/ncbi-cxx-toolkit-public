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
* Author:  Aaron Ucko, NCBI
*
* File Description:
*   new (early 2003) flat-file generator -- context needed when (pre)formatting
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_formatter.hpp>

#include <corelib/ncbiutil.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


void CFlatContext::SetFlags(const CSeq_entry& entry, bool do_parents)
{
    if (do_parents  &&  entry.GetParentEntry()) {
        SetFlags(*entry.GetParentEntry(), true);
    }
    if (entry.IsSet()) {
        const CBioseq_set& bss = entry.GetSet();
        m_SegmentCount = 0;
        switch (bss.GetClass()) {
        case CBioseq_set::eClass_segset:
        case CBioseq_set::eClass_conset:
            ITERATE (CBioseq_set::TSeq_set, it, bss.GetSeq_set()) {
                if ((*it)->IsSeq()) {
                    m_SegMaster = &(*it)->GetSeq();
                    break;
                }
            }
            break;
        case CBioseq_set::eClass_parts:
            m_SegmentNum   = 1;
            m_SegmentCount = bss.GetSeq_set().size();
            break;
            // ...
        default:
            break;
        }
    } else {
        if (m_SegmentCount > 0  &&  m_SegmentNum == 1) {
            // Properly number individual segments
            const CBioseq_set& bss = entry.GetParentEntry()->GetSet();
            ITERATE (CBioseq_set::TSeq_set, it, bss.GetSeq_set()) {
                if (*it == &entry) {
                    break;
                }
                ++m_SegmentNum;
            }
            _ASSERT(m_SegmentNum <= m_SegmentCount);
        }
    }
}


const CSeq_id& CFlatContext::GetPreferredSynonym(const CSeq_id& id) const
{
    if (id.IsGi()  &&  id.GetGi() == m_GI) {
        return *m_PrimaryID;
    }

    CBioseq_Handle h = m_Handle.GetScope().GetBioseqHandle(id);
    if (h == m_Handle) {
        return *m_PrimaryID;
    } else if (h  &&  h.GetSeqId().NotEmpty()) {
        return *FindBestChoice(h.GetBioseqCore()->GetId(), CSeq_id::Score);
    } else {
        return id;
    }
}


const char* CFlatContext::GetUnits(bool abbrev) const
{
    if (m_IsWGSMaster) {
        return abbrev ? "rc" : "bases"; // XXX - not "records"?
    } else if (m_IsProt) {
        return abbrev ? "aa" : "residues";
    } else {
        return abbrev ? "bp" : "bases";
    }
    
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.7  2004/05/21 21:42:53  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.6  2003/11/11 01:31:31  ucko
* Fix for MSVC now that CBioseq_Handle::GetSeqId returns a CConstRef.
*
* Revision 1.5  2003/10/08 21:10:07  ucko
* For segmented sequences, find and note the "master" sequence rather
* than just setting a flag.
*
* Revision 1.4  2003/06/02 16:06:42  dicuccio
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
* Revision 1.3  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.2  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
