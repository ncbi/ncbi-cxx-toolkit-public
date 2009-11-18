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
 * Authors:  Vyacheslav Chetvernin
 *
 * File Description: 
 *
 * Builds annotation models out of chained alignments:
 * selects good chains as alternatively spliced genes,
 * selects good chains inside other chains introns,
 * other chains filtered to leave one chain per placement,
 * gnomon is run to improve chains and predict models in regions w/o chains
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/gnomon/annot.hpp>
#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/gnomon.hpp>

#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(gnomon)

CGnomonAnnotator::CGnomonAnnotator()
{
}

bool s_AlignScoreOrder(const CGeneModel& ap, const CGeneModel& bp)
{
    return (ap.Score() < bp.Score());
}

void CGnomonAnnotator::RemoveShortHolesAndRescore(TGeneModelList chains, const CGnomonEngine& gnomon)
{
    NON_CONST_ITERATE(TGeneModelList, it, chains) {
        it->RemoveShortHolesAndRescore(gnomon);
    }
}

double CGnomonAnnotator::ExtendJustThisChain(CGeneModel& chain,
                                             CGnomonEngine& gnomon, TSignedSeqPos left, TSignedSeqPos right,
                                             double mpp, double nonconsensp)
{
    TGeneModelList test_align;
    test_align.push_back(chain);
    int l = max((int)left,(int)chain.Limits().GetFrom()-10000);
    int r = min(right,chain.Limits().GetTo()+10000);
    cout << "Testing alignment " << chain.ID() << " in fragment " << l << ' ' << r << endl;
                    
    gnomon.ResetRange(l,r);
    return gnomon.Run(test_align, true, false, false, false, false, mpp, nonconsensp);
}

double CGnomonAnnotator::TryWithoutObviouslyBadAlignments(TGeneModelList& aligns, TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                                          CGnomonEngine& gnomon,
                                                          bool leftwall, bool rightwall, bool leftanchor, bool rightanchor,
                                                          TSignedSeqPos left, TSignedSeqPos right,
                                                          double mpp, double nonconsensp, TSignedSeqRange& tested_range)
{
    bool already_tested = Include(tested_range, TSignedSeqRange(left,right));

    if (already_tested) {
        for(TGeneModelList::iterator it = aligns.begin(); it != aligns.end(); it++) {
            if(left <= it->Limits().GetTo() && it->Limits().GetFrom() <= right)
                suspect_aligns.push_back(*it);
        }
    } else {
        tested_range = TSignedSeqRange(left,right);
        
        bool found_bad_cluster = false;
        for(TGeneModelList::iterator it = aligns.begin(); it != aligns.end(); ) {
            if(it->Limits().GetTo() < left || it->Limits().GetFrom() > right) {
                ++it;
                continue;
            }
            
            if ((it->Type() & (CGeneModel::eWall | CGeneModel::eNested))==0 &&
                ExtendJustThisChain(*it, gnomon, left, right, mpp, nonconsensp) == BadScore()) {
                found_bad_cluster = true;
                cout << "Deleting alignment " << it->ID() << endl;
                it->Status() |= CGeneModel::eSkipped;
                it->AddComment("Bad score prediction alone");
                bad_aligns.push_back(*it);
                
                it = aligns.erase(it);
                continue;
            }
            suspect_aligns.push_back(*it++);
        }
        
        gnomon.ResetRange(left, right);
        if(found_bad_cluster) {
            cout << "Testing w/o bad alignments in fragment " << left << ' ' << right << endl;
            return gnomon.Run(suspect_aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp);
        }
    }
    return BadScore();
}

double CGnomonAnnotator::TryToEliminateOneAlignment(TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                      CGnomonEngine& gnomon,
                                      bool leftwall, bool rightwall, bool leftanchor, bool rightanchor,
                                      double mpp, double nonconsensp)
{
    double score = BadScore();
    for(TGeneModelList::iterator it = suspect_aligns.begin(); it != suspect_aligns.end();) {
        if((it->Type() & (CGeneModel::eWall | CGeneModel::eNested))!=0) {
            ++it;
            continue;
        }
        CGeneModel algn = *it;
        it = suspect_aligns.erase(it);
        
        cout << "Testing w/o " << algn.ID();
        score = gnomon.Run(suspect_aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp);
        if (score != BadScore()) {
            cout << "- Good. Deleting alignment " << algn.ID() << endl;
            algn.Status() |= CGeneModel::eSkipped;
            algn.AddComment("Good score prediction without");
            bad_aligns.push_back(algn);
            break;
        } else {
            cout << " - Still bad." << endl;                        
        }
        suspect_aligns.insert(it,algn);
    }
    return score;
}

