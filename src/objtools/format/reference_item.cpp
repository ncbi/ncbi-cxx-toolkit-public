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
*          Mati Shomrat
*
* File Description:
*   flat-file generator -- bibliographic references
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <serial/iterator.hpp>
#include <util/static_set.hpp>
#include <objects/biblio/biblio__.hpp>
#include <objects/general/Name_std.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/general/Date.hpp>
#include <objects/general/Date_std.hpp>
#include <objects/medline/Medline_entry.hpp>
#include <objects/mla/mla_client.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub_set.hpp>
#include <objects/seqloc/Patent_seq_id.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/biblio/Imprint.hpp>
#include <objects/submit/Submit_block.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/seq_loc_util.hpp>

#include <algorithm>

#include <objtools/format/flat_expt.hpp>
#include <objtools/format/text_ostream.hpp>
#include <objtools/format/formatter.hpp>
#include <objtools/format/items/reference_item.hpp>
#include <objtools/format/context.hpp>
#include "utils.hpp"


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
USING_SCOPE(sequence);


/////////////////////////////////////////////////////////////////////////////
//
// LessThan - predicate class for sorting references

class LessThan
{
public:
    enum ESerialFirst {
        eSerialFirst_No = 0,
        eSerialFirst_Yes
    };
    LessThan(ESerialFirst serial_first, bool is_refseq);
    bool operator()(const CRef<CReferenceItem>& ref1, const CRef<CReferenceItem>& ref2);
private:
    ESerialFirst m_SerialFirst;
    bool         m_IsRefSeq;
};

/////////////////////////////////////////////////////////////////////////////


void CReferenceItem::FormatAffil(const CAffil& affil, string& result, bool gen_sub)
{
    result.erase();

    if (affil.IsStr()) {
        result = affil.GetStr();
    } else if (affil.IsStd()) {
        const CAffil::C_Std& std = affil.GetStd();
        if (gen_sub) {
            if (std.IsSetDiv()) {
                result = std.GetDiv();
            }
            if (std.IsSetAffil()) {
                if (!result.empty()) {
                    result += ", ";
                }
                result += std.GetAffil();
            }
            
        } else {
            if (std.IsSetAffil()) {
                result = std.GetAffil();
            }
            if (std.IsSetDiv()) {
                if (!result.empty()) {
                    result += ", ";
                }
                result = std.GetDiv();
            } 
        }
        if (std.IsSetStreet()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetStreet();
        }
        if (std.IsSetCity()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetCity();
        }
        if (std.IsSetSub()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetSub();
        }
        if (gen_sub  &&  std.IsSetPostal_code()) {
            if (!result.empty()) {
                result += ' ';
            }
            result += std.GetPostal_code();
        }
        if (std.IsSetCountry()) {
            if (!result.empty()) {
                result += ", ";
            }
            result += std.GetCountry();
        }
    }
    if (gen_sub) {
        ConvertQuotes(result);
    }
    NStr::TruncateSpacesInPlace(result);
}


CReferenceItem::CReferenceItem(const CSeqdesc& desc, CBioseqContext& ctx) :
    CFlatItem(&ctx), m_PubType(ePub_not_set), m_Category(eUnknown),
    m_PatentId(0), m_PMID(0), m_MUID(0), m_Serial(kMax_Int),
    m_JustUids(true), m_Elect(false)
{
    _ASSERT(desc.IsPub());
    
    x_SetObject(desc.GetPub());
    m_Pubdesc.Reset(&(desc.GetPub()));

    if (ctx.GetMapper() != NULL) {
        m_Loc.Reset(ctx.GetMapper()->Map(ctx.GetLocation()));
    } else {
        m_Loc.Reset(&ctx.GetLocation());
    }

    x_GatherInfo(ctx);
}


CReferenceItem::CReferenceItem
(const CSeq_feat& feat,
 CBioseqContext& ctx,
 const CSeq_loc* loc) :
    CFlatItem(&ctx), m_PubType(ePub_not_set), m_Category(eUnknown),
    m_PatentId(0), m_PMID(0), m_MUID(0), m_Serial(kMax_Int),
    m_JustUids(true), m_Elect(false)
{
    _ASSERT(feat.GetData().IsPub());

    x_SetObject(feat);

    m_Pubdesc.Reset(&(feat.GetData().GetPub()));
    
    if (loc != NULL) {
        m_Loc.Reset(loc);
    } else if ( ctx.GetMapper() != 0 ) {
        m_Loc.Reset(ctx.GetMapper()->Map(feat.GetLocation()));
    } else {
        m_Loc.Reset(&(feat.GetLocation()));
    }
    // cleanup location
    const bool is_circular = ( ctx.GetHandle().CanGetInst_Topology() && ctx.GetHandle().GetInst_Topology() == CSeq_inst_Base::eTopology_circular );
    CSeq_loc::EOpFlags merge_flags = ( is_circular ? CSeq_loc::fMerge_All : CSeq_loc::fSortAndMerge_All );
    m_Loc = Seq_loc_Merge(*m_Loc, merge_flags, &ctx.GetScope());

    x_GatherInfo(ctx);
}


CReferenceItem::CReferenceItem(const CSubmit_block& sub, CBioseqContext& ctx) :
    CFlatItem(&ctx), m_PubType(ePub_sub), m_Category(eSubmission),
    m_PatentId(0), m_PMID(0), m_MUID(0), m_Serial(kMax_Int),
    m_JustUids(false), m_Elect(false)
{
    x_SetObject(sub);

    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->SetWhole(*ctx.GetPrimaryId());
    m_Loc = loc;

    if (sub.IsSetCit()) {
        m_Sub.Reset(&sub.GetCit());
        m_Title = "Direct Submission";
        m_PubType = ePub_sub;
        if (m_Sub->IsSetAuthors()) {
            m_Authors.Reset(&m_Sub->GetAuthors());
        }
        if (m_Sub->IsSetDate()) {
            m_Date.Reset(&m_Sub->GetDate());
        }
    } else {
        x_SetSkip();
    }
}


CReferenceItem::~CReferenceItem() {
}


void CReferenceItem::SetLoc(const CConstRef<CSeq_loc>& loc)
{
    m_Loc = loc;
}

void CReferenceItem::SetRemark( const CPubdesc::TFig* new_fig,
    const CPubdesc::TMaploc *new_maploc,
    const CPubdesc::TPoly_a *new_poly_a ) 
{
    _ASSERT( m_Pubdesc.NotEmpty() && GetContext() );

    CRef<CPubdesc> new_pubdesc( new CPubdesc() );
    new_pubdesc->Assign( *m_Pubdesc );

    if( new_fig ) {
        new_pubdesc->SetFig( *new_fig );
    }
    if( new_maploc ) {
        new_pubdesc->SetMaploc( *new_maploc );
    }
    if( new_poly_a ) {
        new_pubdesc->SetPoly_a( *new_poly_a );
    }

    m_Pubdesc.Reset( new_pubdesc );

    // The above changes only affect m_Remark, so
    // there's less to do than otherwise expected
    // for a change to m_Pubdesc
    x_GatherRemark( *GetContext() );
}

