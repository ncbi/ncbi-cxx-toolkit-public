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
#include <objmgr/scope.hpp>
#include <objmgr/seqdesc_ci.hpp>
#include <objmgr/util/sequence.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/primary_item.hpp>
#include <objtools/format/context.hpp>
#include <objmgr/util/objutil.hpp>


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


static bool s_IsTPA(CBioseqContext& ctx, bool has_tpa_assembly )
{
    bool has_bankit = false;
    bool has_genbank = false;
    bool has_gi = false;
    bool has_local = false;
    bool has_refseq = false;
    bool has_smart = false;
    bool has_tpa = false;
    bool is_tsa = false;

    ITERATE (CBioseq::TId, it, ctx.GetBioseqIds()) {
        switch ( (*it)->Which() ) {
      case CSeq_id::e_Local:
          has_local = true;
          break;
      case CSeq_id::e_Genbank:
      case CSeq_id::e_Embl:
      case CSeq_id::e_Ddbj:
          has_genbank = true;
          break;
      case CSeq_id::e_Other:
          has_refseq = true;
          break;
      case CSeq_id::e_Gi:
          has_gi = true;
          break;
      case CSeq_id::e_Tpg:
      case CSeq_id::e_Tpe:
      case CSeq_id::e_Tpd:
          has_tpa = true;
          break;
      case CSeq_id::e_General:
          if ( (*it)->GetGeneral().CanGetDb() ) {
              const string& db = (*it)->GetGeneral().GetDb();
              if ( NStr::EqualNocase(db, "BankIt") ) {
                  has_bankit = true;
              }
              if ( NStr::EqualNocase(db, "TMSMART") ) {
                  has_smart = true;
              }
          }
          break;
      default :
          break;
        }
    }

    if( ctx.GetTech() == CMolInfo::eTech_tsa ) {
        is_tsa = true;
    }

    if (is_tsa) return true;
    if (has_genbank) return false;
    if (has_tpa) return true;
    if (has_refseq) return true;
    if (has_bankit && has_tpa_assembly) return true;
    if (has_smart && has_tpa_assembly) return true;
    if (has_gi) return false;
    if (has_local && has_tpa_assembly) return true;

    return false;
}


void CPrimaryItem::x_GatherInfo(CBioseqContext& ctx)
{
    bool has_tpa_assembly = false;
    bool has_tsa = false;
    for ( CSeqdesc_CI desc(ctx.GetHandle(), CSeqdesc::e_User);
          desc  &&  !has_tpa_assembly  &&  !has_tsa;
          ++desc ) {
        const CUser_object& o = desc->GetUser();
        if ( o.CanGetType()  &&  o.GetType().IsStr()) {
             if (o.GetType().GetStr() == "TpaAssembly" ) {
                 has_tpa_assembly = true;
             }
             else if (o.GetType().GetStr() == "TSA") {
                 has_tsa = true;
             }
        }
    }

    if (has_tsa) {
        /// FIXME:
        /// do TSA thingies here
        return;
    }

    CBioseq_Handle& seq = ctx.GetHandle();
    bool has_hist_assembly =
        seq.IsSetInst_Hist()  &&  !seq.GetInst_Hist().GetAssembly().empty();

    if ( !s_IsTPA(ctx, has_tpa_assembly) || !has_hist_assembly ) {
        return;
    }
    if ( seq.IsSetInst_Hist()  &&  !seq.GetInst_Hist().GetAssembly().empty() ) {
        x_GetStrForPrimary(ctx);   
    }
}


static const char* s_PrimaryHeader( CBioseqContext &ctx )
{
    if( ctx.IsRefSeq() ) {
        return "REFSEQ_SPAN         PRIMARY_IDENTIFIER PRIMARY_SPAN        COMP" ;
    } else if( ctx.GetTech() == CMolInfo::eTech_tsa ) {
        return "TSA_SPAN            PRIMARY_IDENTIFIER PRIMARY_SPAN        COMP";
    } else {
        return "TPA_SPAN            PRIMARY_IDENTIFIER PRIMARY_SPAN        COMP";
    }        
}



