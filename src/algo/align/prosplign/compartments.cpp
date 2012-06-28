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
#include <objects/seq/seq__.hpp>

#include <algo/align/prosplign/compartments.hpp>
#include <algo/align/prosplign/prosplign_exception.hpp>

#include <algo/align/util/hit_comparator.hpp>
#include <algo/align/util/compartment_finder.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(prosplign)
USING_SCOPE(ncbi::objects);


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


void RestoreOriginalHits(THitRefs& hitrefs,
                         const THitRefs& orig_hitrefs,
                         bool is_protein_subject)
{
    NON_CONST_ITERATE (THitRefs, h, hitrefs) {

        TSeqPos subj_start = (*h)->GetSubjStart();
        TSeqPos subj_stop = (*h)->GetSubjStop();
        TSeqPos qry_start = (*h)->GetQueryStart();
        TSeqPos qry_stop = (*h)->GetQueryStop();

        if (!is_protein_subject) {
            qry_start /=3;
            qry_stop /=3;
        }

        //find hit with same boundaries and max score
        double score = 0;
        ITERATE(THitRefs, oh, orig_hitrefs) {
            if ((*oh)->GetSubjStart() == subj_start &&
                subj_stop == (*oh)->GetSubjStop() &&
                (*oh)->GetQueryStart() == qry_start &&
                qry_stop == (*oh)->GetQueryStop() &&
                score < (*oh)->GetScore()) {
                score = (*oh)->GetScore();
                **h = **oh;
            }
        }
    }
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
    double result = 0;
    ITERATE(THitRefs, i, hitrefs) {
        result += (*i)->GetScore();
    }
    return result;
}

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

CRef<CSeq_annot> MakeCompartment(THitRefs& hitrefs)
{
    _ASSERT( !hitrefs.empty() );

    CRef<CSeq_align> seq_align(new CSeq_align);
    CSeq_align& compartment = *seq_align;
    
    compartment.SetType(CSeq_align::eType_partial);
    CSeq_align::TSegs::TStd& std_segs = compartment.SetSegs().SetStd();

    TSeqPos subj_leftmost = hitrefs.front()->GetSubjMin();
    TSeqPos subj_rightmost = 0;


    ITERATE (THitRefs, h, hitrefs) {

        bool strand = (*h)->GetSubjStrand();
        TSeqPos subj_min = (*h)->GetSubjMin();
        TSeqPos subj_max = (*h)->GetSubjMax();
        TSeqPos qry_min = (*h)->GetQueryMin();
        TSeqPos qry_max = (*h)->GetQueryMax();
        double pct_identity =(*h) ->GetIdentity();
        double bit_score = (*h)->GetScore();

        subj_leftmost = min(subj_leftmost, subj_min);
        subj_rightmost = max(subj_rightmost, subj_max);

        CRef<CStd_seg> std_seg(new CStd_seg);

        CRef<CSeq_id> qry_id(new CSeq_id);
        qry_id->Assign(*(*h)->GetQueryId());
        CRef<CSeq_loc> qry_loc(new CSeq_loc(*qry_id,qry_min,qry_max,eNa_strand_plus));
        std_seg->SetLoc().push_back(qry_loc);

        CRef<CSeq_id> subj_id(new CSeq_id);
        subj_id->Assign(*(*h)->GetSubjId());
        CRef<CSeq_loc> subj_loc(new CSeq_loc(*subj_id,subj_min,subj_max,strand?eNa_strand_plus:eNa_strand_minus));
        std_seg->SetLoc().push_back(subj_loc);

        std_seg->SetScores().push_back(RealScore("pct_identity",pct_identity*100));
        std_seg->SetScores().push_back(RealScore("bit_score",bit_score));

        std_segs.push_back(std_seg);
    }

    CRef<CSeq_annot> result(new CSeq_annot);
    result->SetData().SetAlign().push_back(seq_align);

    CRef<CUser_object> uo(new CUser_object);
    uo->SetType().SetStr("Compart Scores");
    uo->AddField("bit_score", TotalScore(hitrefs));
    uo->AddField("num_covered_aa", CountQueryCoverage(hitrefs));
    result->AddUserObject( *uo );

    CRef<CSeq_id> qry_id(new CSeq_id);
    qry_id->Assign(*hitrefs.front()->GetQueryId());
    CRef<CAnnotdesc> align(new CAnnotdesc);
    align->SetAlign().SetAlign_type(CAlign_def::eAlign_type_ref);
    align->SetAlign().SetIds().push_back( qry_id );
    result->SetDesc().Set().push_back( align );

    CRef<CSeq_id> subj_id(new CSeq_id);
    subj_id->Assign(*hitrefs.front()->GetSubjId());
    CRef<CSeq_loc> subj_loc(new CSeq_loc(*subj_id, subj_leftmost, subj_rightmost, hitrefs.front()->GetSubjStrand()?eNa_strand_plus:eNa_strand_minus));
    CRef<CAnnotdesc> region(new CAnnotdesc);
    region->SetRegion(*subj_loc);
    result->SetDesc().Set().push_back(region);

    return result;
}

