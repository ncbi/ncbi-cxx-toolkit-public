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
*   flat-file generator -- comment item implementation
*
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Dbtag.hpp>
#include <objmgr/seqdesc_ci.hpp>

#include <objtools/format/formatter.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/items/comment_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// static variables initialization
bool CCommentItem::sm_FirstComment = true;

static const string kRefSeq = "REFSEQ";
static const string kRefSeqLink = "<a href=http://www.ncbi.nlm.nih.gov/RefSeq/>REFSEQ</a>";

/////////////////////////////////////////////////////////////////////////////
//
//  CCommentItem

CCommentItem::CCommentItem(CBioseqContext& ctx, bool need_period) :
    CFlatItem(&ctx), m_First(false), m_NeedPeriod(need_period)
{
    swap(m_First, sm_FirstComment);
}


CCommentItem::CCommentItem
(const string& comment,
 CBioseqContext& ctx,
 const CSerialObject* obj) :
    CFlatItem(&ctx), m_Comment(ExpandTildes(comment, eTilde_comment)),
    m_First(false), m_NeedPeriod(true)
{
    swap(m_First, sm_FirstComment);
    if ( obj != 0 ) {
        x_SetObject(*obj);
    }
}

    
CCommentItem::CCommentItem(const CSeqdesc&  desc, CBioseqContext& ctx) :
    CFlatItem(&ctx), m_First(false), m_NeedPeriod(true)
{
    swap(m_First, sm_FirstComment);
    x_SetObject(desc);
    x_GatherInfo(ctx);
    if ( m_Comment.empty() ) {
        x_SetSkip();
    }
}


CCommentItem::CCommentItem(const CSeq_feat& feat, CBioseqContext& ctx) :
    CFlatItem(&ctx), m_First(false), m_NeedPeriod(true)
{
    swap(m_First, sm_FirstComment);
    x_SetObject(feat);
    x_GatherInfo(ctx);
    TrimSpacesAndJunkFromEnds(m_Comment);
    if (m_Comment.empty()) {
        x_SetSkip();
    }       
}


void CCommentItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatComment(*this, text_os);
}


void CCommentItem::AddPeriod(void)
{
    ncbi::objects::AddPeriod(m_Comment);
}


const string& CCommentItem::GetNsAreGapsStr(void)
{
    static const string kNsAreGaps = "The strings of n's in this record represent " \
        "gaps between contigs, and the length of each string corresponds " \
        "to the length of the gap.";
    return kNsAreGaps;
}


string CCommentItem::GetStringForTPA
(const CUser_object& uo,
 CBioseqContext& ctx)
{
    static const string tpa_string = 
        "THIRD PARTY ANNOTATION DATABASE: This TPA record uses data from DDBJ/EMBL/GenBank ";

    if ( !ctx.IsTPA()  ||  ctx.IsRefSeq() ) {
        return kEmptyStr;
    }
    if ( !uo.CanGetType()  ||  !uo.GetType().IsStr()  ||  
         uo.GetType().GetStr() != "TpaAssembly" ) {
        return kEmptyStr;
    }
    
    CBioseq_Handle& seq = ctx.GetHandle();
    if (seq.IsSetInst_Hist()  &&  seq.GetInst_Hist().IsSetAssembly()) {
        return kEmptyStr;
    }

    string id;
    vector<string> accessions;
    ITERATE (CUser_object::TData, curr, uo.GetData()) {
        const CUser_field& uf = **curr;
        if ( !uf.CanGetData()  ||  !uf.GetData().IsFields() ) {
            continue;
        }

        ITERATE (CUser_field::C_Data::TFields, ufi, uf.GetData().GetFields()) {
            if( !(*ufi)->CanGetData()  ||  !(*ufi)->GetData().IsStr()  ||
                !(*ufi)->CanGetLabel() ) {
                continue;
            }
            const CObject_id& oid = (*ufi)->GetLabel();
            if ( oid.IsStr()  &&  
                 (NStr::CompareNocase(oid.GetStr(), "accession") == 0) ) {
                string acc = (*ufi)->GetData().GetStr();
                if ( !acc.empty() ) {
                    accessions.push_back(NStr::ToUpper(acc));
                }
            }
        }
    }
    if ( accessions.empty() ) {
        return kEmptyStr;
    }

    CNcbiOstrstream text;
    text << tpa_string << ((accessions.size() > 1) ? "entries " : "entry ");

    size_t size = accessions.size();
    size_t last = size - 1;

    for ( size_t i = 0; i < size; ) {
        text << accessions[i];
        ++i;
        if ( i < size ) {
            text << ((i == last) ? " and " : ", ");
        }
    }

    return CNcbiOstrstreamToString(text);
}