static bool s_ShouldRemoveRef
(const CReferenceItem& prev_ref,
 const CReferenceItem& curr_ref)
{
    // to remove, the references must overlap (or at least abut)
    {{
        // they overlap (or at least abut)
        if( ! prev_ref.GetLoc().IsWhole() && ! curr_ref.GetLoc().IsWhole() ) {
            const TSeqPos prev_stop = prev_ref.GetLoc().GetStop(eExtreme_Positional);
            const TSeqPos curr_start = curr_ref.GetLoc().GetStart(eExtreme_Positional);
            if( (prev_stop == kInvalidSeqPos) || (curr_start == kInvalidSeqPos) ) {
                // invalid start/stop
                return false;
            }

            if( ! ( prev_stop + 1 >= curr_start ) ) {
                return false;
            }
        }
    }}

    // same PMID ( and overlap )
    if( curr_ref.GetPMID() != 0 && prev_ref.GetPMID() != 0 ) {
        return ( curr_ref.GetPMID() == prev_ref.GetPMID() );
    }
        
    // same MUID ( and overlap )
    if( curr_ref.GetMUID() != 0 && prev_ref.GetMUID() != 0 ) {
        return ( curr_ref.GetMUID() == prev_ref.GetMUID() );
    }

    // next use AUTHOR string
    string auth1, auth2;
    if (curr_ref.IsSetAuthors()) {
        CReferenceItem::FormatAuthors(curr_ref.GetAuthors(), auth1);
    }
    if (prev_ref.IsSetAuthors()) {
        CReferenceItem::FormatAuthors(prev_ref.GetAuthors(), auth2);
    }
    const bool authorsSame = NStr::EqualNocase(auth1, auth2);

    const string& curr_unique_str = curr_ref.GetUniqueStr();
    const string& prev_unique_str = prev_ref.GetUniqueStr();
    const bool locationsEqual = curr_ref.GetLoc().Equals( prev_ref.GetLoc() );
    if( NStr::EqualNocase( curr_unique_str, prev_unique_str ) &&
        locationsEqual &&
        authorsSame ) {
            return true;
    }

    return false;
}

static void s_CombineRefs
( CReferenceItem& prev_ref,
  CReferenceItem& curr_ref,
  CBioseqContext &ctx)
{
    // merge locations
    {{
        CConstRef<CSeq_loc> prev_loc( &prev_ref.GetLoc() );
        CConstRef<CSeq_loc> curr_loc( &curr_ref.GetLoc() );

        CConstRef<CSeq_id> prev_id( prev_loc->GetId() );
        CConstRef<CSeq_id> curr_id( curr_loc->GetId() );

        const bool is_circular = 
            ( ctx.GetHandle().CanGetInst_Topology() && ctx.GetHandle().GetInst_Topology() == CSeq_inst_Base::eTopology_circular );

        CRef<CSeq_loc> new_loc = Seq_loc_Add( *prev_loc, *curr_loc,
            ( is_circular ? CSeq_loc::fMerge_All : CSeq_loc::fSortAndMerge_All ),
            &prev_ref.GetContext()->GetScope() );

        // save the old id because sometimes the merging changes it
        // We check for sameness to make sure it's reasonable to do this.
        if( prev_id && curr_id && prev_id->Equals( *curr_id ) ) {
            new_loc->SetId( *prev_id );
        }

        prev_ref.SetLoc( new_loc );
    }}

    // most merging ops are only done if muid or pmid match
    const bool same_muid = ( curr_ref.GetMUID() != 0 && (prev_ref.GetMUID() == curr_ref.GetMUID()) );
    const bool same_pmid = ( curr_ref.GetPMID() != 0 && (prev_ref.GetPMID() == curr_ref.GetPMID()) );
    if( (same_muid || same_pmid) &&
        ( prev_ref.GetRemark() != curr_ref.GetRemark() )  ) 
    {
        const CPubdesc& prev_pubdesc = prev_ref.GetPubdesc();
        const CPubdesc& curr_pubdesc = curr_ref.GetPubdesc();

        const CPubdesc::TFig* new_fig = ( (! prev_pubdesc.IsSetFig() && curr_pubdesc.IsSetFig()) ? &curr_pubdesc.GetFig() : NULL );
        const CPubdesc::TMaploc *new_maploc = ( (! prev_pubdesc.IsSetMaploc() && curr_pubdesc.IsSetMaploc()) ? &curr_pubdesc.GetMaploc() : NULL );
        // the "false" is arbitrary and won't get ever used
        const CPubdesc::TPoly_a new_poly_a =
            ( curr_pubdesc.IsSetPoly_a() ? curr_pubdesc.GetPoly_a() : false );

        prev_ref.SetRemark(
            new_fig,
            new_maploc,
            ( (! prev_pubdesc.IsSetPoly_a() && curr_pubdesc.IsSetPoly_a()) ? &new_poly_a : NULL )
        );
    }
}

static void s_MergeDuplicates
(CReferenceItem::TReferences& refs,
 CBioseqContext& ctx)
{
    /**
    static const CSeq_loc::TOpFlags kMergeFlags = 
        CSeq_loc::fSort | CSeq_loc::fStrand_Ignore | CSeq_loc::fMerge_All;
        **/

    if ( refs.size() < 2 ) {
        return;
    }

    // for EMBL and DDBJ, we don't remove refs
    if( ctx.IsEMBL() || ctx.IsDDBJ() ) {
        return;
    }

    // see if merging is allowed
    if( ! ctx.CanGetTLSeqEntryCtx() || 
        ! ctx.GetTLSeqEntryCtx().GetCanSourcePubsBeFused() )
    {
      return;
    }

    CReferenceItem::TReferences::iterator curr = refs.begin();

    while ( curr != refs.end() ) {
        if ( !*curr ) {
            curr = refs.erase(curr);
            continue;
        }
        _ASSERT(*curr);

        bool remove = false;
        bool combine_allowed = true;

        CReferenceItem& curr_ref = **curr;
        if ( curr_ref.IsJustUids() ) {
            remove = true;
        } else {

            // EMBL patent records do not need author or title - A29528.1
            // if ( !(ctx.IsEMBL()  &&  ctx.IsPatent())  ) {

                // do not allow no author reference to appear by itself - U07000.1
                if( !curr_ref.IsSetAuthors() ) {
                    remove = true;
                    combine_allowed = false;
                } else {
                    // GenPept RefSeq suppresses cit-subs
                    if( ctx.IsRefSeq() && ctx.IsProt() && curr_ref.GetCategory() == CReferenceItem::eSubmission ) {
                        remove = true;
                        combine_allowed = false;
                    }
                }

            // }
        }

        // check for duplicate references (if merging is allowed)
        if( curr != refs.begin() ) {
            CReferenceItem& prev_ref = **(curr-1);

            if( prev_ref.GetReftype() == CPubdesc::eReftype_seq && 
                    curr_ref.GetReftype() != CPubdesc::eReftype_seq ) {
                combine_allowed = false;
            }

            if( s_ShouldRemoveRef( prev_ref, curr_ref ) ) {
                if( combine_allowed ) {
                    s_CombineRefs( prev_ref, curr_ref, ctx );
                }
                remove = true;
            }
        }

        if (remove) {
            curr = refs.erase(curr);
        } else {
            ++curr;
        }
    }
}


