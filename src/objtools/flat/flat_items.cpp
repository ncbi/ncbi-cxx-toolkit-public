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
*   new (early 2003) flat-file generator -- most item types
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objtools/flat/flat_items.hpp>

#include <serial/iterator.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/PIR_block.hpp>
#include <objects/seqblock/PRF_block.hpp>
#include <objects/seqblock/SP_block.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/BinomialOrgName.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/PartialOrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/TaxElement.hpp>

#include <objtools/alnmgr/alnmap.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/seq_vector.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CFlatKeywords::CFlatKeywords(const CFlatContext& ctx)
{
    for (CSeqdesc_CI it(ctx.GetHandle());  it;  ++it) {
        switch (it->Which()) {
        // Grr, MSVC won't let me handle everything with a single template.
        case CSeqdesc::e_Pir:
          if (it->GetPir().IsSetKeywords()) {
              m_Keywords.insert(m_Keywords.end(),
                                it->GetPir().GetKeywords().begin(),
                                it->GetPir().GetKeywords().end());
          }
	  break;

        case CSeqdesc::e_Genbank:
          if (it->GetGenbank().IsSetKeywords()) {
              m_Keywords.insert(m_Keywords.end(),
                                it->GetGenbank().GetKeywords().begin(),
                                it->GetGenbank().GetKeywords().end());
          }
	  break;

        case CSeqdesc::e_Sp:
          if (it->GetSp().IsSetKeywords()) {
              m_Keywords.insert(m_Keywords.end(),
                                it->GetSp().GetKeywords().begin(),
                                it->GetSp().GetKeywords().end());
          }
	  break;

        case CSeqdesc::e_Embl:
          if (it->GetEmbl().IsSetKeywords()) {
              m_Keywords.insert(m_Keywords.end(),
                                it->GetEmbl().GetKeywords().begin(),
                                it->GetEmbl().GetKeywords().end());
          }
	  break;

        case CSeqdesc::e_Prf:
          if (it->GetPrf().IsSetKeywords()) {
              m_Keywords.insert(m_Keywords.end(),
                                it->GetPrf().GetKeywords().begin(),
                                it->GetPrf().GetKeywords().end());
          }
	  break;

        default:
            break;
        }
    }
}


CFlatSource::CFlatSource(const CFlatContext& ctx)
{
    for (CSeqdesc_CI it(ctx.GetHandle());  it;  ++it) {
        switch (it->Which()) {
        case CSeqdesc::e_Org:
        case CSeqdesc::e_Source:
        {
            if ( !m_Descriptor ) {
                m_Descriptor.Reset(&*it);
            }
            const COrg_ref& org = (it->IsOrg() ? it->GetOrg()
                                   : it->GetSource().GetOrg());
            if (org.IsSetOrgname()) {
                // done first so taxname can override m_FormalName
                org.GetOrgname().GetFlatName(m_FormalName, &m_Lineage);
            }
            if (org.IsSetTaxname()) {
                m_FormalName = org.GetTaxname();
            }
            if (org.IsSetCommon()) {
                m_CommonName = org.GetCommon();
            }
            m_TaxID = org.GetTaxId();
            break;
        }
        default:
            break;
        }
    }
}


CFlatComment::CFlatComment(const CFlatContext& ctx)
{
    string delim;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Comment);  it;  ++it) {
        m_Comment += delim + it->GetComment();
        delim = '\n';
    }
    for (CFeat_CI it(ctx.GetHandle().GetScope(), ctx.GetLocation(),
                     CSeqFeatData::e_Comment);
         it;  ++it) {
        if (it->IsSetComment()) { // ought to be, but just in case...
            m_Comment += delim + it->GetComment();
            delim = '\n';
        }
    }    
}


CFlatPrimary::CFlatPrimary(const CFlatContext& ctx)
    : m_IsRefSeq(ctx.IsRefSeq())
{
    const CSeq_inst& inst = ctx.GetHandle().GetBioseqCore()->GetInst();
    if ( !inst.IsSetHist()  ||  !inst.GetHist().IsSetAssembly()) {
        return;
    }
    ITERATE (CSeq_hist::TAssembly, it, inst.GetHist().GetAssembly()) {
        if ( !(*it)->GetSegs().IsDenseg() ) {
            // complain
            continue;
        }
        CAlnMap aln((*it)->GetSegs().GetDenseg());
        // XXX - more sanity checks (2 rows, first is seq, running forward)
        // XXX - should honor m_Location
        SPiece p;
        p.m_Span         = aln.GetSeqRange(0);
        p.m_PrimaryID    = &ctx.GetPreferredSynonym(aln.GetSeqId(1));
        p.m_PrimarySpan  = aln.GetSeqRange(1);
        p.m_Complemented = aln.IsNegativeStrand(1);
        m_Pieces.push_back(p);
    }
}


const char* CFlatPrimary::GetHeader(void) const
{
    if (m_IsRefSeq) {
        return
            "REFSEQ_SPAN         PRIMARY_IDENTIFIER PRIMARY_SPAN        COMP";
    } else {
        return
            "TPA_SPAN            PRIMARY_IDENTIFIER PRIMARY_SPAN        COMP";
    }
}


