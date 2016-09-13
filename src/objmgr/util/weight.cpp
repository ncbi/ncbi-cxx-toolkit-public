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
* Author:  Aaron Ucko
*
* File Description:
*   Weights for protein sequences
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objects/seqloc/Seq_loc.hpp>

#include <objmgr/util/weight.hpp>
#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// By NCBIstdaa:
// A B C D E F G H  I  K  L M N P Q  R S T V  W X Y Z U *  O  J
static const int kNumC[] =
{0,3,4,3,4,5,9,2,6, 6, 6, 6,5,4,5,5, 6,3,4,5,11,0,9,5,3,0,12, 6};
static const int kNumH[] =
{0,5,5,5,5,7,9,3,7,11,12,11,9,6,7,8,12,5,7,9,10,0,9,7,5,0,19,11};
static const int kNumN[] =
{0,1,1,1,1,1,1,1,3, 1, 2, 1,1,2,1,2, 4,1,1,1, 2,0,1,1,1,0, 3, 1};
static const int kNumO[] =
{0,1,3,1,3,3,1,1,1, 1, 1, 1,1,2,1,2, 1,2,2,1, 1,0,2,3,1,0, 2, 1};
static const int kNumS[] =
{0,0,0,1,0,0,0,0,0, 0, 0, 0,1,0,0,0, 0,0,0,0, 0,0,0,0,0,0, 0, 0};
static const int kNumSe[] =
{0,0,0,0,0,0,0,0,0, 0, 0, 0,0,0,0,0, 0,0,0,0, 0,0,0,0,1,0, 0, 0};
static const size_t kMaxRes = sizeof(kNumC) / sizeof(*kNumC) - 1;


template <class Iterator>
double s_GetProteinWeight(Iterator start, Iterator end)
{
    // Start with water (H2O)
    size_t c = 0, h = 2, n = 0, o = 1, s = 0, se = 0;

    Iterator p(start);
    for ( ;  start != end;  ++start) {
        unsigned char res = *start;
        if ( res > kMaxRes  ||  !kNumC[res] ) {
            NCBI_THROW(CObjmgrUtilException, eBadResidue,
                "GetProteinWeight: bad residue");
        }
        c  += kNumC [res];
        h  += kNumH [res];
        n  += kNumN [res];
        o  += kNumO [res];
        s  += kNumS [res];
        se += kNumSe[res];
    }
    return 12.01115 * c + 1.0079 * h + 14.0067 * n + 15.9994 * o + 32.064 * s
        + 78.96 * se;
}