double CGnomonAnnotator::TryToEliminateAlignmentsFromTail(TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                      CGnomonEngine& gnomon,
                                      bool leftwall, bool rightwall, bool leftanchor, bool rightanchor,
                                      double mpp, double nonconsensp)
{
    double score = BadScore();
    for (TGeneModelList::iterator it = suspect_aligns.begin(); score == BadScore() && it != suspect_aligns.end(); ) {
        if ((it->Type() & (CGeneModel::eWall | CGeneModel::eNested))!=0 || it->GoodEnoughToBeAnnotation()) {
            ++it;
            continue;
        }
        cout << "Deleting alignment " << it->ID() << endl;
        it->Status() |= CGeneModel::eSkipped;
        it->AddComment("Bad score prediction in combination");
        bad_aligns.push_back(*it);
        it = suspect_aligns.erase(it);
        
        cout << "Testing fragment " << left << ' ' << right << endl;
        score = gnomon.Run(suspect_aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp);
    }
    return score;
}

void CGnomonAnnotator::Predict(CConstRef<CHMMParameters> hmm_params, const CResidueVec& seq, TSignedSeqPos llimit, TSignedSeqPos rlimit, TGeneModelList::const_iterator il, TGeneModelList::const_iterator ir, TGeneModelList& models,
             int window, int margin, bool leftmostwall, bool rightmostwall, bool leftmostanchor, bool rightmostanchor, double mpp, double nonconsensp, TGeneModelList& bad_aligns)
{
    TGeneModelList aligns(il, ir);

    TSignedSeqPos left = llimit;
    bool leftwall = leftmostwall;
    bool leftanchor = leftmostanchor;

    TSignedSeqPos right = llimit+window;
    bool rightwall = false;
    bool rightanchor = false;

    TSignedSeqPos prev_bad_right = rlimit+1;
    bool do_it_again = false;
        
    CGnomonEngine gnomon(hmm_params, seq, TSignedSeqRange(left, right));

    RemoveShortHolesAndRescore(aligns, gnomon);

    TGeneModelList suspect_aligns;
    TSignedSeqRange tested_range;

    do {
        while( right <= rlimit ) {
            bool close = false;
            ITERATE(TGeneModelList, it_c, aligns) {
                if ((it_c->Type() & CGeneModel::eWall)==0 &&
                    it_c->MaxCdsLimits().GetFrom()-margin < right && right < it_c->MaxCdsLimits().GetTo()+margin)  {
                    close = true;
                    right = it_c->MaxCdsLimits().GetTo()+margin;
                    break;
                }
            }
            if(!close) break;
        }
            
        if (right + (right-left)/2 >= rlimit) {
            right = rlimit;
            rightwall = rightmostwall;
            rightanchor = rightmostanchor;
        } else {
            rightwall = false;
            rightanchor = false;
        }

        if (do_it_again)
            rightwall = true;

        double score = BadScore(); 

        if (right < prev_bad_right) {
            suspect_aligns.clear();

            gnomon.ResetRange(left,right);

            cout << left << ' ' << right << ' ' << gnomon.GetGCcontent() << endl;
        
            score = gnomon.Run(aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp);
        
            if(score == BadScore()) {
                cout << "Inconsistent alignments in fragment " << left << ' ' << right << '\n';

                score = TryWithoutObviouslyBadAlignments(aligns, suspect_aligns, bad_aligns,
                                                         gnomon, leftwall, rightwall, leftanchor, rightanchor,
                                                         left, right, mpp, nonconsensp, tested_range);
            }

            if(score == BadScore()) {
        
                prev_bad_right = right;
                right = (left+right)/2;
                    
                continue;
            }
        } else {
            suspect_aligns.sort(s_AlignScoreOrder);

            score = TryToEliminateOneAlignment(suspect_aligns, bad_aligns,
                                               gnomon, leftwall, rightwall, leftanchor, rightanchor,
                                               mpp, nonconsensp);
            if (score == BadScore())
                score = TryToEliminateAlignmentsFromTail(suspect_aligns, bad_aligns,
                                                         gnomon, leftwall, rightwall, leftanchor, rightanchor,
                                                         mpp, nonconsensp);
            if(score == BadScore()) {
                cout << "!!! BAD SCORE EVEN WITH FINISHED ALIGNMENTS !!! " << endl;
                ITERATE(TGeneModelList, it, suspect_aligns) {
                    if ((it->Type() & (CGeneModel::eWall | CGeneModel::eNested))==0 && it->GoodEnoughToBeAnnotation())
                       models.push_back(*it);
                }
            }                
        }
        prev_bad_right = rlimit+1;
        
        list<CGeneModel> genes = gnomon.GetGenes();
        
        TSignedSeqPos partial_start = right;
 
        if (right < rlimit && !genes.empty() && !genes.back().RightComplete() && !do_it_again) {
            partial_start = genes.back().LeftComplete() ? genes.back().RealCdsLimits().GetFrom() : left;
            _ASSERT ( partial_start < right );
            genes.pop_back();
        }

        do_it_again = false;

        if (!genes.empty()) {
            left = genes.back().ReadingFrame().GetTo()+1;
            leftanchor = true;
        } else if (partial_start < left+1000) {
            do_it_again=true;
        } else if (partial_start < right) {
            left = partial_start-100;
            leftanchor = false;
        } else {
            left = (left+right)/2+1;
            leftanchor = false;
        }

        models.splice(models.end(), genes);

        if (right >= rlimit)
            break;

        if (!do_it_again)
            leftwall = true;

        right = left + window;
            
    } while(left <= rlimit);
}