string CCommentItem::GetStringForBankIt(const CUser_object& uo)
{
    if ( !uo.CanGetType()  ||  !uo.GetType().IsStr()  ||
         uo.GetType().GetStr() != "Submission" ) {
        return kEmptyStr;
    }

    const string* uvc = 0, *bic = 0;
    if ( uo.HasField("UniVecComment") ) {
        const CUser_field& uf = uo.GetField("UniVecComment");
        if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
            uvc = &(uf.GetData().GetStr());
        } 
    }
    if ( uo.HasField("AdditionalComment") ) {
        const CUser_field& uf = uo.GetField("AdditionalComment");
        if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
            bic = &(uf.GetData().GetStr());
        } 
    }

    CNcbiOstrstream text;
    if ( uvc != 0  &&  bic != 0 ) {
        text << "Vector Explanation: " << *uvc << "~Bankit Comment: " << *bic;
    } else if ( uvc != 0 ) {
        text << "Vector Explanation: " << *uvc;
    } else if ( bic != 0 ) {
         text << "Bankit Comment: " << *bic;
    }

    return CNcbiOstrstreamToString(text);
}


CCommentItem::TRefTrackStatus CCommentItem::GetRefTrackStatus
(const CUser_object& uo,
 string* st)
{
    TRefTrackStatus retval = eRefTrackStatus_Unknown;
    if ( st != 0 ) {
        st->erase();
    }
    if ( !uo.HasField("Status") ) {
        return retval;
    }

    const CUser_field& field = uo.GetField("Status");
    if ( field.GetData().IsStr() ) {
        string status = field.GetData().GetStr();
        if ( status == "Inferred" ) { 
            retval = eRefTrackStatus_Inferred;
        } else if ( status == "Provisional" ) {
            retval = eRefTrackStatus_Provisional;
        } else if ( status == "Predicted" ) {
            retval = eRefTrackStatus_Predicted;
        } else if ( status == "Validated" ) {
            retval = eRefTrackStatus_Validated;
        } else if ( status == "Reviewed" ) {
            retval = eRefTrackStatus_Reviewd;
        } else if ( status == "Model" ) {
            retval = eRefTrackStatus_Model;
        } else if ( status == "WGS" ) {
            retval = eRefTrackStatus_WGS;
        }

        if ( st != 0  &&  retval != eRefTrackStatus_Unknown ) {
            *st = NStr::ToUpper(status);
        }
    }

    return retval;
}