string& CFlatPrimary::SPiece::Format(string& s) const
{
    s += NStr::IntToString(m_Span.GetFrom()) + '-'
        + NStr::IntToString(m_Span.GetTo());
    s.resize(20, ' ');
    s += m_PrimaryID->GetSeqIdString(true);
    s.resize(39, ' ');
    s += NStr::IntToString(m_PrimarySpan.GetFrom()) + '-'
        + NStr::IntToString(m_PrimarySpan.GetTo());
    s.resize(59, ' ');
    s += m_Complemented ? 'c' : ' ';
    return s;
}


void CFlatDataHeader::GetCounts(TSeqPos& a, TSeqPos& c, TSeqPos& g, TSeqPos& t,
                                TSeqPos& other) const
{
    if ( !m_IsProt
        &&  !m_As  &&  !m_Cs  &&  !m_Gs  &&  !m_Ts  &&  !m_Others ) {
        CSeqVector v = m_Handle.GetSequenceView
            (*m_Loc, CBioseq_Handle::eViewConstructed);
        // TSeqPos counts[numeric_limits<CSeqVector::TResidue>::max() + 1];
        TSeqPos counts[256];
        memset(counts, 0, sizeof(counts));
        string buf; // avoids slow CSeqVector::operator[]
        static const TSeqPos chunk_size = 4096;
        for (TSeqPos start = 0;  start < v.size();  start += chunk_size) {
            TSeqPos count = min(chunk_size, v.size() - start);
            v.GetSeqData(start, start + count, buf);
            for (TSeqPos i = 0;  i < count;  ++i) {
                ++counts[buf[i]];
            }
        }
        m_As = counts[1];
        m_Cs = counts[2];
        m_Gs = counts[4];
        m_Ts = counts[8];
        m_Others = v.size() - m_As - m_Cs - m_Gs - m_Ts;
    }
    a     = m_As;
    c     = m_Cs;
    g     = m_Gs;
    t     = m_Ts;
    other = m_Others;
}


CFlatWGSRange::CFlatWGSRange(const CFlatContext& ctx)
{
    for (CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_User);  desc;  ++desc) {
        const CUser_object& uo = desc->GetUser();
        if ( !uo.GetType().IsStr()
            ||  NStr::CompareNocase(uo.GetType().GetStr(), "WGSProjects")) {
            continue;
        }
        ITERATE (CUser_object::TData, it, uo.GetData()) {
            if ( !(*it)->GetLabel().IsStr() ) {
                // complain?
                continue;
            }
            const string& label = (*it)->GetLabel().GetStr();
            if        ( !NStr::CompareNocase(label, "WGS_accession_first") ) {
                // san-check
                m_First = (*it)->GetData().GetStr();
            } else if ( !NStr::CompareNocase(label, "WGS_accession_last") ) {
                // san-check
                m_Last  = (*it)->GetData().GetStr();
            }
        }
        if ( !m_UO ) {
            m_UO.Reset(&uo);
        }
    }

    if (m_First.empty()  ||  m_Last.empty()) {
        // complain?
        // reconstruct from other info
        SIZE_TYPE digit_pos = ctx.GetAccession().find_first_of("0123456789");
        string    prefix    = ctx.GetAccession().substr(0, digit_pos + 2);
        m_First = prefix + "000001";
        CNcbiOstrstream oss;
        oss << prefix << setw(6) << setfill('0')
            << ctx.GetHandle().GetBioseqCore()->GetInst().GetLength();
        m_Last = CNcbiOstrstreamToString(oss);
    }
}


CFlatGenomeInfo::CFlatGenomeInfo(const CFlatContext& ctx)
{
    for (CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_User);  desc;  ++desc) {
        const CUser_object& uo = desc->GetUser();
        if ( !uo.GetType().IsStr()
            ||  NStr::CompareNocase(uo.GetType().GetStr(), "GenomeProject")) {
            continue;
        }
        ITERATE (CUser_object::TData, it, uo.GetData()) {
            if ( !(*it)->GetLabel().IsStr() ) {
                // complain?
                continue;
            }
            const string& label = (*it)->GetLabel().GetStr();
            if        ( !NStr::CompareNocase(label, "accession") ) {
                // san-check
                m_Accession = &(*it)->GetData().GetStr();
            } else if ( !NStr::CompareNocase(label, "Moltype") ) {
                // san-check
                m_Moltype   = &(*it)->GetData().GetStr();
            }
        }
        if ( !m_UO ) {
            m_UO.Reset(&uo);
        }
    }

    // complain if no accession found?  (moltype seems optional)
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
*
* $Log$
* Revision 1.6  2004/05/21 21:42:53  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.5  2003/06/02 16:06:42  dicuccio
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
* Revision 1.4  2003/03/21 18:49:17  ucko
* Turn most structs into (accessor-requiring) classes; replace some
* formerly copied fields with pointers to the original data.
*
* Revision 1.3  2003/03/11 15:37:51  kuznets
* iterate -> ITERATE
*
* Revision 1.2  2003/03/10 22:04:43  ucko
* Expand out x_AddKeys manually because MSVC wouldn't.
*
* Revision 1.1  2003/03/10 16:39:09  ucko
* Initial check-in of new flat-file generator
*
*
* ===========================================================================
*/