auto_ptr<CCompartmentAccessor<THit> > CreateCompartmentAccessor(const THitRefs& orig_hitrefs,
                                                                CCompartOptions compart_options)
{
    auto_ptr<CCompartmentAccessor<THit> > comps_ptr;
    if (orig_hitrefs.empty())
        return comps_ptr;

    THitRefs hitrefs;

    bool is_protein_subject = double(orig_hitrefs.front()->GetSubjSpan())/orig_hitrefs.front()->GetQuerySpan() < 2;

    ITERATE(THitRefs, it, orig_hitrefs) {
        THitRef hitref(new THit(**it));
        if (!hitref->GetQueryStrand())
            NCBI_THROW(CProSplignException, eFormat, "minus strand on protein in BLAST hit");
        if (!is_protein_subject) {
            // set max first, otherwise min can be more than max and will throw an error
            hitref->SetQueryMax(hitref->GetQueryMax()*3+2);
            hitref->SetQueryMin(hitref->GetQueryMin()*3);
        }
        if (compart_options.m_Maximizing == CCompartOptions::eCoverage)
            hitref->SetIdentity(0.9999f);
        else if (compart_options.m_Maximizing == CCompartOptions::eScore)
            hitref->SetIdentity(hitref->GetScore()/hitref->GetLength());

        hitrefs.push_back(hitref);
    }

    //count 'pseudo' length	
    int len = CountQueryCoverage(hitrefs);

    comps_ptr.reset
        (new  CCompartmentAccessor<THit>( int(compart_options.m_CompartmentPenalty * len),
                                          int(compart_options.m_MinCompartmentIdty * len),
                                          int(compart_options.m_MinSingleCompartmentIdty * len)));

    CCompartmentAccessor<THit>& comps = *comps_ptr;

    comps.SetMaxIntron(compart_options.m_MaxIntron);
    comps.Run(hitrefs.begin(), hitrefs.end());

    THitRefs comphits;
    if(comps.GetFirst(comphits)) {
        do {
            RestoreOriginalHits(comphits, orig_hitrefs, is_protein_subject);
            RemoveOverlaps(comphits);

        } while (comps.GetNext(comphits));
    }
    return comps_ptr;
}

TCompartments SelectCompartmentsHits(const THitRefs& orig_hitrefs, CCompartOptions compart_options)
{
    auto_ptr<CCompartmentAccessor<THit> >  comps_ptr =
        CreateCompartmentAccessor( orig_hitrefs, compart_options);

    TCompartments results = FormatAsAsn(comps_ptr.get(), compart_options);
    return results;
}

TCompartments FormatAsAsn(CCompartmentAccessor<THit>* comps_ptr, CCompartOptions compart_options)
{
    TCompartments results;
    if (comps_ptr == NULL)
        return results;

    CCompartmentAccessor<THit>& comps = *comps_ptr;

    THitRefs comphits;
    if(comps.GetFirst(comphits)) {
    CRef<CSeq_loc> prev_compartment_loc;
    const TSeqPos max_extent = compart_options.m_MaxExtent;

        size_t i = 0;
        do {
            CRef<CSeq_annot> compartment = MakeCompartment(comphits);

            const TSeqPos* boxPtr = comps.GetBox(i);
            TSeqPos cur_begin = boxPtr[2];
            TSeqPos cur_end = boxPtr[3];
            TSeqPos cur_begin_extended = cur_begin < max_extent ? 0 : cur_begin - max_extent;
            TSeqPos cur_end_extended = cur_end + max_extent;

            CRef<CSeq_loc> cur_compartment_loc(&compartment->SetDesc().Set().back()->SetRegion());
            cur_compartment_loc->SetInt().SetFrom(cur_begin_extended);
            cur_compartment_loc->SetInt().SetTo(cur_end_extended);

            if (prev_compartment_loc.NotEmpty() &&
                prev_compartment_loc->GetId()->Match(*cur_compartment_loc->GetId()) &&
                prev_compartment_loc->GetStrand()==cur_compartment_loc->GetStrand()
               ) {
                TSeqPos prev_end_extended = prev_compartment_loc->GetStop(eExtreme_Positional);
                TSeqPos prev_end = prev_end_extended - max_extent;
                _ASSERT(prev_end < cur_begin);
                if (prev_end_extended >= cur_begin_extended) {
                    prev_end_extended = (prev_end + cur_begin)/2;
                    cur_begin_extended = prev_end_extended+1;
                    _ASSERT(cur_begin_extended <= cur_begin);

                    prev_compartment_loc->SetInt().SetTo(prev_end_extended);
                    cur_compartment_loc->SetInt().SetFrom(cur_begin_extended);
                }
            }
            prev_compartment_loc=cur_compartment_loc;

            results.push_back(compartment);
            ++i;
        } while (comps.GetNext(comphits));
    }

    return results;
}

