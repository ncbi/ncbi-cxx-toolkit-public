/* $Id$
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
 * Author:   Boris Kiryutin, Vyacheslav Chetvernin
 *
 * File Description:  Get protein compartments from BLAST hits
 *
 */

#include <ncbi_pch.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <algo/align/prosplign/compartments.hpp>

#include <algo/align/util/hit_comparator.hpp>
#include <algo/align/util/compartment_finder.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
USING_SCOPE(ncbi::objects);

typedef CSplign::THit     THit;
typedef CSplign::THitRef  THitRef;
typedef CSplign::THitRefs THitRefs;

int CountQueryCoverage(THitRefs& hitrefs)
{
    typedef CHitComparator<THit> THitComparator;
    THitComparator sorter (THitComparator::eQueryMin);
    stable_sort(hitrefs.begin(), hitrefs.end(), sorter);

    int len = 0;
    TSeqPos stmin = hitrefs.front()->GetQueryMin();
    TSeqPos enmax = hitrefs.front()->GetQueryMax();

    ITERATE(THitRefs, it, hitrefs) {
        if((*it)->GetQueryMin() <= enmax) {//overlaping hits
            enmax = max((*it)->GetQueryMax(), enmax);
        } else {//next chain starts
            len += enmax - stmin + 1;
            stmin = (*it)->GetQueryMin();
            enmax = (*it)->GetQueryMax();
        }
    }
    len += enmax - stmin + 1;

    _ASSERT( len>0 );
    return len;
}

void RemoveOverlaps(THitRefs& hitrefs)
{
    THitRefs hits_new;
    CHitFilter<THit>::s_RunGreedy(hitrefs.begin(), hitrefs.end(), 
                                  &hits_new, 1, 0);
    hitrefs.erase(remove_if(hitrefs.begin(), hitrefs.end(), CHitFilter<THit>::s_PNullRef), 
                  hitrefs.end());
    copy(hits_new.begin(), hits_new.end(), back_inserter(hitrefs));
}

double TotalScore(THitRefs& hitrefs)
{
    double result;
    ITERATE(THitRefs, i, hitrefs) {
        result += (*i)->GetScore();
    }
    return result;
}

class NotEnoughAA {
public:
    NotEnoughAA(int requered_aa) : m_requered_aa(requered_aa) {}
    int m_requered_aa;
    bool operator()(const SCompartment& compartment)
    {
        return compartment.covered_aa < m_requered_aa;
    }
};

CRef<CScore> IntScore(const string& id, int value)
{
    CRef<CScore> result(new CScore);
    result->SetId().SetStr(id);
    result->SetValue().SetInt(value);
    return result;
}

CRef<CScore> RealScore(const string& id, double value)
{
    CRef<CScore> result(new CScore);
    result->SetId().SetStr(id);
    result->SetValue().SetReal(value);
    return result;
}

CRef<CSeq_align> MakeCompartment(THitRefs& hitrefs)
{
    _ASSERT( !hitrefs.empty() );
    CRef<CSeq_align> result(new CSeq_align);
    CSeq_align& compartment = *result;
    
    compartment.SetType(CSeq_align::eType_partial);
    CSeq_align::TSegs::TStd& std_segs = compartment.SetSegs().SetStd();
    ITERATE (THitRefs, h, hitrefs) {
        CRef<CStd_seg> std_seg(new CStd_seg);

        CRef<CSeq_id> qry_id(new CSeq_id);
        qry_id->Assign(*(*h)->GetQueryId());
        CRef<CSeq_loc> qry_loc(new CSeq_loc(*qry_id,(*h)->GetQueryMin(),(*h)->GetQueryMax(),(*h)->GetQueryStrand()?eNa_strand_plus:eNa_strand_minus));
        std_seg->SetLoc().push_back(qry_loc);

        CRef<CSeq_id> subj_id(new CSeq_id);
        subj_id->Assign(*(*h)->GetSubjId());
        CRef<CSeq_loc> subj_loc(new CSeq_loc(*subj_id,(*h)->GetSubjMin(),(*h)->GetSubjMax(),(*h)->GetSubjStrand()?eNa_strand_plus:eNa_strand_minus));
        std_seg->SetLoc().push_back(subj_loc);

        std_seg->SetScores().push_back(IntScore("pct_identity",int((*h)->GetIdentity()*100)));
        std_seg->SetScores().push_back(RealScore("bit_score",fabs((*h)->GetScore())));

        std_segs.push_back(std_seg);
    }
    return result;
}