void CReferenceItem::Rearrange(TReferences& refs, CBioseqContext& ctx)
{
    {{
        stable_sort(refs.begin(), refs.end(), 
            LessThan(LessThan::eSerialFirst_No, ctx.IsRefSeq()));
    }}

    {{
        // merge duplicate references (except for dump mode)
        if (!ctx.Config().IsModeDump()) {
            s_MergeDuplicates(refs, ctx);
        }
    }}

    {{
        // !!! add submit reference
    }}

    {{
        // re-sort, take serial number into consideration.
        stable_sort(refs.begin(), refs.end(), 
            LessThan(LessThan::eSerialFirst_Yes, ctx.IsRefSeq()));
    }}
    
    // assign final serial numbers
    size_t size = refs.size();
    int current_serial = 1;
    for ( size_t i = 0;  i < size; ++i ) {
        if (refs[i]->m_Serial != kMax_Int) {
            current_serial = refs[i]->m_Serial + 1;
        }
        else {
            refs[i]->m_Serial = current_serial++;
        }
    }
}


void CReferenceItem::Format
(IFormatter& formatter,
 IFlatTextOStream& text_os) const
{
    formatter.FormatReference(*this, text_os);
}


bool CReferenceItem::Matches(const CPub_set& ps) const
{
    if ( !ps.IsPub() ) {
        return false;
    }

    ITERATE (CPub_set::TPub, it, ps.GetPub()) {
        if ( Matches(**it) ) {
            return true;
        }
    }
    return false;
}


static bool s_IsOnlySerial(const CPub& pub)
{
    if (!pub.IsGen()) {
        return false;
    }

    const CCit_gen& gen = pub.GetGen();

    if ( !gen.IsSetCit() ) 
    {
        if (!gen.IsSetJournal()  &&  !gen.IsSetDate()  &&
            gen.IsSetSerial_number()  &&  gen.GetSerial_number() > 0) {
            return true;
        }
    }

    return false;
}

    
void CReferenceItem::x_CreateUniqueStr(void) const
{
    if (!NStr::IsBlank(m_UniqueStr)) {  // already created
        return;
    }
    if (m_Pubdesc.Empty()) {  // not pub to generate from
        return;
    }

    ITERATE (CPubdesc::TPub::Tdata, it, m_Pubdesc->GetPub().Get()) {
        const CPub& pub = **it;
        if (pub.IsMuid()  ||  pub.IsPmid()  ||  pub.IsPat_id()  ||  pub.IsEquiv()) {
            continue;
        }
        if (!s_IsOnlySerial(pub)) {
            pub.GetLabel(&m_UniqueStr, CPub::eContent, CPub::fLabel_Unique, CPub::eLabel_V1 );
        }
    }
}


bool CReferenceItem::Matches(const CPub& pub) const
{
    switch (pub.Which()) {
    case CPub::e_Muid:
        return pub.GetMuid() == GetMUID();
    case CPub::e_Pmid:
        return pub.GetPmid() == GetPMID();
    case CPub::e_Equiv:
        ITERATE (CPub::TEquiv::Tdata, it, pub.GetEquiv().Get()) {
            if ( Matches(**it) ) {
                return true;
            }
        }
        break;
    default:
        // compare based on unique string
        {{
            // you can only compare on unique string if the reference
            // does not have a pmid or muid (example accession: L40362.1)
            if( GetMUID() == 0 && GetPMID() == 0 ) {
                x_CreateUniqueStr();
                const string& uniquestr = m_UniqueStr;

                string pub_unique;
                pub.GetLabel(&pub_unique, CPub::eContent, CPub::fLabel_Unique, CPub::eLabel_V1 );

                size_t len = pub_unique.length();
                if (len > 0  &&  pub_unique[len - 1] == '>') {
                    --len;
                }
                len = min(len , uniquestr.length());
                pub_unique.resize(len);
                if (!NStr::IsBlank(uniquestr)  &&  !NStr::IsBlank(pub_unique)) {
                    if (NStr::StartsWith(uniquestr, pub_unique, NStr::eNocase)) {
                        return true;
                    }
                }
            }
        }}
        break;
    }

    return false;
}



void CReferenceItem::x_GatherInfo(CBioseqContext& ctx)
{
    _ASSERT(m_Pubdesc.NotEmpty());

    if (!m_Pubdesc->IsSetPub()) {
        NCBI_THROW(CFlatException, eInvalidParam, "Pub not set on Pubdesc");
    }

    const CPubdesc::TPub& pub = m_Pubdesc->GetPub();

    /*if (ctx.GetSubmitBlock() != NULL) {
        m_Title = "Direct Submission";
        m_Sub.Reset(&ctx.GetSubmitBlock()->GetCit());
        m_PubType = ePub_sub;
    }*/

    //CPub_equiv::Tdata::const_iterator last = m_Pubdesc->GetPub().Get().end()--;
    ITERATE (CPub_equiv::Tdata, it, pub.Get()) {
        x_Init(**it, ctx);
    }

    // if just pmid or just muid, we look it up, assuming the user has
    // somehow given permission for remote lookups 
    const static string kGbLoader = "GBLOADER";
    if( IsJustUids() && 
        ctx.GetScope().GetObjectManager().FindDataLoader(kGbLoader) ) 
    {
        // TODO: To avoid repeated connections, in the future we should have 
        // one CMLAClient shared by the whole program
        CMLAClient mlaClient;

        vector< CRef<CPub> > new_pubs;

        ITERATE( CPub_equiv::Tdata, it, pub.Get() ) {
            const CPub & pub = **it;
            CRef<CPub> new_pub;

            try {
                switch(pub.Which()) {
                case CPub::e_Pmid:
                    {
                        const int pmid = pub.GetPmid().Get();

                        CPubMedId req(pmid);
                        CMLAClient::TReply reply;
                        new_pub = mlaClient.AskGetpubpmid(req, &reply);
                    }
                    break;
                case CPub::e_Muid:
                    {
                        const int muid = pub.GetMuid();

                        const int pmid = mlaClient.AskUidtopmid(muid);
                        if( pmid > 0 ) {
                            CPubMedId req(pmid);
                            CMLAClient::TReply reply;
                            new_pub = mlaClient.AskGetpubpmid(req, &reply);
                        }
                    }
                    break;
                default:
                    // ignore if type unknown
                    break;
                }
            } catch(...) {
                // don't worry if we can't look it up
            }

            if( new_pub ) {
                // authors come back in a weird format that we need
                // to convert to ISO
                ChangeMedlineAuthorsToISO(new_pub);

                new_pubs.push_back(new_pub);
            }
        }

        if( ! new_pubs.empty() ) {
            ITERATE( vector< CRef<CPub> >, new_pub_iter, new_pubs ) {
                x_Init( **new_pub_iter, ctx );
            }

            // we have to add the new_pubs to m_Pubdesc->GetPub() but m_Pubdesc
            // is const.  The solution is to copy it, modify the copy, and 
            // set the copy to have CConstRef
            CRef<CPubdesc> new_pubdesc( new CPubdesc );
            new_pubdesc->Assign(*m_Pubdesc);
            CPub_equiv::Tdata & new_pub_list = new_pubdesc->SetPub().Set();
            copy( new_pubs.begin(), new_pubs.end(),
                back_inserter(new_pub_list) );
            m_Pubdesc = new_pubdesc;
        }
    }

    // gather Genbank specific fields (formats: Genbank, GBSeq, DDBJ)
    if ( ctx.IsGenbankFormat() ) {
        x_GatherRemark(ctx);
    }

    x_CleanData();
}
 