double GetProteinWeight(const CSeq_feat& feat, CScope& scope,
                        const CSeq_loc* location,
                        TGetProteinWeight opts )
{
    if (feat.GetData().Which() != CSeqFeatData::e_Prot) {
        NCBI_THROW(CException, eUnknown,
                   "molecular weight only valid for protein features");
    }

    const CSeq_loc& loc =
        (location ? *location : feat.GetLocation());
    CSeqVector v(loc, scope);
    v.SetCoding(CSeq_data::e_Ncbistdaa);

    CSeqVector_CI vit(v);

    /// find out if the molecule is complete
    CMolInfo::TCompleteness comp = CMolInfo::eCompleteness_unknown;
    const CProt_ref& prot = feat.GetData().GetProt();
    
    switch (prot.GetProcessed()) {
    case CProt_ref::eProcessed_not_set:
    case CProt_ref::eProcessed_preprotein:
        /// follow the molecule's setting
        break;
    case CProt_ref::eProcessed_mature:
    case CProt_ref::eProcessed_signal_peptide:
    case CProt_ref::eProcessed_transit_peptide:
        /// trust the location as-is
        comp = CMolInfo::eCompleteness_partial;
        break;
    default:
        ;
    }

    if (comp == CMolInfo::eCompleteness_unknown) {
        /// assess based on the molecule
        CBioseq_Handle bsh = scope.GetBioseqHandle(loc);
        if (loc.GetTotalRange().GetFrom() > 0  ||
            loc.GetTotalRange().GetLength() < bsh.GetBioseqLength()) {
            /// we don' want to clip
            comp = CMolInfo::eCompleteness_partial;
        } else {
            comp = CMolInfo::eCompleteness_complete;

            if (prot.GetProcessed() == CProt_ref::eProcessed_not_set) {
                /// look for a signal peptide; if there is one, consider
                /// ourselves partial
                CFeat_CI feat_it(bsh, CSeqFeatData::e_Prot);
                for ( ;  feat_it;  ++feat_it) {
                    switch (feat_it->GetData().GetProt().GetProcessed()) {
                    case CProt_ref::eProcessed_transit_peptide:
                    case CProt_ref::eProcessed_signal_peptide:
                        comp = CMolInfo::eCompleteness_partial;
                        break;

                    default:
                        break;
                    }
                }
            }

            /**
            /// NB: the C toolkit has not yet implemented this; commented out
            /// for now to maintain compatibility
            CConstRef<CMolInfo> molinfo(sequence::GetMolInfo(bsh));
            if (molinfo) {
                comp = molinfo->GetCompleteness();
                LOG_POST(Error << "comp = " << comp);
            }
            **/
        }
    }

    if( (opts & fGetProteinWeight_ForceInitialMetTrim) != 0 ) {
        if ( vit.GetBufferSize() > 1 && *vit == ('M' - 'A')) {
            ++vit;
        }
    } else {
        switch (comp) {
        case CMolInfo::eCompleteness_unknown:
        case CMolInfo::eCompleteness_partial:
        case CMolInfo::eCompleteness_no_left:
        case CMolInfo::eCompleteness_no_ends:
            /// molecule is incomplete at the start; any 'M' here should be trusted
            break;

        default:
            /// for complete molecules, we skip the leading 'M' since this is
            /// cleaved as a post-transcriptional modification
            if ( vit.GetBufferSize() > 1 && *vit == ('M' - 'A')) {
                ++vit;
            }
            break;
        }
    }

    return s_GetProteinWeight(vit, v.end());
}


double GetProteinWeight(const CBioseq_Handle& handle, const CSeq_loc* location, 
                        TGetProteinWeight opts )
{
    CSeqVector v = (location
                    ? CSeqVector(*location, handle.GetScope())
                    : handle.GetSeqVector());
    v.SetCoding(CSeq_data::e_Ncbistdaa);

    CSeqVector_CI vit(v);

    /// find out if the molecule is complete
    CMolInfo::TCompleteness comp = CMolInfo::eCompleteness_partial;
    if (location  &&
        (location->GetTotalRange().GetFrom() > 0  ||
         location->GetTotalRange().GetLength() < handle.GetBioseqLength())) {
        /// we don' want to clip
        comp = CMolInfo::eCompleteness_partial;
    } else {
        comp = CMolInfo::eCompleteness_complete;
        /**
        /// NB: the C toolkit has not yet implemented this; commented out
        /// for now to maintain compatibility
        CConstRef<CMolInfo> molinfo(sequence::GetMolInfo(handle));
        if (molinfo) {
            comp = molinfo->GetCompleteness();
            LOG_POST(Error << "comp = " << comp);
        }
        **/
    }

    if( (opts & fGetProteinWeight_ForceInitialMetTrim) != 0 ) {
        if (*vit == ('M' - 'A')) {
            ++vit;
        }
    } else {
        switch (comp) {
        case CMolInfo::eCompleteness_unknown:
        case CMolInfo::eCompleteness_partial:
        case CMolInfo::eCompleteness_no_left:
        case CMolInfo::eCompleteness_no_ends:
            /// molecule is incomplete at the start; any 'M' here should be trusted
            break;

        default:
            /// for complete molecules, we skip the leading 'M' since this is
            /// cleaved as a post-transcriptional modification
            if (*vit == ('M' - 'A')) {
                ++vit;
            }
            break;
        }
    }

    return s_GetProteinWeight(vit, v.end());
}