TSignedSeqRange WalledCdsLimits(const CGeneModel& a)
{
    return ((a.Type() & CGeneModel::eWall)!=0) ? a.Limits() : a.MaxCdsLimits();
}

bool s_AlignSeqOrder(const CGeneModel& ap, const CGeneModel& bp)
{
    return (ap.MaxCdsLimits().GetFrom() != bp.MaxCdsLimits().GetFrom() ? 
            ap.MaxCdsLimits().GetFrom() < bp.MaxCdsLimits().GetFrom() :
            ap.MaxCdsLimits().GetTo() > bp.MaxCdsLimits().GetTo()
            );
}

void CGnomonAnnotator::Predict(CConstRef<CHMMParameters> hmm_params, const CResidueVec& seq, TGeneModelList& models,
             int window, int margin, bool wall, double mpp, double nonconsensp, TGeneModelList& bad_aligns)
{

    models.sort(s_AlignSeqOrder);

    TGeneModelList aligns;
    TSignedSeqPos right = -1;
    auto_ptr<CGeneModel> wall_model;

    for (TGeneModelList::iterator ir = models.begin(); ir != models.end();) {

        TSignedSeqRange limits = ir->RealCdsLimits().Empty() ? ir->Limits() : ir->RealCdsLimits();

        if ( right < limits.GetFrom() ) { // new cluster
            if (wall_model.get() != 0) {
                aligns.push_back(*wall_model);
                wall_model.reset();
            }
            if (ir->GeneID() != 0) {
                wall_model.reset( new CGeneModel(ir->Strand(),ir->ID(),CGeneModel::eWall+CGeneModel::eGnomon));
                wall_model->AddExon(limits);
            }
        } else { // same cluster
            _ASSERT( ir->GeneID() != 0 );
            if (wall_model.get() == 0) {
                wall_model.reset( new CGeneModel(ir->Strand(),ir->ID(),CGeneModel::eNested+CGeneModel::eGnomon));
                wall_model->AddExon(limits);
            }
        }
        
        right = max(right, limits.GetTo());
        TGeneModelList::iterator next = ir; ++next;
        if (ir->GeneID() == 0) {
            aligns.splice(aligns.end(), models, ir);
        } else {
            wall_model->ExtendRight(limits.GetTo()- wall_model->Limits().GetTo());
        }
        ir = next;
    }
    if (wall_model.get() != 0) {
        aligns.push_back(*wall_model);
        wall_model.reset();
    }

    TGeneModelList::const_iterator il = aligns.begin();
    TSignedSeqPos left = 0;

    /*
    ITERATE(TGeneModelList, ir, aligns) {
        if ((ir->Type() & CGeneModel::eWall)!=0) {
            TGeneModelList::const_iterator curwall = ir;
            TGeneModelList::const_iterator next = ir;
            Predict(seq, left, WalledCdsLimits(*ir).GetFrom(), il, ++next, models, window, margin,
                    (left!=0 || wall), true, left!=0, true, mpp, nonconsensp, bad_aligns);

            for( ; next != aligns.end() && WalledCdsLimits(*next).GetFrom() <= WalledCdsLimits(*curwall).GetTo(); ++next) {
                _ASSERT((next->Type() & CGeneModel::eNested)!=0 || next->Strand() != curwall->Strand());
                ir = next;
                if(WalledCdsLimits(*next).GetTo() > WalledCdsLimits(*curwall).GetTo()) {
                    cout << "Overlapping genes: " << WalledCdsLimits(*curwall).GetFrom() << ' ' << WalledCdsLimits(*next).GetTo() << endl;
                    curwall = next;
                }
            }

            il = curwall;
            left = WalledCdsLimits(*curwall).GetTo();
        }
    }
    */

    Predict(hmm_params, seq, left, TSignedSeqPos(seq.size()-1), il, aligns.end(), models, window, margin,
            (left!=0 || wall), wall, left!=0, false, mpp, nonconsensp, bad_aligns);

    NON_CONST_ITERATE(TGeneModelList, it, models) {
#ifdef _DEBUG
        {
            string protein = it->GetProtein(seq);
            int nstar = 0;
            ITERATE(string, is, protein) {
                if(*is == '*')
                    ++nstar;
            }
            int nstop = it->HasStop() ? 1 : 0;
            nstop += it->GetCdsInfo().PStops().size();
            _ASSERT(nstar == nstop);
        }
#endif
        if (it->PStop() || !it->FrameShifts().empty()) {
            it->Status() |= CGeneModel::ePseudo;
        }
        if(it->OpenCds()) {
            CCDSInfo cds_info = it->GetCdsInfo();
            cds_info.SetScore(cds_info.Score(),false);     // kill the Open flag
            it->SetCdsInfo(cds_info);
        }
    }
}
    
END_SCOPE(gnomon)
END_NCBI_SCOPE