void CReferenceItem::x_Init(const CPub& pub, CBioseqContext& ctx)
{
    switch (pub.Which()) {
    case CPub::e_Gen:
        x_Init(pub.GetGen(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Sub:
        x_Init(pub.GetSub(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Medline:
        x_Init(pub.GetMedline(), ctx);
        break;

    case CPub::e_Muid:
        if (m_MUID == 0) {
            m_MUID = pub.GetMuid();
            m_Category = ePublished;
        }
        break;

    case CPub::e_Article:
        x_Init(pub.GetArticle(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Journal:
        x_Init(pub.GetJournal(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Book:
        m_PubType = ePub_book;
        x_Init(pub.GetBook(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Proc:
        m_PubType = ePub_book;
        x_InitProc(pub.GetProc().GetBook(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Patent:
        x_Init(pub.GetPatent(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Man:
        x_Init(pub.GetMan(), ctx);
        m_JustUids = false;
        break;

    case CPub::e_Equiv:
        ITERATE (CPub_equiv::Tdata, it, pub.GetEquiv().Get()) {
            x_Init(**it, ctx);
        }
        break;

    case CPub::e_Pmid:
        if (m_PMID == 0) {
            m_PMID = pub.GetPmid();
            m_Category = ePublished;
        }
        break;

    default:
        break;
    }
}


void CReferenceItem::x_Init(const CCit_gen& gen, CBioseqContext& ctx)
{
    if (m_PubType == ePub_not_set) {
        m_PubType = ePub_gen;
    }

    const string& cit = gen.IsSetCit() ? gen.GetCit() : kEmptyStr;

    if (NStr::StartsWith(cit, "BackBone id_pub", NStr::eNocase)) {
        return;
    }

    m_Gen.Reset(&gen);

    // category
    if( m_Category == eUnknown ) {
        m_Category = eUnpublished;
    }

    // serial
    if (gen.IsSetSerial_number()  &&  gen.GetSerial_number() > 0  &&
        m_Serial == kMax_Int) {
        m_Serial = gen.GetSerial_number();
    }

    // Date
    if (gen.CanGetDate()  &&  !m_Date) {
        m_Date.Reset(&gen.GetDate());
    }

    if (!NStr::IsBlank(cit)) {
        if (!NStr::StartsWith(cit, "unpublished")      &&
            !NStr::StartsWith(cit, "submitted")        &&
            !NStr::StartsWith(cit, "to be published")  &&
            !NStr::StartsWith(cit, "in press")         &&
            NStr::Find(cit, "Journal") == NPOS         &&
            gen.IsSetSerial_number()  &&  gen.GetSerial_number() == 0) {
            x_SetSkip();
            return;
        } 
    } else if ((!gen.IsSetJournal()  ||  !m_Date)  &&  m_Serial == 0) {
        x_SetSkip();
        return;
    }

    // title
    if (NStr::IsBlank(m_Title)) {
        if (gen.CanGetTitle()  &&  !NStr::IsBlank(gen.GetTitle())) {
            m_Title = gen.GetTitle();
        } else if (!NStr::IsBlank(cit)) {
            SIZE_TYPE pos = NStr::Find(cit, "Title=\"");
            if (pos != NPOS) {
                pos += 7;
                SIZE_TYPE end = cit.find_first_of('"', pos);
                m_Title = cit.substr(pos, end - pos);
            }
        }
    }

    // Electronic publication
    if (!NStr::IsBlank(m_Title)  &&  NStr::StartsWith(m_Title, "(er)")) {
        m_Elect = true;
    }
    
    // Authors
    if (gen.CanGetAuthors()) {
        x_AddAuthors(gen.GetAuthors());
    }

    // MUID
    if (gen.CanGetMuid()  &&  m_MUID == 0) {
        m_MUID = gen.GetMuid();
    }
    
    // PMID
    if (gen.CanGetPmid()  &&  m_PMID == 0) {
        m_PMID = gen.GetPmid();
    }
}


void CReferenceItem::x_Init(const CCit_sub& sub, CBioseqContext& ctx)
{
    m_PubType = ePub_sub;
    m_Sub.Reset(&sub);

    // Title
    m_Title = "Direct Submission";

    // Authors
    if (sub.IsSetAuthors()) {
        x_AddAuthors(sub.GetAuthors());
    }

    // Date
    if (sub.CanGetDate()) {
        m_Date.Reset(&sub.GetDate());
    } 
    if (sub.CanGetImp()) {
        x_AddImprint(sub.GetImp(), ctx);
    }

    m_Category = eSubmission;
}


void CReferenceItem::x_Init(const CMedline_entry& mle, CBioseqContext& ctx)
{
    m_Category = ePublished;

    if (mle.CanGetUid()  &&  m_MUID == 0) {
        m_MUID = mle.GetUid();
    }

    if (mle.CanGetPmid()  &&  m_PMID == 0) {
        m_PMID = mle.GetPmid();
    }

    if (mle.CanGetCit()) {
        x_Init(mle.GetCit(), ctx);
    }
}


void CReferenceItem::x_Init(const CCit_art& art, CBioseqContext& ctx)
{
    // Title
    if (art.CanGetTitle()) {
        m_Title = art.GetTitle().GetTitle();
    }

    // Authors
    if ( art.CanGetAuthors() ) {
        x_AddAuthors(art.GetAuthors());
    }

    switch (art.GetFrom().Which()) {
    case CCit_art::C_From::e_Journal:
        m_PubType = ePub_jour;
        x_Init(art.GetFrom().GetJournal(), ctx);
        break;
    case CCit_art::C_From::e_Proc:
        m_PubType = ePub_book_art;
        x_Init(art.GetFrom().GetProc(), ctx);
        break;
    case CCit_art::C_From::e_Book:
        m_PubType = ePub_book_art;
        x_Init(art.GetFrom().GetBook(), ctx);
        break;
    default:
        break;
    }

    if (art.CanGetIds()) {
        ITERATE (CArticleIdSet::Tdata, it, art.GetIds().Get()) {
            switch ((*it)->Which()) {
            case CArticleId::e_Pubmed:
                if (m_PMID == 0) {
                    m_PMID = (*it)->GetPubmed();
                }
                break;
            case CArticleId::e_Medline:
                if (m_MUID == 0) {
                    m_MUID = (*it)->GetMedline();
                }
                break;
            default:
                break;
            }
        }
    }
}

void CReferenceItem::x_Init(const CCit_proc& proc, CBioseqContext& ctx)
{
    if (proc.IsSetBook()) {
        x_Init(proc.GetBook(), ctx);
    } else if (proc.IsSetMeet()) {
        // !!!
    }
}


void CReferenceItem::x_Init(const CCit_jour& jour, CBioseqContext& ctx)
{
    if( m_Journal.IsNull() ) {
        m_Journal.Reset(&jour);
    }

    if (jour.IsSetImp()) {
        x_AddImprint(jour.GetImp(), ctx);
    }

    if (jour.IsSetTitle()) {
        ITERATE (CCit_jour::TTitle::Tdata, it, jour.GetTitle().Get()) {
            if ((*it)->IsName()  &&  NStr::StartsWith((*it)->GetName(), "(er)")) {
                m_Elect = true;
                break;
            }
        }
    }
}


void CReferenceItem::x_InitProc(const CCit_book& proc, CBioseqContext& ctx)
{
    m_Book.Reset();
    if (!m_Authors  &&  proc.IsSetAuthors()) {
        x_AddAuthors(proc.GetAuthors());
    } 
    if (proc.IsSetTitle()) {
        m_Title = proc.GetTitle().GetTitle();
    }
    if (proc.CanGetImp()) {
        x_AddImprint(proc.GetImp(), ctx);
    }
}


void CReferenceItem::x_Init
(const CCit_book& book,
 CBioseqContext& ctx)
{
    m_Book.Reset(&book);
    if (!m_Authors  &&  book.IsSetAuthors()) {
        x_AddAuthors(book.GetAuthors());
    } 
    if (book.CanGetImp()) {
        x_AddImprint(book.GetImp(), ctx);
    }

}


void CReferenceItem::x_Init(const CCit_pat& pat, CBioseqContext& ctx)
{
    //bool embl = ctx.Config().IsFormatEMBL();
    m_Patent.Reset(&pat);
    m_PubType = ePub_pat;
    m_Category = ePublished;

    if (pat.IsSetTitle()) {
        m_Title = pat.GetTitle();
    }
    if (pat.IsSetAuthors()) {
        x_AddAuthors(pat.GetAuthors());
    }
    if (pat.IsSetDate_issue()) {
        m_Date.Reset(&pat.GetDate_issue());
    } else if (pat.IsSetApp_date()) {
        m_Date.Reset(&pat.GetApp_date());
    }

    m_PatentId = ctx.GetPatentSeqId();
}


void CReferenceItem::x_Init(const CCit_let& man, CBioseqContext& ctx)
{
    if (!man.IsSetType()  ||  man.GetType() != CCit_let::eType_thesis) {
        return;
    }

    m_PubType = ePub_thesis;
  
    if (man.IsSetCit()) {
        const CCit_book& book = man.GetCit();
        x_Init(book, ctx);
        if (book.IsSetTitle()) {
            m_Title = book.GetTitle().GetTitle();
        }
    }
}


void CReferenceItem::x_AddAuthors(const CAuth_list& auth_list)
{
    m_Authors.Reset(&auth_list);

    if (!auth_list.CanGetNames()) {
        return;
    }
    
    // also populate the consortium (if available).
    // note: there may be more than one, and they all want to be listed.
    if (!NStr::IsBlank(m_Consortium)) {
        return;
    }

    const CAuth_list::TNames& names = auth_list.GetNames();
    
    if (names.IsStd()) {
        ITERATE (CAuth_list::TNames::TStd, it, names.GetStd()) {
            const CAuthor& auth = **it;
            if (auth.CanGetName()  &&  auth.GetName().IsConsortium()) {
                if (NStr::IsBlank(m_Consortium)) {
                    m_Consortium = auth.GetName().GetConsortium();
                }
                else {
                    m_Consortium += "; " + auth.GetName().GetConsortium();
                }
            }
        }
    }
}


void CReferenceItem::x_AddImprint(const CImprint& imp, CBioseqContext& ctx)
{
    // electronic journal
    if (imp.IsSetPubstatus()) {
        CImprint::TPubstatus pubstatus = imp.GetPubstatus();
        m_Elect = (pubstatus == 3  || pubstatus == 10);
    }

    // date
    if (!m_Date  &&  imp.IsSetDate()) {
        m_Date.Reset(&imp.GetDate());
    }

    // prepub
    if (imp.IsSetPrepub()) {
        CImprint::TPrepub prepub = imp.GetPrepub();
        //m_Prepub = imp.GetPrepub();
        m_Category = 
            prepub != CImprint::ePrepub_in_press ? eUnpublished : ePublished;
    } else {
        m_Category = ePublished;
    }
}


void CReferenceItem::GetAuthNames(const CAuth_list& alp, TStrList& authors)
{   
    authors.clear();

    const CAuth_list::TNames& names = alp.GetNames();
    string name;
    switch (names.Which()) {
    case CAuth_list::TNames::e_Std:
        ITERATE (CAuth_list::TNames::TStd, it, names.GetStd()) {
            if (!(*it)->CanGetName()) {
                continue;
            }
            const CPerson_id& pid = (*it)->GetName();
            if (pid.IsName()  ||  pid.IsMl()  ||  pid.IsStr()) {
                name.erase();
                pid.GetLabel(&name, CPerson_id::eGenbank);
                authors.push_back(name);
            }
        }
        break;
        
    case CAuth_list::TNames::e_Ml:
        authors.insert(authors.end(), names.GetMl().begin(), names.GetMl().end());
        break;
        
    case CAuth_list::TNames::e_Str:
        authors.insert(authors.end(), names.GetStr().begin(), names.GetStr().end());
        break;
        
    default:
        break;
    }
}


void CReferenceItem::FormatAuthors(const CAuth_list& alp, string& auth)
{
    TStrList authors;

    CReferenceItem::GetAuthNames(alp, authors);
    if (authors.empty()) {
        return;
    }

    CNcbiOstrstream auth_line;
    TStrList::const_iterator last = --(authors.end());

    string separator = kEmptyStr;
    //bool first = true;
    ITERATE (TStrList, it, authors) {
        auth_line << separator << *it;
        ++it;
        // might want to remove "et al" detection once we've moved over to the C++ toolkit.
        // It's here to make the diffs match.
        if( ( it == last ) &&
            ( NStr::StartsWith(*it, "et al", NStr::eNocase) || NStr::StartsWith(*it, "et,al", NStr::eNocase) ) ) {
                separator = " ";
        } else if (it == last) {
            separator = " and ";
        } else {
            separator = ", ";
        }
        --it;
    }

    auth = CNcbiOstrstreamToString(auth_line);

    if( auth.empty() ) {
        auth = ".";
    }
}


// Historical relic from C version
static void s_RemovePeriod(string& title)
{
    if (NStr::EndsWith(title, '.')) {
        size_t last = title.length() - 1;
        if (last > 5) {
           if (title[last - 1] != '.'  ||  title[last - 2] != '.') {
               title.erase(last);
           }
        }
    }
}


void CReferenceItem::x_CleanData(void)
{
    // title
    NStr::TruncateSpacesInPlace(m_Title);
    StripSpaces(m_Title);   // internal spaces
    // In the future, expand tildes before stripping spaces.'
    // We leave this ordering for now for compatibility with C toolkit asn2gb
    ExpandTildes(m_Title, eTilde_space);
    ConvertQuotes(m_Title);
    s_RemovePeriod(m_Title);
    x_CapitalizeTitleIfNecessary();
    // remark
    ConvertQuotesNotInHTMLTags(m_Remark);
    ExpandTildes(m_Remark, eTilde_newline);
}

// Unfortunately, some compilers won't let us pass
// islower straight to STL algorithms
class CIsLowercase {
public:
    bool operator()( const char ch ) {
        return 0 != islower(ch);
    }
};

static
bool s_ContainsNoLowercase( const string &str ) 
{
    return ( find_if( str.begin(), str.end(), CIsLowercase() ) == str.end() );
}

void CReferenceItem::x_CapitalizeTitleIfNecessary()
{
    if( ! GetPubdesc().CanGetPub() ) {
        return;
    }

    if( ! GetPubdesc().GetPub().CanGet() ) {
        return;
    }

    ITERATE ( CPubdesc::TPub::Tdata, it, GetPubdesc().GetPub().Get() ) {
        const CPub& pub = **it;

        switch( pub.Which() ) {
            case CPub::e_Proc:
            case CPub::e_Man:
                if(  m_Title.length() > 3 ) {
                    // capitalize the title before checking for all caps
                    m_Title[0] = toupper( m_Title[0] );
                    if( s_ContainsNoLowercase(m_Title) ) {
                        NStr::ToLower( m_Title );
                        // we undid the earlier uppercasing step.  Slightly slower, but
                        // the code is a little cleaner than using a loop
                        m_Title[0] = toupper( m_Title[0] );
                    }
                    return;
                }
                break;
            default:
                // do nothing
                break;
        }
    }
}

void CReferenceItem::ChangeMedlineAuthorsToISO( CRef<CPub> pub )
{
    // leave early if it doesn't need to be changed
    if( ! pub || ! pub->IsArticle() || ! pub->IsSetAuthors() || ! pub->GetAuthors().IsSetNames() ||
        ! pub->GetAuthors().GetNames().IsMl() )
    {
        return;
    }

    // build our new authors list in here
    CAuth_list::C_Names::TStd new_authors;

    const CAuth_list::C_Names::TMl & ml_names = pub->GetAuthors().GetNames().GetMl();
    ITERATE( CAuth_list::C_Names::TMl, ml_name_iter, ml_names ) {
        string author_name = *ml_name_iter;

        // we will fill in these 3 as we go along
        string lastname;
        string initials;
        string suffix;

        // this scope fills in lastname, initials, and suffix
        {
            NStr::TruncateSpacesInPlace(author_name);
            vector<string> tokens;
            NStr::Tokenize(author_name, " ", tokens, NStr::eMergeDelims);

            // get suffix if it exists, and remove it from the list
            if( tokens.size() >= 3 && 
                ! x_StringIsJustCapitalLetters(tokens.back()) && 
                x_StringIsJustCapitalLetters(tokens[tokens.size() - 2]) ) 
            {
                suffix = tokens.back();
                tokens.pop_back();
            }

            // get initials if they exist, and remove them from the list
            if( tokens.size() >= 2 &&
                x_StringIsJustCapitalLetters(tokens.back()) )
            {
                initials = tokens.back();
                tokens.pop_back();
            }

            // remaining pieces belong to the last name
            lastname = NStr::Join(tokens, " ");
        }

        // put period in initials. e.g. "MJ" -> "M.J."
        {
            string new_initials;
            ITERATE( string, ch_iter, initials ) {
                new_initials += *ch_iter;
                new_initials += '.';
            }
            // swap is faster than assignment
            initials.swap(new_initials);
        }

        // a couple of static transformations for the suffix
        typedef SStaticPair<const char*, const char*> TSufElem;
        static const TSufElem sc_suf_map[] = {
            { "1d",  "I" },
            { "1st", "I" },
            { "2d",  "II" },
            { "2nd", "II" },
            { "3d",  "III" },
            { "3rd", "III" },
            { "4th", "IV" },
            { "5th", "V" },
            { "6th", "VI" },
            { "Jr",  "Jr." },
            { "Sr",  "Sr."}
        };
        typedef CStaticArrayMap<const char *, const char *, PCase_CStr> TSufMap;
        DEFINE_STATIC_ARRAY_MAP(TSufMap, sc_SufMap, sc_suf_map);

        TSufMap::const_iterator suf_find_iter = sc_SufMap.find(suffix.c_str());
        if( suf_find_iter != sc_SufMap.end() ) {
            suffix = suf_find_iter->second;
        }

        CRef<CAuthor> new_author( new CAuthor );
        CPerson_id_Base::TName & name = new_author->SetName().SetName();
        name.SetLast( lastname );
        if( ! initials.empty() ) {
            name.SetInitials( initials );
        }
        if( ! suffix.empty() ) {
            name.SetSuffix( suffix );
        }

        new_authors.push_back( new_author );
    }
    
    copy( new_authors.begin(), 
        new_authors.end(),
        back_inserter( pub->SetArticle().SetAuthors().SetNames().SetStd() ) );
}

bool CReferenceItem::x_StringIsJustCapitalLetters( const string & str )
{
    if( str.empty() ) {
        return false;
    }

    ITERATE(string, ch_iter, str ) {
        if( ! isupper(*ch_iter) ) {
            return false;
        }
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
//
// Genbank Format Specific

// these must be in "ASCIIbetical" order; beware of the fact that
// closing quotes sort after spaces.
static const char* const sc_RemarkText[] = {
  "full automatic",
  "full staff_entry",
  "full staff_review",
  "simple automatic",
  "simple staff_entry",
  "simple staff_review",
  "unannotated automatic",
  "unannotated staff_entry",
  "unannotated staff_review"
};
typedef CStaticArraySet<const char*, PCase_CStr> TStaticRemarkSet;
DEFINE_STATIC_ARRAY_MAP(TStaticRemarkSet, sc_Remarks, sc_RemarkText);


void CReferenceItem::x_GatherRemark(CBioseqContext& ctx)
{
    static const char* const kDoiLink = "http://dx.doi.org/";

    list<string> l;

    // comment

    if ( m_Pubdesc->IsSetComment()  &&  !m_Pubdesc->GetComment().empty() ) {
        const string& comment = m_Pubdesc->GetComment();
        
        if ( sc_Remarks.find(comment.c_str()) == sc_Remarks.end() ) {
            l.push_back(comment);
        }
    }

    // for GBSeq format collect remarks only from comments.
    if ( ctx.Config().IsFormatGBSeq() ) {
        if ( !l.empty() ) {
            m_Remark = l.front();
        }
        return;
    }

    // GIBBSQ
    CSeq_id::TGibbsq gibbsq = 0;
    ITERATE (CBioseq::TId, it, ctx.GetBioseqIds()) {
        if ( (*it)->IsGibbsq() ) {
            gibbsq = (*it)->GetGibbsq();
        }
    }
    if ( gibbsq > 0 ) {
        static const string str1 = 
            "GenBank staff at the National Library of Medicine created this entry [NCBI gibbsq ";
        static const string str2 = "] from the original journal article.";
        l.push_back(str1 + NStr::IntToString(gibbsq) + str2);

        // Figure
        if ( m_Pubdesc->IsSetFig()  &&  !m_Pubdesc->GetFig().empty()) {
            l.push_back("This sequence comes from " + m_Pubdesc->GetFig());
            if (!NStr::EndsWith(l.back(), '.')) {
                AddPeriod(l.back());
            }
            NStr::ReplaceInPlace( l.back(), "\"", "\'" );
        }

        // Poly_a
        if ( m_Pubdesc->IsSetPoly_a()  &&  m_Pubdesc->GetPoly_a() ) {
            l.push_back("Polyadenylate residues occurring in the figure were omitted from the sequence.");
        }

        // Maploc
        if ( m_Pubdesc->IsSetMaploc()  &&  !m_Pubdesc->GetMaploc().empty()) {
            l.push_back("Map location: " + m_Pubdesc->GetMaploc());
            if (!NStr::EndsWith(l.back(), '.')) {
                AddPeriod(l.back());
            }
        }
    }
    
    if ( m_Pubdesc->CanGetPub() ) {
        ITERATE (CPubdesc::TPub::Tdata, it, m_Pubdesc->GetPub().Get()) {
            if ( (*it)->IsArticle() ) {
                if ( (*it)->GetArticle().GetFrom().IsJournal() ) {
                    const CCit_jour& jour = 
                        (*it)->GetArticle().GetFrom().GetJournal();
                    if ( jour.IsSetImp() ) {
                        const CCit_jour::TImp& imp = jour.GetImp();
                        if ( imp.IsSetRetract() ) {
                            const CCitRetract& ret = imp.GetRetract();
                            switch (ret.GetType()) {
                            case CCitRetract::eType_in_error:
                                if (ret.IsSetExp()  &&
                                    !ret.GetExp().empty() ) {
                                    l.push_back("Erratum:[" + ret.GetExp() + "]");
                                } else {
                                    l.push_back("Erratum");
                                }
                                break;

                            case CCitRetract::eType_retracted:
                                if (ret.IsSetExp()  &&
                                    !ret.GetExp().empty() ) {
                                    l.push_back("Retracted:[" + ret.GetExp() + "]");
                                } else {
                                    l.push_back("Retracted");
                                }
                                break;

                            case CCitRetract::eType_erratum:
                                if (ret.IsSetExp()  &&
                                    !ret.GetExp().empty() ) {
                                    l.push_back("Correction to:[" + ret.GetExp() + "]");
                                } else {
                                    l.push_back("Correction");
                                }
                                break;

                            default:
                                break;
                            }
                        }
                        if ( imp.CanGetPubstatus() ) {
                            CImprint::TPubstatus pubstatus = imp.GetPubstatus();
                            switch ( pubstatus ) {
                            case 3:
                                l.push_back( "Publication Status: Online-Only" );
                                break;
                            case 10:
                                l.push_back( "Publication Status: Available-Online prior to print" );
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }

                if( (*it)->GetArticle().CanGetIds() ) {
                    const CCit_art_Base::TIds & ids = (*it)->GetArticle().GetIds();
                    if( ids.CanGet() ) {

                        // no DOIs pritned if there's a pmid or muid
                        bool hasPmidOrMuid = false;
                        ITERATE( CArticleIdSet_Base::Tdata, it, ids.Get() ) {
                            if( (*it)->IsPubmed() && (*it)->GetPubmed().Get() != 0 ) {
                                hasPmidOrMuid = true;
                                break;
                            } else if(  (*it)->IsMedline() && (*it)->GetMedline().Get() != 0 ) {
                                hasPmidOrMuid = true;
                                break;
                            }
                        }

                        if( ! hasPmidOrMuid ) {
                            ITERATE( CArticleIdSet_Base::Tdata, it, ids.Get() ) {
                                if( (*it)->Which() == CArticleId_Base::e_Doi) {
                                    const string & doi = (*it)->GetDoi().Get();
                                    if( NStr::StartsWith( doi, "10." ) ) {
                                        if( ctx.Config().DoHTML() && ! CommentHasSuspiciousHtml(doi) ) {
                                            CNcbiOstrstream result;
                                            result << "DOI: <a href=\""
                                                   << kDoiLink << doi << "\">"
                                                   << doi << "</a>";
                                            l.push_back( CNcbiOstrstreamToString(result) );
                                        } else {
                                            l.push_back( "DOI: " + doi );
                                        }
                                        break;
                                    }
                                }
                            }
                        }

                    }
                }
            } else if ( (*it)->IsSub() ) {
                const CCit_sub& sub = (*it)->GetSub();
                if ( sub.IsSetDescr()  &&  !sub.GetDescr().empty() ) {
                    l.push_back(sub.GetDescr());
                }
            }
        }
    }

    if (!l.empty()) {
        m_Remark = NStr::Join(l, "\n");
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// Reference Sorting

// Used for sorting references
static CDate::ECompare s_CompareDates(const CDate& d1, const CDate& d2)
{
    if (d1.IsStr()  &&  d2.IsStr()) {
        int diff = NStr::CompareNocase(d1.GetStr(), d2.GetStr());
        if (diff == 0) {
            return CDate::eCompare_same;
        } else {
            return (diff < 0) ? CDate::eCompare_before : CDate::eCompare_after;
        }
    }

    // arbitrary ordering (std before str)
    if (d1.Which() != d2.Which()) {
        return d1.IsStd() ? CDate::eCompare_before : CDate::eCompare_after;
    }

    _ASSERT(d1.IsStd()  &&  d2.IsStd());

    const CDate::TStd& std1 = d1.GetStd();
    const CDate::TStd& std2 = d2.GetStd();

    if( std1.IsSetYear() || std2.IsSetYear() ) {
        CDate_std::TYear y1 = std1.IsSetYear() ? std1.GetYear() : 0;
        CDate_std::TYear y2 = std2.IsSetYear() ? std2.GetYear() : 0;
        if (y1 < y2) {
            return CDate::eCompare_before;
        } else if (y1 > y2) {
            return CDate::eCompare_after;
        }
    }
    if (std1.IsSetMonth()  ||  std2.IsSetMonth()) {
        CDate_std::TMonth m1 = std1.IsSetMonth() ? std1.GetMonth() : 0;
        CDate_std::TMonth m2 = std2.IsSetMonth() ? std2.GetMonth() : 0;
        if (m1 < m2) {
            return CDate::eCompare_before;
        } else if (m1 > m2) {
            return CDate::eCompare_after;
        }
    }
    if (std1.IsSetDay()  ||  std2.IsSetDay()) {
        CDate_std::TDay day1 = std1.IsSetDay() ? std1.GetDay() : 0;
        CDate_std::TDay day2 = std2.IsSetDay() ? std2.GetDay() : 0;
        if (day1 < day2) {
            return CDate::eCompare_before;
        } else if (day1 > day2) {
            return CDate::eCompare_after;
        }
    }
    if (std1.IsSetSeason()  &&  !std2.IsSetSeason()) {
        return CDate::eCompare_after;
    } else if (!std1.IsSetSeason()  &&  std2.IsSetSeason()) {
        return CDate::eCompare_before;
    } else if (std1.IsSetSeason()  && std2.IsSetSeason()) {
        int diff = NStr::CompareNocase(std1.GetSeason(), std2.GetSeason());
        if (diff == 0) {
            return CDate::eCompare_same;
        } else {
            return (diff < 0) ? CDate::eCompare_before : CDate::eCompare_after;
        }
    }

    return CDate::eCompare_same;
}

LessThan::LessThan(ESerialFirst serial_first, bool is_refseq) :
    m_SerialFirst(serial_first), m_IsRefSeq(is_refseq)
{}


bool LessThan::operator()
(const CRef<CReferenceItem>& ref1,
 const CRef<CReferenceItem>& ref2)
{
    if ( m_SerialFirst == eSerialFirst_Yes &&  ref1->GetSerial() != ref2->GetSerial() ) {
        return ref1->GetSerial() < ref2->GetSerial();
    }

    // sort by category (published / unpublished / submission)
    if ( ref1->GetCategory() != ref2->GetCategory() ) {
        return ref1->GetCategory() < ref2->GetCategory();
    }

    // sort by date:
    // - publications with date come before those without one.
    // - more specific dates come before less specific ones.
    // - older publication comes first (except RefSeq).
    if (ref1->IsSetDate()  &&  !ref2->IsSetDate()) {
        return m_IsRefSeq;
    } else if (!ref1->IsSetDate()  &&  ref2->IsSetDate()) {
        return ! m_IsRefSeq;
    }
    
    if (ref1->IsSetDate()  &&  ref2->IsSetDate()) {
        CDate::ECompare status = s_CompareDates(ref1->GetDate(), ref2->GetDate());
        _ASSERT( status != CDate::eCompare_unknown ); // unknown would produce invalid ordering
        if (status != CDate::eCompare_same ) {
            return m_IsRefSeq ? (status == CDate::eCompare_after) :
                                (status == CDate::eCompare_before);
        }
    }
    //}
    // after: dates are the same, or both missing.
    
    // distinguish by uids (swap order for RefSeq)
    if ( ref1->GetPMID() != 0  &&  ref2->GetPMID() != 0  &&
         !(ref1->GetPMID() == ref2->GetPMID()) ) {
        return m_IsRefSeq ? (ref1->GetPMID() > ref2->GetPMID()) :
            (ref1->GetPMID() < ref2->GetPMID());
    }
    if ( ref1->GetMUID() != 0  &&  ref2->GetMUID() != 0  &&
         !(ref1->GetMUID() == ref2->GetMUID()) ) {
        return m_IsRefSeq ? (ref1->GetMUID() > ref2->GetMUID()) :
            (ref1->GetMUID() < ref2->GetMUID());
    }

    // just uids goes last
    if ( (ref1->GetPMID() != 0  &&  ref2->GetPMID() != 0)  ||
         (ref1->GetMUID() != 0  &&  ref2->GetMUID() != 0) ) {
        if ( ref1->IsJustUids()  &&  !ref2->IsJustUids() ) {
            return true;
        } else if ( !ref1->IsJustUids()  &&  ref2->IsJustUids() ) {
            return false;
        }
    }

    // put sites after pubs that refer to all or a range of bases
    if (ref1->GetReftype() != ref2->GetReftype()) {
        return ref1->GetReftype() < ref2->GetReftype();
    }

    // next use AUTHOR string
    string auth1, auth2;
    if (ref1->IsSetAuthors()) {
        CReferenceItem::FormatAuthors(ref1->GetAuthors(), auth1);
        if( ! ref1->GetConsortium().empty() ) {
            if( ! auth1.empty() ) {
                auth1 += "; ";
            }
            auth1 += ref1->GetConsortium();
        }
    }
    if (ref2->IsSetAuthors()) {
        CReferenceItem::FormatAuthors(ref2->GetAuthors(), auth2);
        if( ! ref2->GetConsortium().empty() ) {
            if( ! auth2.empty() ) {
                auth2 += "; ";
            }
            auth2 += ref2->GetConsortium();
        }
    }
    int comp = NStr::CompareNocase(auth1, auth2);
    if ( comp != 0 ) {
        return comp < 0;
    }

    // use unique label string to determine sort order
    const string& uniquestr1 = ref1->GetUniqueStr();
    const string& uniquestr2 = ref2->GetUniqueStr();
    if (!NStr::IsBlank(uniquestr1)  &&  !NStr::IsBlank(uniquestr2)) {
        comp = NStr::CompareNocase(uniquestr1, uniquestr2);
        if ( comp != 0 ) {
            return comp < 0;
        }
    }

    // put pub descriptors before features, sort features by location
    const CSeq_feat* f1 = dynamic_cast<const CSeq_feat*>(ref1->GetObject());
    const CSeq_feat* f2 = dynamic_cast<const CSeq_feat*>(ref2->GetObject());
    if (f1 == NULL  &&  f2 != NULL) {
        return true;
    } else if (f1 != NULL  &&  f2 == NULL) {
        return false;
    } else if (f1 != NULL  &&  f2 != NULL) {
        CSeq_loc::TRange r1 = f1->GetLocation().GetTotalRange();
        CSeq_loc::TRange r2 = f2->GetLocation().GetTotalRange();
        if (r1 < r2) {
            return true;
        } else if (r2 < r1) {
            return false;
        }
    }

    if ( !m_SerialFirst ) {
        return ref1->GetSerial() < ref2->GetSerial();
    }

    return false;
}


END_SCOPE(objects)
END_NCBI_SCOPE