CRef<CSeq_align_set> SelectCompartmentsHits(THitRefs& hitrefs, CCompartOptions compart_options)
{
    CRef<CSeq_align_set> results (new CSeq_align_set);
    if (hitrefs.empty())
        return results;

    NON_CONST_ITERATE(THitRefs, it, hitrefs) {
        (*it)->SetQueryMax(((*it)->GetQueryMax()+1)*3-1);
        (*it)->SetQueryMin((*it)->GetQueryMin()*3);
        (*it)->SetIdentity(0.99);
    }

    //count 'pseudo' length	
    int len = CountQueryCoverage(hitrefs);

    CCompartmentAccessor<THit> comps ( hitrefs.begin(), hitrefs.end(),
                                       int(compart_options.m_CompartmentPenalty * len), int(compart_options.m_MinCompartmentIdty * len) );

    THitRefs comphits;
    if(comps.GetFirst(comphits)) {
        size_t i = 0;
        do {
            RemoveOverlaps(comphits);

            CRef<CSeq_align> compartment = MakeCompartment(comphits);

            compartment->SetNamedScore(CSeq_align::eScore_BitScore, TotalScore(comphits));
            compartment->SetNamedScore("num_covered_aa", CountQueryCoverage(comphits)/3);

            const THit::TCoord* boxPtr = comps.GetBox(i);

            CRef<CSeq_id> qry_id(new CSeq_id);
            qry_id->Assign(*hitrefs.front()->GetQueryId());
            CRef<CSeq_loc> qry_loc(new CSeq_loc(*qry_id,boxPtr[0],boxPtr[1],eNa_strand_plus));
            compartment->SetBounds().push_back(qry_loc);

            CRef<CSeq_id> subj_id(new CSeq_id);
            subj_id->Assign(*hitrefs.front()->GetSubjId());
            CRef<CSeq_loc> subj_loc(new CSeq_loc(*subj_id,boxPtr[2],boxPtr[3],comps.GetStrand(i)?eNa_strand_plus:eNa_strand_minus));
            compartment->SetBounds().push_back(subj_loc);

            results->Set().push_back(compartment);
            ++i;
        } while (comps.GetNext(comphits));
    }

    return results;
}

TCompartments MakeCompartments(const CSeq_align_set& compartments, CCompartOptions compart_options)
{
    TCompartments results;

    ITERATE(CSeq_align_set::Tdata, i, compartments.Get()) {
        const CSeq_align& comp = **i;
        const CSeq_loc& subj_loc = *comp.GetBounds().back();
        int covered_aa = 0;
        comp.GetNamedScore("num_covered_aa", covered_aa);
        double score = 0;
        comp.GetNamedScore(CSeq_align::eScore_BitScore, score);
        results.push_back(SCompartment(subj_loc.GetStart(eExtreme_Positional),subj_loc.GetStop(eExtreme_Positional),subj_loc.GetStrand()!=eNa_strand_minus,
                                       covered_aa, score));
    }
    sort(results.begin(),results.end());

    const int max_extent = compart_options.m_MaxExtent;

    for (size_t i=0; i<results.size(); ++i) {
        SCompartment& comp = results[i];
        int prev = (i==0 || results[i-1].strand != comp.strand) ?
            -comp.from :
            results[i-1].to;
        int next = (i==results.size()-1 || results[i+1].strand != comp.strand) ?
            comp.to+2*max_extent+2 :
            results[i+1].from;
        comp.from = max(comp.from-max_extent,(prev+comp.from)/2);
        comp.to = min(comp.to+max_extent,(comp.to+next)/2-1);
    }

    return results;
}

TCompartments MakeCompartments(CSplign::THitRefs& hitrefs, CCompartOptions compart_options, int protein_len)
{
    TCompartments results = MakeCompartments(*SelectCompartmentsHits(hitrefs, compart_options), compart_options);
    
    const int required_aa = int(protein_len * compart_options.m_MinProteinCoverage);

    TCompartments::iterator new_end = remove_if(results.begin(),results.end(),NotEnoughAA(required_aa));
    results.erase(new_end, results.end());
    
    return results;
}


void CCompartOptions::SetupArgDescriptions(CArgDescriptions* argdescr)
{
    argdescr->AddDefaultKey
        ("max_extent",
         "max_extent",
         "Max genomic extent to look for exons beyond compartment ends.",
         CArgDescriptions::eInteger,
         "500" );

    argdescr->AddDefaultKey
        ("compartment_penalty",
         "compartment_penalty",
         "Penalty to open a new compartment "
         "(compartment identification parameter). "
         "Multiple compartments will only be identified if "
         "they have at least this level of coverage.",
         CArgDescriptions::eDouble,
         "0.125");
    
    argdescr->AddDefaultKey
        ("min_compartment_idty",
         "min_compartment_identity",
         "Minimal compartment identity to align.",
         CArgDescriptions::eDouble,
         ".333333");
    
    argdescr->AddDefaultKey
        ("min_prot_coverage",
         "min_prot_coverage",
         "Minimal protein coverage by hits.",
         CArgDescriptions::eDouble,
         ".25");
    
}

CCompartOptions::CCompartOptions(const CArgs& args) :
    m_CompartmentPenalty(args["compartment_penalty"].AsDouble()),
    m_MinCompartmentIdty(args["min_compartment_idty"].AsDouble()),
    m_MaxExtent(args["max_extent"].AsInteger()),
    m_MinProteinCoverage(args["min_prot_coverage"].AsDouble())
{
}

END_SCOPE(prosplign)
END_NCBI_SCOPE
