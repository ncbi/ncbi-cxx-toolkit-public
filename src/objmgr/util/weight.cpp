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

#include <objmgr/object_manager.hpp>
#include <objmgr/bioseq_handle.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/seq_vector.hpp>
#include <objmgr/seq_vector_ci.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_interval.hpp>

#include <objects/seqloc/Seq_loc.hpp>

#include <objmgr/util/weight.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

// By NCBIstdaa:
//  A  B  C  D  E  F  G  H  I  K  L  M  N  P  Q  R  S  T  V  W  X  Y  Z  U  *
static const int kNumC[] =
{0, 3, 4, 3, 4, 5, 9, 2, 6, 6, 6, 6, 5, 4, 5, 5, 6, 3, 4, 5,11, 0, 9, 5, 3, 0};
static const int kNumH[] =
{0, 5, 5, 5, 5, 7, 9, 3, 7,11,12,11, 9, 6, 7, 8,12, 5, 7, 9,10, 0, 9, 7, 5, 0};
static const int kNumN[] =
{0, 1, 1, 1, 1, 1, 1, 1, 3, 1, 2, 1, 1, 2, 1, 2, 4, 1, 1, 1, 2, 0, 1, 1, 1, 0};
static const int kNumO[] =
{0, 1, 3, 1, 3, 3, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 2, 2, 1, 1, 0, 2, 3, 1, 0};
static const int kNumS[] =
{0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const int kNumSe[] =
{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0};
static const size_t kMaxRes = sizeof(kNumC) / sizeof(*kNumC) - 1;


double GetProteinWeight(const CBioseq_Handle& handle, const CSeq_loc* location)
{
    CSeqVector v = (location
                    ? handle.GetSequenceView(*location,
                                             CBioseq_Handle::eViewConstructed)
                    : handle.GetSeqVector());
    v.SetCoding(CSeq_data::e_Ncbistdaa);

    TSeqPos size = v.size();

    // Start with water (H2O)
    TSeqPos c = 0, h = 2, n = 0, o = 1, s = 0, se = 0;

    for (CSeqVector_CI vit(v);  vit.GetPos() < size;  ++vit) {
        CSeqVector::TResidue res = *vit;
        if ( res >= kMaxRes  ||  !kNumC[res] ) {
            THROW1_TRACE(CBadResidueException,
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


void GetProteinWeights(const CBioseq_Handle& handle, TWeights& weights)
{
    CBioseq_Handle::TBioseqCore core = handle.GetBioseqCore();
    if (core->GetInst().GetMol() != CSeq_inst::eMol_aa) {
        NCBI_THROW(CObjmgrUtilException, eBadSequenceType,
            "GetMolecularWeights requires a protein!");
    }
    weights.clear();

    set<CConstRef<CSeq_loc> > locations;
    CSeq_loc* whole = new CSeq_loc;
    whole->SetWhole().Assign(*handle.GetSeqId());

    CConstRef<CSeq_loc> signal;

    // Look for explicit markers: ideally cleavage products (mature
    // peptides), but possibly just signal peptides
    for (CFeat_CI feat(handle, 0, 0, CSeqFeatData::e_not_set,
                       SAnnotSelector::eOverlap_Intervals,
                       SAnnotSelector::eResolve_TSE);
         feat;  ++feat) {
        bool is_mature = false, is_signal = false;
        const CSeqFeatData& data = feat->GetData();
        switch (data.Which()) {
        case CSeqFeatData::e_Prot:
            switch (data.GetProt().GetProcessed()) {
            case CProt_ref::eProcessed_mature:         is_mature = true; break;
            case CProt_ref::eProcessed_signal_peptide: is_signal = true; break;
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
        if ( signal.NotEmpty() ) {
            // Expects to see at beginning; is this assumption safe?
            CSeq_interval& interval = whole->SetInt();
            interval.SetFrom(signal->GetTotalRange().GetTo() + 1);
            interval.SetTo(v.size() - 1);
            interval.SetId(*const_cast<CSeq_id*>(core->GetId().front().GetPointer()));                
        } else if (v[0] == 'M') { // Treat initial methionine as start codon
            CSeq_interval& interval = whole->SetInt();
            interval.SetFrom(1);
            interval.SetTo(v.size() - 1);
            interval.SetId(*const_cast<CSeq_id*>(core->GetId().front().GetPointer()));
        }
        locations.insert(CConstRef<CSeq_loc>(whole));
    }

    ITERATE(set<CConstRef<CSeq_loc> >, it, locations) {
        try {
            weights[*it] = GetProteinWeight(handle, *it);
        } catch (CBadResidueException) {
            // Silently elide
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ===========================================================================
* $Log$
* Revision 1.29  2004/05/25 15:38:23  ucko
* Remove inappropriate THROWS declaration from GetProteinWeight.
*
* Revision 1.28  2004/05/21 21:42:14  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.27  2004/04/05 15:56:14  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.26  2003/11/19 22:18:06  grichenk
* All exceptions are now CException-derived. Catch "exception" rather
* than "runtime_error".
*
* Revision 1.25  2003/06/02 18:58:25  dicuccio
* Fixed #include directives
*
* Revision 1.24  2003/06/02 18:53:32  dicuccio
* Fixed bungled commit
*
* Revision 1.22  2003/05/27 19:44:10  grichenk
* Added CSeqVector_CI class
*
* Revision 1.21  2003/03/18 21:48:35  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.20  2003/03/11 16:00:58  kuznets
* iterate -> ITERATE
*
* Revision 1.19  2003/02/11 20:49:39  ucko
* Improve support for signal peptide features: ignore them if they have
* WHOLE locations, and optimize logic to avoid GetMappedFeature (expensive).
*
* Revision 1.18  2003/02/10 15:54:01  grichenk
* Use CFeat_CI->GetMappedFeature() and GetOriginalFeature()
*
* Revision 1.17  2002/12/24 16:12:00  ucko
* Make handle const per recent changes to CFeat_CI.
*
* Revision 1.16  2002/12/06 15:36:05  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.15  2002/11/18 19:48:45  grichenk
* Removed "const" from datatool-generated setters
*
* Revision 1.14  2002/11/08 19:43:38  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.13  2002/11/04 21:29:19  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.12  2002/09/03 21:27:04  grichenk
* Replaced bool arguments in CSeqVector constructor and getters
* with enums.
*
* Revision 1.11  2002/06/12 14:39:05  grichenk
* Renamed enumerators
*
* Revision 1.10  2002/06/07 18:19:54  ucko
* Reworked to take advantage of CBioseq_Handle::GetSequenceView.
*
* Revision 1.9  2002/06/06 18:38:11  clausen
* Added include for object_manager.hpp
*
* Revision 1.8  2002/05/10 14:54:12  ucko
* Dropped test for negative start positions, which are now impossible.
*
* Revision 1.7  2002/05/06 17:12:29  ucko
* Take advantage of new eResolve_TSE option.
*
* Revision 1.6  2002/05/06 16:11:49  ucko
* Update for new OM; move CVS log to end.
*
* Revision 1.5  2002/05/06 03:39:13  vakatov
* OM/OM1 renaming
*
* Revision 1.4  2002/05/03 21:28:20  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.3  2002/04/09 20:58:09  ucko
* Look for "processed active peptide" in addition to "mature chain";
* SWISS-PROT changed their labels.
*
* Revision 1.2  2002/03/21 18:57:31  ucko
* Fix check for initial Met.
*
* Revision 1.1  2002/03/06 22:08:40  ucko
* Add code to calculate protein weights.
* ===========================================================================
*/