double GetProteinWeight(const string& iupac_aa_sequence)
{
    string ncbistdaa;
    SIZE_TYPE len =
        CSeqConvert::Convert(iupac_aa_sequence, CSeqUtil::e_Iupacaa,
                             0, TSeqPos(iupac_aa_sequence.size()),
                             ncbistdaa, CSeqUtil::e_Ncbistdaa);
    if (len < iupac_aa_sequence.size()) {
        NCBI_THROW(CException, eUnknown,
                   "failed to convert IUPACaa sequence to NCBIstdaa");
    }
    return s_GetProteinWeight(ncbistdaa.begin(),
                              ncbistdaa.end());
}


void GetProteinWeights(const CBioseq_Handle& handle, TWeights& weights)
{
    if (handle.GetBioseqMolType() != CSeq_inst::eMol_aa) {
        NCBI_THROW(CObjmgrUtilException, eBadSequenceType,
            "GetMolecularWeights requires a protein!");
    }
    weights.clear();

    set<CConstRef<CSeq_loc> > locations;
    CConstRef<CSeq_loc> signal;

    // Look for explicit markers: ideally cleavage products (mature
    // peptides), but possibly just signal peptides
    SAnnotSelector sel;
    sel.SetOverlapIntervals().SetResolveTSE()
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_mat_peptide_aa) // mature
        .IncludeFeatSubtype(CSeqFeatData::eSubtype_sig_peptide_aa) // signal
        .IncludeFeatType(CSeqFeatData::e_Region)
        .IncludeFeatType(CSeqFeatData::e_Site);
    for (CFeat_CI feat(handle, sel); feat;  ++feat) {
        bool is_mature = false, is_signal = false;
        const CSeqFeatData& data = feat->GetData();
        switch (data.Which()) {
        case CSeqFeatData::e_Prot:
            switch (data.GetProt().GetProcessed()) {
            case CProt_ref::eProcessed_mature:         is_mature = true; break;
            case CProt_ref::eProcessed_signal_peptide: is_signal = true; break;
            default: break;
            }
            break;

        case CSeqFeatData::e_Region:
            if (!NStr::CompareNocase(data.GetRegion(), "mature chain")
                ||  !NStr::CompareNocase(data.GetRegion(),
                                         "processed active peptide")) {
                is_mature = true;
            } else if (!NStr::CompareNocase(data.GetRegion(), "signal")) {
                is_signal = true;
            }
            break;

        case CSeqFeatData::e_Site:
            if (data.GetSite() == CSeqFeatData::eSite_signal_peptide) {
                is_signal = true;
            }
            break;

        default:
            break;
        }

        if (is_mature) {
            locations.insert(CConstRef<CSeq_loc>(&feat->GetLocation()));
        } else if (is_signal  &&  signal.Empty()
                   &&  !feat->GetLocation().IsWhole() ) {
            signal = &feat->GetLocation();
        }
    }

    if (locations.empty()) {
        CSeqVector v = handle.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
        CRef<CSeq_loc> whole(new CSeq_loc);
        if ( signal.NotEmpty() ) {
            // Expects to see at beginning; is this assumption safe?
            CSeq_interval& interval = whole->SetInt();
            interval.SetFrom(signal->GetTotalRange().GetTo() + 1);
            interval.SetTo(v.size() - 1);
            interval.SetId(const_cast<CSeq_id&>(*handle.GetSeqId()));
        } else if (v[0] == 'M') { // Treat initial methionine as start codon
            CSeq_interval& interval = whole->SetInt();
            interval.SetFrom(1);
            interval.SetTo(v.size() - 1);
            interval.SetId(const_cast<CSeq_id&>(*handle.GetSeqId()));
        }
        else {
            whole->SetWhole(const_cast<CSeq_id&>(*handle.GetSeqId()));
        }
        locations.insert(CConstRef<CSeq_loc>(whole));
    }

    ITERATE(set<CConstRef<CSeq_loc> >, it, locations) {
        try {
            // Split up to ensure that we call [] only if
            // GetProteinWeight succeeds.
            double weight = GetProteinWeight(handle, *it);
            weights[*it] = weight;
        } catch (CObjmgrUtilException) {
            // Silently elide
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