string CCommentItem::GetStringForRefTrack
(const CUser_object& uo,
 ECommentFormat format)
{
    if ( !uo.CanGetType()  ||  !uo.GetType().IsStr()  ||
         uo.GetType().GetStr() != "RefGeneTracking" ) {
        return kEmptyStr;
    }

    string status_str;
    TRefTrackStatus status = GetRefTrackStatus(uo, &status_str);
    if ( status == eRefTrackStatus_Unknown ) {
        return kEmptyStr;
    }

    string collaborator;
    if ( uo.HasField("Collaborator") ) {
        const CUser_field& colab_field = uo.GetField("Collaborator");
        if ( colab_field.GetData().IsStr() ) {
            collaborator = colab_field.GetData().GetStr();
        }
    }

    string source;
    if ( uo.HasField("GenomicSource") ) {
        const CUser_field& source_field = uo.GetField("GenomicSource");
        if ( source_field.GetData().IsStr() ) {
            source = source_field.GetData().GetStr();
        }
    }

    const string *refseq = (format == eFormat_Html ? &kRefSeqLink : &kRefSeq);

    CNcbiOstrstream oss;
    oss << status_str << ' ' << *refseq << ": ";
    switch ( status ) {
    case eRefTrackStatus_Inferred:
        oss << "This record is predicted by genome sequence analysis and is "
            << "not yet supported by experimental evidence.";
        break;
    case eRefTrackStatus_Provisional:
        oss << "This record has not yet been subject to final NCBI review.";
        break;
    case eRefTrackStatus_Predicted:
        oss << "The mRNA record is supported by experimental evidence;"
            << "however, the coding sequence is predicted.";
        break;
    case eRefTrackStatus_Validated:
        oss << "This record has undergone preliminary review of the sequence,"
            << "but has not yet been subject to final review.";
        break;
    case eRefTrackStatus_Reviewd:
        oss << "This record has been curated by " 
            << (collaborator.empty() ? "NCBI staff" : collaborator) << '.';
        break;
    case eRefTrackStatus_Model:
        oss << "This record is predicted by automated computational analysis.";
        break;
    case eRefTrackStatus_WGS:
        oss << "This record is provided to represent a collection of "
            << "whole genome shotgun sequences.";
        break;
    default:
        break;
    }

    if ( status != eRefTrackStatus_Reviewd  &&  !collaborator.empty() ) {
        oss << "This record has been curated by " << collaborator << '.';
    }

    if ( !source.empty() ) {
        oss << "This record is derived from an annotated genomic sequence ("
            << source << ").";
    }

    vector < pair<const string*, bool> > assembly;
    if ( uo.HasField("Assembly") ) {
        const CUser_field& field = uo.GetField("Assembly");
        if ( field.GetData().IsFields() ) {
            ITERATE (CUser_field::C_Data::TFields, fit, field.GetData().GetFields()) {
                if ( !(*fit)->GetData().IsFields() ) {
                    continue;
                }
                ITERATE (CUser_field::C_Data::TFields, it, (*fit)->GetData().GetFields()) {
                    const CUser_field& uf = **it;
                    if ( !uf.CanGetLabel()  ||  !uf.GetLabel().IsStr() ) {
                        continue;
                    }
                    const string& label = uf.GetLabel().GetStr();
                    if ( label == "accession"  ||  label == "name" ) {
                        bool is_accn = (label == "accession");
                        if ( uf.GetData().IsStr()  &&  !uf.GetData().GetStr().empty() ) {
                            assembly.push_back(make_pair(&uf.GetData().GetStr(), is_accn));
                        }
                    }
                }
            }
        }
    }
    if ( assembly.size() > 0 ) {
        oss << " The reference sequence was derived from ";
        size_t assembly_size = assembly.size();
        for ( size_t i = 0; i < assembly_size; ++i ) {
            if ( i > 0  ) {
                oss << ((i < assembly_size - 1) ? ", " : " and ");
            }
            const string& acc = *(assembly[i].first);
            NcbiId(oss, acc, format == CCommentItem::eFormat_Html);
        }
        oss << '.';
    }

    return CNcbiOstrstreamToString(oss);
}


