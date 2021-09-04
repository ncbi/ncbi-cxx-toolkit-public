#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiobj.hpp>

#include <objtools/validator/feature_match.hpp>
#include <objmgr/mapped_feat.hpp>

#include <objects/seq/Bioseq.hpp>

//#include <objmgr/bioseq_handle.hpp>

#include <objects/seqfeat/seqfeat_macros.hpp>
#include <objmgr/util/sequence.hpp>
#include <objmgr/util/feature.hpp>

#include <objtools/validator/utilities.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

//using namespace sequence;
//LCOV_EXCL_START
//not currently using
bool CMatchFeat::operator < (const CMatchFeat& o) const
{
    const CMatchFeat& l = *this;
    const CMatchFeat& r = o;

    const CSeq_feat& lf = l.GetFeat();
    const CSeq_feat& rf = r.GetFeat();

    if (l.m_pos_start != r.m_pos_start)
        return l.m_pos_start < r.m_pos_start;

    if (l.m_pos_stop != r.m_pos_stop)
        return l.m_pos_stop < r.m_pos_stop;

    if (lf.Compare(rf) < 0)
        return true;

    return false;
}

struct feat_loc_compare
{
    template<class _T>
    bool operator()(const CRef<_T>& l, const CRef<_T>& r) const
    {
        return *l < *r;
    }
};

template<typename TFeatList>
void s_SetUpXrefPairs(
    vector < CRef<CMatchCDS> > & cds_list,
    vector < CRef<CMatchmRNA> > & mrna_list,
    const TFeatList & feat_list, CScope* scope, ENoteCDS eNoteCDS)
{
    // predict the size of the final arrays
    cds_list.reserve(feat_list.size() / 2);
    mrna_list.reserve(feat_list.size() / 2);

    // fill in cds_list, mrna_list
    for (auto& feat_it : feat_list) {
        const CSeq_feat& feature = feat_it.GetOriginalFeature();
        if (feature.IsSetData()) {
            if (feature.GetData().IsCdregion()) {
                cds_list.push_back(Ref(new CMatchCDS(feat_it)));
            }
            else
                if (feature.GetData().IsRna() &&
                    feature.GetData().GetSubtype() == CSeqFeatData::eSubtype_mRNA) {
                    mrna_list.push_back(Ref(new CMatchmRNA(feat_it)));
                }
        }
    }

    // sort them
    sort(cds_list.begin(), cds_list.end(), feat_loc_compare());
    sort(mrna_list.begin(), mrna_list.end(), feat_loc_compare());

    if (cds_list.empty() || mrna_list.empty())
        return;

    auto mrna_begin = mrna_list.begin();

    size_t compare_attempts = 0;
    for (auto& cds_it : cds_list)
    {
        CMatchCDS& cds_match = *cds_it;
        const CSeq_feat& cds = cds_match.GetFeat();

        sequence::EOverlapType overlap_type = sequence::eOverlap_CheckIntRev;
        if (cds.IsSetExcept_text()
            && (NStr::FindNoCase(cds.GetExcept_text(), "ribosomal slippage") != string::npos
            || NStr::FindNoCase(cds.GetExcept_text(), "trans-splicing") != string::npos)) {
            overlap_type = sequence::eOverlap_SubsetRev;
        }


        while (mrna_begin != mrna_list.end() &&
            (**mrna_begin).maxpos() < cds_match.minpos())
            mrna_begin++;

        for (auto mrna_it = mrna_begin;
            mrna_it != mrna_list.end() &&
            (**mrna_it).minpos() <= cds_match.maxpos() &&
            cds_match.minpos() <= (**mrna_it).maxpos();
        ++mrna_it)
        {
            CMatchmRNA& mrna_match = **mrna_it;
            const CSeq_feat& mrna = mrna_match.GetFeat();
            if (!mrna_match.IsAccountedFor()) {
                compare_attempts++;
                if (TestForOverlapEx(cds.GetLocation(), mrna.GetLocation(), overlap_type, scope) >= 0) {
                    cds_match.AddmRNA(mrna_match);
                    if (eNoteCDS == eNoteCDS_No) {
                        mrna_match.AddCDS(cds_match);
                    }
                    if (validator::s_IdXrefsAreReciprocal(cds, mrna)) {
                        cds_match.SetXrefMatch(mrna_match);
                        if (eNoteCDS == eNoteCDS_No) {
                            mrna_match.SetCDS(cds);
                        }
                        else {
                            mrna_match.SetAccountedFor(true);
                        }
                        mrna_match.SetCDS(cds);
                    }
                }
                else
                {
                    //cout << "problem" << endl;
                }
            }
        }

        if (!cds_match.HasmRNA()) {
            cds_match.AssignSinglemRNA();
        }
    }
    // cout << "Totals CDS, mRNAs, compare attempts:" << cds_list.size() << " " << mrna_list.size() << " " << compare_attempts << endl;
}