void CPrimaryItem::x_GetStrForPrimary(CBioseqContext& ctx)
{
    CBioseq_Handle& seq = ctx.GetHandle();


    TAlnConstList seglist;
    x_CollectSegments(seglist, seq.GetInst_Hist().GetAssembly());
    
    string str;
    string s;
    s.reserve(80);
    CConstRef<CSeq_id> other_id;

    TSignedSeqPos last_stop = -1;

    ITERATE( TAlnConstList, it, seglist ) {
        s.erase();
        const CSeq_align& align = **it;

        TSeqPos this_start = align.GetSeqStart(0);
        TSeqPos this_stop = align.GetSeqStop(0);

        if ( ctx.IsRefSeq() && last_stop > -1 &&  (this_start > (last_stop + 1)) ) {
            if (this_start < (15 + last_stop) ) {
                s += NStr::IntToString(last_stop + 2) + '-' +
                    NStr::IntToString(this_start);
                s.resize(20, ' ');
                s += '"';

                string ss;
                CSeqVector v(seq, CBioseq_Handle::eCoding_Iupac);
                v.GetSeqData(last_stop + 1, this_start, ss);
                s += ss;
                s += '"';
                s.resize(39, ' ');

                s += "1-" + NStr::IntToString(this_start - last_stop - 1);
            } else {
                s += NStr::IntToString(last_stop + 2) + '-' +
                    NStr::IntToString(this_start);
                s.resize(20, ' ');
                s += '"';

                string ss;
                CSeqVector v(seq, CBioseq_Handle::eCoding_Iupac);
                v.GetSeqData(last_stop + 1, last_stop + 5, ss);
                s += ss;
                s += "...";

                v.GetSeqData(this_start - 4, this_start, ss);
                s += ss;
                s += '"';
                s.resize(39, ' ');

                s += "1-" + NStr::IntToString(this_start - last_stop - 1);
            }

            str += '\n';
            str += s;
            s.erase();

            ctx.ThrowIfCanceled();
        }
        last_stop = this_stop;

        s += NStr::IntToString(this_start + 1) + '-' +
             NStr::IntToString(this_stop + 1);
        s.resize(20, ' ');
        other_id.Reset(&align.GetSeq_id(1));
        if (!other_id) {
            continue;
        }
        if (other_id->IsGi()) {

            // don't show PRIMARY line if network access unavailable (and hence can't translate gi)
            CSeq_id_Handle idh = GetId(*other_id, ctx.GetScope(), eGetId_Best);
            if( ! idh ) {
                return;
            }

            other_id = idh.GetSeqId();
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
        s += NStr::IntToString(align.GetSeqStart(1) + 1) + '-' +
            NStr::IntToString(align.GetSeqStop(1) + 1);

        ENa_strand s0 = align.GetSeqStrand(0);
        ENa_strand s1 = align.GetSeqStrand(1);
        if (s0 != s1) {
            s.resize(59, ' ');
            s += 'c';
        }

        if (!s.empty()) {
            str += '\n';
            str+= s;
        }
    }

    if (!str.empty()) {
        m_Str = s_PrimaryHeader(ctx);
        m_Str += str;
    }
}


void CPrimaryItem::x_CollectSegments
(TAlnConstList& seglist,
 const TAlnList& aln_list)
{
    ITERATE (TAlnList, it, aln_list) {
        x_CollectSegments(seglist, **it);
    }
}


void CPrimaryItem::x_CollectSegments
(TAlnConstList& seglist, const CSeq_align& aln)
{
    if ( !aln.CanGetSegs() ) {
        return;
    }

    if ( aln.GetSegs().IsDenseg() ) {
        seglist.push_back( TAln(&aln) );
    } else if ( aln.GetSegs().IsDisc() ) {
        x_CollectSegments(seglist, aln.GetSegs().GetDisc().Get());
    }
}



END_SCOPE(objects)
END_NCBI_SCOPE