string CCommentItem::GetStringForWGS(CBioseqContext& ctx)
{
    static const string default_str = "?";

    if ( !ctx.IsWGSMaster()  ||  ctx.GetWGSMasterAccn().empty() ) {
        return kEmptyStr;
    }
    const string& wgsaccn = ctx.GetWGSMasterAccn();

    const string* taxname = 0;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_Source); it; ++it) {
        const CBioSource& src = it->GetSource();
        if ( src.CanGetOrg()  &&  src.GetOrg().CanGetTaxname() ) {
            taxname = &(src.GetOrg().GetTaxname());
        }
    }

    const string* first = 0, *last = 0;
    for (CSeqdesc_CI it(ctx.GetHandle(), CSeqdesc::e_User); it; ++it) {
        const CUser_object& uo = it->GetUser();
        if ( uo.CanGetType()  &&  uo.GetType().IsStr()  &&
             NStr::CompareNocase(uo.GetType().GetStr(), "WGSProjects") == 0 ) {
            if ( uo.HasField("WGS_accession_first") ) {
                const CUser_field& uf = uo.GetField("WGS_accession_first");
                if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
                    first = &(uf.GetData().GetStr());
                }
            }
            if ( uo.HasField("WGS_accession_last") ) {
                const CUser_field& uf = uo.GetField("WGS_accession_last");
                if ( uf.CanGetData()  &&  uf.GetData().IsStr() ) {
                    last = &(uf.GetData().GetStr());
                }
            }
        }
    }

    if ( taxname == 0  ||  taxname->empty() ) {
        taxname = &default_str;
    }
    if ( first == 0  ||  first->empty() ) {
        first = &default_str;
    }
    if ( last == 0  ||  last->empty() ) {
        last = &default_str;
    }

    string version = (wgsaccn.length() == 15) ? wgsaccn.substr(4) :
                                                wgsaccn.substr(7);

    CNcbiOstrstream text;
    text << "The " << *taxname 
         << "whole genome shotgun (WGS) project has the project accession " 
         << wgsaccn << ".  This version of the project (" << version 
         << ") has the accession number " << ctx.GetWGSMasterName() << ",";
    if ( (*first != *last) ) {
        text << " and consists of sequences " << *first << "-" << *last;
    } else {
        text << " and consists of sequence " << *first;
    }

    return CNcbiOstrstreamToString(text);
}


string CCommentItem::GetStringForMolinfo(const CMolInfo& mi, CBioseqContext& ctx)
{
    _ASSERT(mi.CanGetCompleteness());

    bool is_prot = ctx.IsProt();

    switch ( mi.GetCompleteness() ) {
    case CMolInfo::eCompleteness_complete:
        return "COMPLETENESS: full length";

    case CMolInfo::eCompleteness_partial:
        return "COMPLETENESS: not full length";

    case CMolInfo::eCompleteness_no_left:
        return (is_prot ? "COMPLETENESS: incomplete on the amino end" :
                          "COMPLETENESS: incomplete on the 5' end");

    case CMolInfo::eCompleteness_no_right:
        return (is_prot ? "COMPLETENESS: incomplete on the carboxy end" :
                          "COMPLETENESS: incomplete on the 3' end");

    case CMolInfo::eCompleteness_no_ends:
        return "COMPLETENESS: incomplete on both ends";

    case CMolInfo::eCompleteness_has_left:
        return (is_prot ? "COMPLETENESS: complete on the amino end" :
                          "COMPLETENESS: complete on the 5' end");

    case CMolInfo::eCompleteness_has_right:
        return (is_prot ? "COMPLETENESS: complete on the carboxy end" :
                          "COMPLETENESS: complete on the 3' end");

    default:
        return "COMPLETENESS: unknown";
    }

    return kEmptyStr;
}


string CCommentItem::GetStringForHTGS(CBioseqContext& ctx)
{
    SDeltaSeqSummary summary;
    if (ctx.IsDelta()) {
        GetDeltaSeqSummary(ctx.GetHandle(), summary);
    }

    CMolInfo::TTech tech = ctx.GetTech();

    CNcbiOstrstream text;

    if ( tech == CMolInfo::eTech_htgs_0 ) {
        if ( summary.num_segs > 0 ) {
            text << "* NOTE: This record contains " << (summary.num_gaps + 1) << " individual~"
                 << "* sequencing reads that have not been assembled into~"
                 << "* contigs. Runs of N are used to separate the reads~"
                 << "* and the order in which they appear is completely~"
                 << "* arbitrary. Low-pass sequence sampling is useful for~"
                 << "* identifying clones that may be gene-rich and allows~"
                 << "* overlap relationships among clones to be deduced.~"
                 << "* However, it should not be assumed that this clone~"
                 << "* will be sequenced to completion. In the event that~"
                 << "* the record is updated, the accession number will~"
                 << "* be preserved.";
        }
        text << "~";
        text << summary.text;
    } else if ( tech == CMolInfo::eTech_htgs_1 ) {
        text << "* NOTE: This is a \"working draft\" sequence.";
        if ( summary.num_segs > 0 ) {
            text << " It currently~"
                 << "* consists of " << (summary.num_gaps + 1) << " contigs. The true order of the pieces~"
                 << "* is not known and their order in this sequence record is~"
                 << "* arbitrary. Gaps between the contigs are represented as~"
                 << "* runs of N, but the exact sizes of the gaps are unknown.";
        }
        text << "~* This record will be updated with the finished sequence~"
             << "* as soon as it is available and the accession number will~"
             << "* be preserved."
             << "~"
             << summary.text;
    } else if ( tech == CMolInfo::eTech_htgs_2 ) {
        text << "* NOTE: This is a \"working draft\" sequence.";
        if ( summary.num_segs > 0 ) {
            text << " It currently~* consists of " << (summary.num_gaps + 1) 
                 << " contigs. Gaps between the contigs~"
                 << "* are represented as runs of N. The order of the pieces~"
                 << "* is believed to be correct as given, however the sizes~"
                 << "* of the gaps between them are based on estimates that have~"
                 << "* provided by the submittor.";
        }
        text << "~* This sequence will be replaced~"
             << "* by the finished sequence as soon as it is available and~"
             << "* the accession number will be preserved."
             << "~"
             << summary.text;
    } else if ( !GetTechString(tech).empty() ) {
        text << "Method: " << GetTechString(tech) << ".";
    }

    string comment = CNcbiOstrstreamToString(text);
    ConvertQuotes(comment);
    ncbi::objects::AddPeriod(comment);

    return comment;
}