CMatchFeat::CMatchFeat(const CMappedFeat &feat) : m_feat(feat.GetSeq_feat())
{
    m_pos_start = m_feat->GetLocation().GetStart(eExtreme_Positional);
    m_pos_stop = m_feat->GetLocation().GetStop(eExtreme_Positional);
}


bool CMatchmRNA::HasCDSMatch()
{
    bool rval = false;

    if (m_Cds) {
        return true;
    }
    else {
        // iterate through underlying cdss that aren't accounted for
        vector < CRef<CMatchCDS> >::iterator cds_it =
            m_UnderlyingCDSs.begin();
        for (; cds_it != m_UnderlyingCDSs.end() && !rval; ++cds_it) {
            if (!(*cds_it)->HasmRNA()) {
                return true;
            }
        }
    }
    return false;
}


bool CMatchmRNA::MatchesUnderlyingCDS(unsigned int partial_type) const
{
    bool rval = false;

    TSeqPos mrna_start = m_feat->GetLocation().GetStart(eExtreme_Biological);
    TSeqPos mrna_stop = m_feat->GetLocation().GetStop(eExtreme_Biological);

    if (m_Cds) {
        if (partial_type == sequence::eSeqlocPartial_Nostart) {
            if (m_Cds->GetLocation().GetStart(eExtreme_Biological) == mrna_start) {
                rval = true;
            }
            else {
                rval = false;
            }
        }
        else if (partial_type == sequence::eSeqlocPartial_Nostop) {
            if (m_Cds->GetLocation().GetStop(eExtreme_Biological) == mrna_stop) {
                rval = true;
            }
            else {
                rval = false;
            }
        }
#if 0
    }
    else {
        // iterate through underlying cdss that aren't accounted for
        vector < CRef<CMatchCDS> >::iterator cds_it =
            m_UnderlyingCDSs.begin();
        while (cds_it != m_UnderlyingCDSs.end() && !rval) {
            if (!(*cds_it)->HasmRNA()) {
                if (partial_type == eSeqlocPartial_Nostart) {
                    if ((*cds_it)->m_Cds->GetLocation().GetStart(eExtreme_Biological) == mrna_start) {
                        rval = true;
                    }
                    else {
                        rval = false;
                    }
                }
                else if (partial_type == eSeqlocPartial_Nostop) {
                    if ((*cds_it)->m_Cds->GetLocation().GetStop(eExtreme_Biological) == mrna_stop) {
                        rval = true;
                    }
                    else {
                        rval = false;
                    }
                }
            }
            ++cds_it;
        }
#endif
    }
    return rval;
}


bool CMatchmRNA::MatchAnyUnderlyingCDS(unsigned int partial_type) const
{
    bool rval = false;

    TSeqPos mrna_start = m_feat->GetLocation().GetStart(eExtreme_Biological);
    TSeqPos mrna_stop = m_feat->GetLocation().GetStop(eExtreme_Biological);

    // iterate through underlying cdss that aren't accounted for
    vector < CRef<CMatchCDS> >::const_iterator cds_it = m_UnderlyingCDSs.begin();
    while (cds_it != m_UnderlyingCDSs.end() && !rval) {
        if (partial_type == sequence::eSeqlocPartial_Nostart) {
            if ((*cds_it)->GetFeat().GetLocation().GetStart(eExtreme_Biological) == mrna_start) {
                rval = true;
            }
            else {
                rval = false;
            }
        }
        else if (partial_type == sequence::eSeqlocPartial_Nostop) {
            if ((*cds_it)->GetFeat().GetLocation().GetStop(eExtreme_Biological) == mrna_stop) {
                rval = true;
            }
            else {
                rval = false;
            }
        }
        ++cds_it;
    }
    return rval;
}