TCompartmentStructs MakeCompartments(const TCompartments& compartments, CCompartOptions compart_options)
{
    TCompartmentStructs results;

    ITERATE(TCompartments, i, compartments) {
        const CSeq_annot& comp = **i;
        const CSeq_loc* subj_loc = NULL;
        int covered_aa = 0;
        double score = 0;
        ITERATE (CAnnot_descr::Tdata, desc_it, comp.GetDesc().Get()) {
            const CAnnotdesc& desc = **desc_it;
            if (desc.IsRegion()) {
                subj_loc = &desc.GetRegion();
            } else if (desc.IsUser() && desc.GetUser().GetType().IsStr() && desc.GetUser().GetType().GetStr()=="Compart Scores") {
                covered_aa = desc.GetUser().GetField("num_covered_aa").GetData().GetInt();
                score = desc.GetUser().GetField("bit_score").GetData().GetReal();
            }
        }
        if (subj_loc)
            results.push_back(SCompartment(subj_loc->GetStart(eExtreme_Positional),
                                           subj_loc->GetStop(eExtreme_Positional),
                                           subj_loc->GetStrand()!=eNa_strand_minus,
                                           covered_aa, score));
    }
    sort(results.begin(),results.end());

    return results;
}

TCompartmentStructs MakeCompartments(const CSplign::THitRefs& hitrefs, CCompartOptions compart_options)
{
    return MakeCompartments(SelectCompartmentsHits(hitrefs, compart_options), compart_options);
}

const double CCompartOptions::default_CompartmentPenalty = 0.5;
const double CCompartOptions::default_MinCompartmentIdty = 0.5;
const double CCompartOptions::default_MinSingleCompartmentIdty = 0.25;
const int CCompartOptions::default_MaxIntron = CCompartmentFinder<THit>::s_GetDefaultMaxIntron();
const char* CCompartOptions::s_scoreNames[] = {"coverage", "identity", "score"};

void CCompartOptions::SetupArgDescriptions(CArgDescriptions* argdescr)
{
    argdescr->AddDefaultKey
        ("max_extent",
         "max_extent",
         "Max genomic extent to look for exons beyond compartment ends.",
         CArgDescriptions::eInteger,
         NStr::IntToString(CCompartOptions::default_MaxExtent) );

    argdescr->AddDefaultKey
        ("compartment_penalty",
         "double",
         "Penalty to open a new compartment "
         "(compartment identification parameter). "
         "Multiple compartments will only be identified if "
         "they have at least this level of coverage.",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CCompartOptions::default_CompartmentPenalty, 2));
    
    argdescr->AddDefaultKey
        ("min_compartment_idty",
         "double",
         "Minimal compartment identity for multiple compartments",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CCompartOptions::default_MinCompartmentIdty, 2));
    
    argdescr->AddDefaultKey
        ("min_singleton_idty",
         "double",
         "Minimal compartment identity for single compartment",
         CArgDescriptions::eDouble,
         NStr::DoubleToString(CCompartOptions::default_MinSingleCompartmentIdty, 2));
    
    argdescr->AddDefaultKey
        ("by_coverage",
         "flag",
         "Ignore hit identity. Set all to 99.99%\nDeprecated: use -maximize arg",
         CArgDescriptions::eBoolean, default_ByCoverage?"T":"F");
    
    argdescr->AddDefaultKey
        ("max_intron",
         "integer",
         "Maximal intron length",
         CArgDescriptions::eInteger,
         NStr::IntToString(CCompartOptions::default_MaxIntron));

    argdescr->AddDefaultKey
        ("maximize",
         "param",
         "parameter to maximize",
         CArgDescriptions::eString,
         s_scoreNames[default_Maximizing]);

    argdescr->SetConstraint("maximize",
                             &(*new CArgAllow_Strings,
                               s_scoreNames[eCoverage], s_scoreNames[eIdentity], s_scoreNames[eScore]));

    argdescr->SetDependency("by_coverage", CArgDescriptions::eExcludes,
                            "maximize");
}

CCompartOptions::CCompartOptions(const CArgs& args) :
    m_CompartmentPenalty(args["compartment_penalty"].AsDouble()),
    m_MinCompartmentIdty(args["min_compartment_idty"].AsDouble()),
    m_MinSingleCompartmentIdty(args["min_singleton_idty"].AsDouble()),
    m_MaxExtent(args["max_extent"].AsInteger()),
    m_MaxIntron(args["max_intron"].AsInteger())
{
    if (args["maximize"]) { 
        m_Maximizing = default_Maximizing;
        for (size_t i = 0; i < sizeof(s_scoreNames)/sizeof(s_scoreNames[0]); ++i) {
            if (args["maximize"].AsString() == s_scoreNames[i]) {
                m_Maximizing = EMaximizing(i);
                break;
            }
        }
        m_ByCoverage = m_Maximizing == eCoverage;
    } else {
        if (args["by_coverage"]) {
            m_ByCoverage = args["by_coverage"].AsBoolean();
        } else {
            m_ByCoverage = default_ByCoverage;
        }
        m_Maximizing = m_ByCoverage ? eCoverage : eIdentity;
    }
}

END_SCOPE(prosplign)
END_NCBI_SCOPE