string CCommentItem::GetStringForModelEvidance
(const SModelEvidance& me,
 ECommentFormat format)
{
    const string *refseq = (format == eFormat_Html ? &kRefSeqLink : &kRefSeq);

    CNcbiOstrstream text;

    text << "MODEL " << *refseq << ":  " << "This record is predicted by "
         << "automated computational analysis. This record is derived from"
         << "an annotated genomic sequence (" << me.name << ")";
    if ( !me.method.empty() ) {
        text << " using gene prediction method: " << me.method;
    }

    if ( me.mrnaEv  ||  me.estEv ) {
        text << ", supported by ";
        if ( me.mrnaEv  &&  me.estEv ) {
            text << "mRNA and EST ";
        } else if ( me.mrnaEv ) {
            text << "mRNA ";
        } else {
            text << "EST ";
        }
        // !!! for html we need much more !!!
        text << "evidence";
    }

    text << ".~Also see:~    " << 
            "Documentation of NCBI's Annotation Process~    ";

    return CNcbiOstrstreamToString(text);
}


string CCommentItem::GetStringForBarcode(CBioseqContext& ctx)
{
    CSeqdesc_CI desc_it(ctx.GetHandle(), CSeqdesc::e_Source);
    if (!desc_it) {
        return kEmptyStr;
    }
    const CBioSource& src = desc_it->GetSource();

    map<CSubSource::TSubtype, const string*> subsources;
    ITERATE (CBioSource::TSubtype, it, src.GetSubtype()) {
        const CSubSource& sub = **it;
        if (sub.IsSetSubtype()  &&  sub.IsSetName()  &&
            !NStr::IsBlank(sub.GetName())) {
            subsources[sub.GetSubtype()] = &sub.GetName();
        }
    }

    const string* taxname = NULL;
    map<COrgMod::TSubtype, const string*> orgmods;
    if (src.IsSetOrg()) {
        const COrg_ref& org = src.GetOrg();
        if (org.IsSetTaxname()  &&  !NStr::IsBlank(org.GetTaxname())) {
            taxname = &org.GetTaxname();
        }
        if (org.IsSetOrgname()) {
            ITERATE (COrgName::TMod, it, src.GetOrg().GetOrgname().GetMod()) {
                const COrgMod& omod = **it;
                if (omod.IsSetSubtype()  &&  omod.IsSetSubname()) {
                    orgmods[omod.GetSubtype()] = &omod.GetSubname();
                }
            }
        }
    }

    CNcbiOstrstream comment;

    comment << "Barcode Consortium: Standard Data Elements" << Endl() << Endl()
            << "    Organism:";
    if (taxname != NULL) {
        comment << "          " << *taxname;
    }
    comment << Endl();

    comment << "    Collected By:";
    if (subsources.find(CSubSource::eSubtype_collected_by) != subsources.end()) {
        comment << "      " << *subsources[CSubSource::eSubtype_collected_by];
    }
    comment << Endl();

    comment << "    Collection Date:";
    if (subsources.find(CSubSource::eSubtype_collection_date) != subsources.end()) {
        comment << "   " << *subsources[CSubSource::eSubtype_collection_date];
    }
    comment << Endl();

    comment << "    Country:";
    if (subsources.find(CSubSource::eSubtype_country) != subsources.end()) {
        comment << "           " << *subsources[CSubSource::eSubtype_country];
    }
    comment << Endl();

    comment << "    Identified By:";
    if (subsources.find(CSubSource::eSubtype_identified_by) != subsources.end()) {
        comment << "     " << *subsources[CSubSource::eSubtype_identified_by];
    }
    comment << Endl();

    comment << "    Isolate:";
    if (orgmods.find(COrgMod::eSubtype_isolate) != orgmods.end()) {
        comment << "           " << *orgmods[COrgMod::eSubtype_isolate];
    }
    comment << Endl();

    comment << "    Lat-Lon:";
    if (subsources.find(CSubSource::eSubtype_lat_lon) != subsources.end()) {
        comment << "           " << *subsources[CSubSource::eSubtype_lat_lon];
    }
    comment << Endl();

    comment << "    Specimen Voucher:";
    if (orgmods.find(COrgMod::eSubtype_specimen_voucher) != orgmods.end()) {
        comment << "  " << *orgmods[COrgMod::eSubtype_specimen_voucher];
    }
    comment << Endl();

    comment << "    Forward Primer:";
    if (subsources.find(CSubSource::eSubtype_fwd_primer_seq) != subsources.end()) {
        comment << "    " << *subsources[CSubSource::eSubtype_fwd_primer_seq];
    }
    comment << Endl();

    comment << "    Reverse Primer:";
    if (subsources.find(CSubSource::eSubtype_rev_primer_seq) != subsources.end()) {
        comment << "    " << *subsources[CSubSource::eSubtype_rev_primer_seq];
    }
    comment << Endl();

    comment << "    Fwd Primer Name:";
    if (subsources.find(CSubSource::eSubtype_fwd_primer_name) != subsources.end()) {
        comment << "   " << *subsources[CSubSource::eSubtype_fwd_primer_name];
    }
    comment << Endl();

    comment << "    Rev Primer Name:";
    if (subsources.find(CSubSource::eSubtype_rev_primer_name) != subsources.end()) {
        comment << "   " << *subsources[CSubSource::eSubtype_rev_primer_name];
    }
    comment << Endl() << Endl();

    return CNcbiOstrstreamToString(comment);
}