// only assign if the coding region has only one overlapping unaccounted for mRNA
void CMatchCDS::AssignSinglemRNA()
{
    CRef<CMatchmRNA> match;

    vector < CRef<CMatchmRNA> >::iterator mrna_it =
        m_OverlappingmRNAs.begin();
    for (; mrna_it != m_OverlappingmRNAs.end(); ++mrna_it) {
        if (!(*mrna_it)->IsAccountedFor()) {
            if (!match) {
                match.Reset(*mrna_it);
            }
            else {
                // found more than one, can't assign either
                match.Reset();
                break;
            }
        }

    }
    if (match) {
        m_AssignedMrna = match;
        match->SetCDS(*m_feat);
    }
}


int CMatchCDS::GetNummRNA(bool &loc_unique)
{
    int num = 0;
    loc_unique = true;

    if (HasmRNA()) {
        // count the one assigned
        num++;
    }
    vector < CRef<CMatchmRNA> >::iterator mrna_it =
        m_OverlappingmRNAs.begin();
    vector < string > product_list;
    while (mrna_it != m_OverlappingmRNAs.end()) {
        if (!(*mrna_it)->IsAccountedFor()) {
            // count overlapping unassigned mRNAS
            if ((*mrna_it)->GetFeat().IsSetProduct()) {
                string label;
                (*mrna_it)->GetFeat().GetProduct().GetLabel(&label);
                product_list.push_back(label);
            }
            num++;
        }
        ++mrna_it;
    }
    if (product_list.size() > 1) {
        stable_sort(product_list.begin(), product_list.end());
        vector < string >::iterator s1 = product_list.begin();
        vector < string >::iterator s2 = s1;
        s2++;
        while (s2 != product_list.end()) {
            if (NStr::Equal(*s1, *s2)) {
                loc_unique = false;
                break;
            }
            ++s1;
            ++s2;
        }
    }
    return num;
}

CmRNAAndCDSIndex::CmRNAAndCDSIndex()
{
    // nothing needed yet
}


CmRNAAndCDSIndex::~CmRNAAndCDSIndex()
{
    // nothing needed yet
}


void CmRNAAndCDSIndex::SetBioseq(const std::vector<CMappedFeat> *feat_list, CScope* scope)
{
    m_CdsList.clear();
    m_mRNAList.clear();

    if (!feat_list) {
        return;
    }

    s_SetUpXrefPairs(
        m_CdsList, m_mRNAList, *feat_list, scope, eNoteCDS_Yes);
}


CRef<CMatchmRNA> CmRNAAndCDSIndex::FindMatchmRNA(const CMappedFeat& mrna)
{
    vector < CRef<CMatchmRNA> >::iterator mrna_it = m_mRNAList.begin();
    for (; mrna_it != m_mRNAList.end(); ++mrna_it) {
        if (mrna.GetOriginalFeature().Equals((*mrna_it)->GetFeat())) {
            return (*mrna_it);
        }
    }
    return CRef<CMatchmRNA>();
}


bool CmRNAAndCDSIndex::MatchmRNAToCDSEnd(const CMappedFeat& mrna, unsigned int partial_type)
{
    bool rval = false;

    CRef<CMatchmRNA> match_mrna = FindMatchmRNA(mrna);
    if (match_mrna && match_mrna->MatchesUnderlyingCDS(partial_type)) {
        rval = true;
    }
    return rval;
}

//LCOV_EXCL_STOP




END_SCOPE(objects)
END_NCBI_SCOPE
