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
* Author:  Mati Shomrat, NCBI
*
* File Description:
*   Primary item for flat-file
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqalign/Seq_align_set.hpp>
#include <objtools/alnmgr/alnmap.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>
#include <objtools/alnmgr/alnmap.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


CPrimaryItem::CPrimaryItem(CBioseqContext& ctx) :
    CFlatItem(&ctx)
{
    x_GatherInfo(ctx);
    if ( m_Str.empty() ) {
        x_SetSkip();
    }
}


void CPrimaryItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const

{
    formatter.FormatPrimary(*this, text_os);
}


static bool s_IsTPA(CBioseqContext& ctx, bool has_tpa_assembly)
{
    ITERATE (CBioseq::TId, it, ctx.GetBioseqIds()) {
        const CSeq_id& id = **it;
        switch ( id.Which() ) {
        case CSeq_id::e_Tpg:
        case CSeq_id::e_Tpe:
        case CSeq_id::e_Tpd:
            return true;
        case CSeq_id::e_Local:
            return has_tpa_assembly;
        case CSeq_id::e_General:
            if ( id.GetGeneral().CanGetDb() ) {
                const string& db = id.GetGeneral().GetDb();
                if ( db == "BankIt"  ||  db == "TMSMART" ) {
                    return has_tpa_assembly;
                }
            }
            break;
        default:
            break;
        }
    }
    return false;
}


void CPrimaryItem::x_GatherInfo(CBioseqContext& ctx)
{
    const CUser_object* uo = 0;
    for ( CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_User); desc; ++desc ) {
        const CUser_object& o = desc->GetUser();
        if ( o.CanGetType()  &&  o.GetType().IsStr()  &&
             o.GetType().GetStr() == "TpaAssembly" ) {
            uo = &o;
            break;
        }
    }
    if ( !s_IsTPA(ctx, uo != 0) ) {
        return;
    }
    CBioseq_Handle& seq = ctx.GetHandle();
    if ( seq.IsSetInst_Hist()  &&  !seq.GetInst_Hist().GetAssembly().empty() ) {
        x_GetStrForPrimary(ctx);   
    }
}


static const char* s_PrimaryHeader(bool is_refseq)
{
    return is_refseq ?
        "REFSEQ_SPAN         PRIMARY_IDENTIFIER PRIMARY_SPAN        COMP" :
        "TPA_SPAN            PRIMARY_IDENTIFIER PRIMARY_SPAN        COMP";
}



void CPrimaryItem::x_GetStrForPrimary(CBioseqContext& ctx)
{
    CBioseq_Handle& seq = ctx.GetHandle();

    TDense_seg_Map segmap;
    x_CollectSegments(segmap, seq.GetInst_Hist().GetAssembly());
    
    string str;
    string s;
    CConstRef<CSeq_id> other_id;

    ITERATE (TDense_seg_Map, it, segmap) {
        s.erase();
        CAlnMap alnmap(*it->second);
        
        s += NStr::IntToString(alnmap.GetSeqStart(0) + 1) + '-' +
             NStr::IntToString(alnmap.GetSeqStop(0) + 1);
        s.resize(20, ' ');
        other_id.Reset(&alnmap.GetSeqId(1));
        if (!other_id) {
            continue;
        }
        if (other_id->IsGi()) {
            other_id = GetId(*other_id, ctx.GetScope(), eGetId_Best).GetSeqId();
            if (other_id->IsGi()) {
                continue;
            }
        }
        if (other_id->IsGeneral()) {
            const CDbtag& dbt = other_id->GetGeneral();
            if (dbt.IsSetDb()  &&  NStr::EqualNocase(dbt.GetDb(), "TI")) {
                s += "TI";
            }
        }
        s += other_id->GetSeqIdString(true);
        s.resize(39, ' ');
        s += NStr::IntToString(alnmap.GetSeqStart(1) + 1) + '-' +
            NStr::IntToString(alnmap.GetSeqStop(1) + 1);
        if (alnmap.IsNegativeStrand(0)  ||  alnmap.IsNegativeStrand(1)) {
            if (!(alnmap.IsNegativeStrand(0)  &&  alnmap.IsNegativeStrand(1))) {
                s.resize(59, ' ');
                s += 'c';
            }
        }

        if (!s.empty()) {
            str += '\n';
            str+= s;
        }
    }

    if (!str.empty()) {
        m_Str = s_PrimaryHeader(ctx.IsRefSeq());
        m_Str += str;
    }
}


void CPrimaryItem::x_CollectSegments
(TDense_seg_Map& segmap,
 const TAlnList& aln_list)
{
    ITERATE (TAlnList, it, aln_list) {
        x_CollectSegments(segmap, **it);
    }
}


void CPrimaryItem::x_CollectSegments
(TDense_seg_Map& segmap, const CSeq_align& aln)
{
    if ( !aln.CanGetSegs() ) {
        return;
    }

    const CSeq_align::C_Segs& segs = aln.GetSegs();
    if ( segs.IsDenseg() ) {
        const CDense_seg& dseg = segs.GetDenseg();
        CAlnMap alnmap(dseg);
        segmap.insert(TDense_seg_Map::value_type(alnmap.GetSeqRange(0), TDenseRef(&dseg)));
    } else if ( segs.IsDisc() ) {
        x_CollectSegments(segmap, segs.GetDisc().Get());
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.8  2005/02/17 15:58:42  grichenk
* Changes sequence::GetId() to return CSeq_id_Handle
*
* Revision 1.7  2004/10/18 18:47:11  shomrat
* Use sequence::GetId
*
* Revision 1.6  2004/10/05 15:47:28  shomrat
*  USe CScope::GetIds
*
* Revision 1.5  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.4  2004/05/06 17:58:28  shomrat
* Do not add spaces if plus strand
*
* Revision 1.3  2004/04/22 15:57:36  shomrat
* New implementation of Primary item
*
* Revision 1.2  2003/12/18 17:43:35  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:23:55  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/