/***************************************************************************/
/*                                 PROTECTED                               */
/***************************************************************************/


void CCommentItem::x_GatherInfo(CBioseqContext& ctx)
{
    const CObject* obj = GetObject();
    if (obj == NULL) {
        return;
    }
    const CSeqdesc* desc = dynamic_cast<const CSeqdesc*>(obj);
    if (desc != NULL) {
        x_GatherDescInfo(*desc);
    } else {
        const CSeq_feat* feat = dynamic_cast<const CSeq_feat*>(obj);
        if (feat != NULL) {
            x_GatherFeatInfo(*feat, ctx);
        }
    }
}


void CCommentItem::x_GatherDescInfo(const CSeqdesc& desc)
{
    string prefix, str, suffix;
    switch ( desc.Which() ) {
    case CSeqdesc::e_Comment:
        {{
            str = desc.GetComment();
            TrimSpacesAndJunkFromEnds(str);
            ConvertQuotes(str);
            ncbi::objects::AddPeriod(str);
        }}
        break;

    case CSeqdesc::e_Maploc:
        {{
            const CDbtag& dbtag = desc.GetMaploc();
            if ( dbtag.CanGetTag() ) {
                const CObject_id& oid = dbtag.GetTag();
                if ( oid.IsStr() ) {
                    prefix = "Map location: ";
                    str = oid.GetStr();
                } else if ( oid.IsId()  &&  dbtag.CanGetDb() ) {
                    prefix = "Map location: (Database ";
                    str = dbtag.GetDb();
                    suffix = "; id # " + NStr::IntToString(oid.GetId()) + ").";
                }
            }
        }}
        break;

    case CSeqdesc::e_Region:
        {{
            prefix = "Region: ";
            str = desc.GetRegion();
        }}
        break;
    default:
        break;
    }

    if ( str.empty() ) {
        return;
    }
    x_SetCommentWithURLlinks(prefix, str, suffix);
    
}


void CCommentItem::x_GatherFeatInfo(const CSeq_feat& feat, CBioseqContext& ctx)
{
    if (!feat.GetData().IsComment()  ||
        !feat.CanGetComment()        ||
        NStr::IsBlank(feat.GetComment())) {
        return;
    }

    x_SetCommentWithURLlinks(kEmptyStr, feat.GetComment(), kEmptyStr);
}


void CCommentItem::x_SetSkip(void)
{
    CFlatItem::x_SetSkip();
    swap(m_First, sm_FirstComment);
}


void CCommentItem::x_SetComment(const string& comment)
{
    m_Comment = ExpandTildes(comment, eTilde_comment);;
}


void CCommentItem::x_SetCommentWithURLlinks
(const string& prefix,
 const string& str,
 const string& suffix)
{
    // !!! test for html - find links within the comment string
    string comment = ExpandTildes(prefix + str + suffix, eTilde_comment);
    if (comment.empty()) {
        return;
    }

    size_t pos = comment.find_last_not_of(" \n\t\r.~");
    if (pos != comment.length() - 1) {
        size_t period = comment.find_last_of('.');
        bool add_period = period > pos;
        if (add_period  &&  !NStr::EndsWith(str, "...")) {
            ncbi::objects::AddPeriod(comment);
        }
    }
    
    m_Comment = comment;
}


/////////////////////////////////////////////////////////////////////////////
//
// Derived Classes

// --- CGenomeAnnotComment

CGenomeAnnotComment::CGenomeAnnotComment
(CBioseqContext& ctx,
 const string& build_num) :
    CCommentItem(ctx), m_GenomeBuildNumber(build_num)
{
    x_GatherInfo(ctx);
}


void CGenomeAnnotComment::x_GatherInfo(CBioseqContext& ctx)
{
    const string *refseq = ctx.Config().DoHTML() ? &kRefSeqLink : &kRefSeq;

    CNcbiOstrstream text;

    text << "GENOME ANNOTATION " << *refseq << ":  ";
    if ( !m_GenomeBuildNumber.empty() ) {
         text << "Features on this sequence have been produced for build "
              << m_GenomeBuildNumber << " of the NCBI's genome annotation"
              << " [see " << "documentation" << "].";
    } else {
        text << "NCBI contigs are derived from assembled genomic sequence data."
             << "~Also see:~    " << "Documentation" 
             << " of NCBI's Annotation Process~ ";
    }
    
    x_SetComment(CNcbiOstrstreamToString(text));
}


// --- CHistComment

CHistComment::CHistComment
(EType type,
 const CSeq_hist& hist,
 CBioseqContext& ctx) : 
    CCommentItem(ctx), m_Type(type), m_Hist(&hist)
{
    x_GatherInfo(ctx);
    m_Hist.Reset();
}


string s_CreateHistCommentString
(const string& prefix,
 const string& suffix,
 const CSeq_hist_rec& hist,
 bool do_html)
{
    //if (!hist.CanGetDate()  ||  !hist.CanGetIds()) {
    //    return "???";
    //}

    string date;
    if (hist.IsSetDate()) {
        hist.GetDate().GetDate(&date, "%{%3N%|???%} %{%D%|??%}, %{%4Y%|????%}");
    }

    vector<int> gis;
    ITERATE (CSeq_hist_rec::TIds, id, hist.GetIds()) {
        if ( (*id)->IsGi() ) {
            gis.push_back((*id)->GetGi());
        }
    }

    CNcbiOstrstream text;

    text << prefix << ((gis.size() > 1) ? " or before " : " ") << date 
         << ' ' << suffix;

    if ( gis.empty() ) {
        text << " gi:?";
        return CNcbiOstrstreamToString(text);
    }

    for ( size_t count = 0; count < gis.size(); ++count ) {
        if ( count != 0 ) {
            text << ",";
        }
        text << " gi:";
        NcbiId(text, gis[count], do_html);
    }
    text << '.' << endl;

    return CNcbiOstrstreamToString(text);
}


void CHistComment::x_GatherInfo(CBioseqContext& ctx)
{
    _ASSERT(m_Hist);

    switch ( m_Type ) {
    case eReplaced_by:
        x_SetComment(s_CreateHistCommentString(
            "[WARNING] On",
            "this sequence was replaced by a newer version",
            m_Hist->GetReplaced_by(),
            ctx.Config().DoHTML()));
        break;
    case eReplaces:
        x_SetComment(s_CreateHistCommentString(
            "On",
            "this sequence version replaced",
            m_Hist->GetReplaces(),
            ctx.Config().DoHTML()));
        break;
    }
}


// --- CGsdbComment

CGsdbComment::CGsdbComment(const CDbtag& dbtag, CBioseqContext& ctx) :
    CCommentItem(ctx), m_Dbtag(&dbtag)
{
    x_GatherInfo(ctx);
}


void CGsdbComment::x_GatherInfo(CBioseqContext&)
{
    if (m_Dbtag->IsSetTag()  &&  m_Dbtag->GetTag().IsId()) {
        string id = NStr::IntToString(m_Dbtag->GetTag().GetId());
        x_SetComment("GSDB:S:" + id);
    } else {
        x_SetSkip();
    }
}


// --- CLocalIdComment

CLocalIdComment::CLocalIdComment(const CObject_id& oid, CBioseqContext& ctx) :
    CCommentItem(ctx, false), m_Oid(&oid)
{
    x_GatherInfo(ctx);
}


void CLocalIdComment::x_GatherInfo(CBioseqContext&)
{
    CNcbiOstrstream msg;

    switch ( m_Oid->Which() ) {
    case CObject_id::e_Id:
        msg << "LocalID: " << m_Oid->GetId();    
        break;
    case CObject_id::e_Str:
        if ( m_Oid->GetStr().length() < 100 ) {
            msg << "LocalID: " << m_Oid->GetStr();
        } else {
            msg << "LocalID string too large";
        }
        break;
    default:
        break;
    }
    x_SetComment(CNcbiOstrstreamToString(msg));
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.16  2005/02/09 14:55:37  shomrat
* initial support for HTML output
*
* Revision 1.15  2005/02/02 19:35:29  shomrat
* Added barcode comment
*
* Revision 1.14  2005/01/12 16:44:04  shomrat
* Removed NsAreGaps; fixed history date
*
* Revision 1.13  2004/10/18 18:54:34  shomrat
* cleanup desc comments
*
* Revision 1.12  2004/10/05 15:36:56  shomrat
* Fixed comment generation
*
* Revision 1.11  2004/08/30 13:36:01  shomrat
* fixed comment formatting
*
* Revision 1.10  2004/08/19 16:27:48  shomrat
* Added constructor from Seq-feat
*
* Revision 1.9  2004/08/12 15:48:42  shomrat
* do not force 2 digit day in date format
*
* Revision 1.8  2004/05/21 21:42:54  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.7  2004/05/06 17:45:43  shomrat
* + CLocalIdComment
*
* Revision 1.6  2004/04/22 15:51:13  shomrat
* Changes in context
*
* Revision 1.5  2004/04/13 16:45:22  shomrat
* Fixed comment cleanup
*
* Revision 1.4  2004/03/26 17:22:51  shomrat
* Minor fixes to comment string
*
* Revision 1.3  2004/03/05 18:44:05  shomrat
* fixed RefTRack comments
*
* Revision 1.2  2003/12/18 17:43:31  shomrat
* context.hpp moved
*
* Revision 1.1  2003/12/17 20:18:17  shomrat
* Initial Revision (adapted from flat lib)
*
*
* ===========================================================================
*/

