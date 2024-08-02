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
 * gnomon is run to improve chains and predict models in regions w/o chains
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>

#include <algo/gnomon/annot.hpp>
#include "gnomon_engine.hpp"
#include <algo/gnomon/gnomon_model.hpp>
#include <algo/gnomon/gnomon.hpp>
#include <algo/gnomon/id_handler.hpp>

#include <objects/seqloc/Seq_loc.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(gnomon)

CGeneSelector::CGeneSelector()
{
}

CGnomonAnnotator::CGnomonAnnotator()
{
}

CGnomonAnnotator::~CGnomonAnnotator()
{
}

bool s_AlignScoreOrder(const CGeneModel& ap, const CGeneModel& bp)
{
    return (ap.Score() < bp.Score());
}

void CGnomonAnnotator::RemoveShortHolesAndRescore(TGeneModelList chains)
{
    NON_CONST_ITERATE(TGeneModelList, it, chains) {
        it->RemoveShortHolesAndRescore(*m_gnomon);
    }
}

double CGnomonAnnotator::ExtendJustThisChain(CGeneModel& chain,
                                             TSignedSeqPos left, TSignedSeqPos right)
{
    TGeneModelList test_align;
    test_align.push_back(chain);
    int l = max((int)left,(int)chain.Limits().GetFrom()-10000);
    int r = min(right,chain.Limits().GetTo()+10000);
    cerr << "Testing alignment " << chain.ID() << " in fragment " << l << ' ' << r << endl;
                    
    m_gnomon->ResetRange(l,r);
    return m_gnomon->Run(test_align, false, false, false, false, mpp, nonconsensp, m_notbridgeable_gaps_len, m_inserted_seqs, m_pcsf_slice.get());
}

double CGnomonAnnotator::TryWithoutObviouslyBadAlignments(TGeneModelList& aligns, TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                                          bool leftwall, bool rightwall, bool leftanchor, bool rightanchor,
                                                          TSignedSeqPos left, TSignedSeqPos right,
                                                          TSignedSeqRange& tested_range)
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
                ExtendJustThisChain(*it, left, right) == BadScore()) {
                found_bad_cluster = true;
                cerr << "Deleting alignment " << it->ID() << endl;
                it->Status() |= CGeneModel::eSkipped;
                it->AddComment("Bad score prediction alone");
                bad_aligns.push_back(*it);
                
                it = aligns.erase(it);
                continue;
            }
            suspect_aligns.push_back(*it++);
        }
        
        m_gnomon->ResetRange(left, right);
        if(found_bad_cluster) {
            cerr << "Testing w/o bad alignments in fragment " << left << ' ' << right << endl;
            return m_gnomon->Run(suspect_aligns, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp, m_notbridgeable_gaps_len, m_inserted_seqs, m_pcsf_slice.get());
        }
    }
    return BadScore();
}

double CGnomonAnnotator::TryToEliminateOneAlignment(TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                      bool leftwall, bool rightwall, bool leftanchor, bool rightanchor)
{
    double score = BadScore();
    for(TGeneModelList::iterator it = suspect_aligns.begin(); it != suspect_aligns.end();) {
        if((it->Type() & (CGeneModel::eWall | CGeneModel::eNested))!=0) {
            ++it;
            continue;
        }
        CGeneModel algn = *it;
        it = suspect_aligns.erase(it);
        
        cerr << "Testing w/o " << algn.ID();
        score = m_gnomon->Run(suspect_aligns, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp, m_notbridgeable_gaps_len, m_inserted_seqs, m_pcsf_slice.get());
        if (score != BadScore()) {
            cerr << "- Good. Deleting alignment " << algn.ID() << endl;
            algn.Status() |= CGeneModel::eSkipped;
            algn.AddComment("Good score prediction without");
            bad_aligns.push_back(algn);
            break;
        } else {
            cerr << " - Still bad." << endl;                        
        }
        suspect_aligns.insert(it,algn);
    }
    return score;
}

double CGnomonAnnotator::TryToEliminateAlignmentsFromTail(TGeneModelList& suspect_aligns, TGeneModelList& bad_aligns,
                                      bool leftwall, bool rightwall, bool leftanchor, bool rightanchor)
{
    double score = BadScore();
    for (TGeneModelList::iterator it = suspect_aligns.begin(); score == BadScore() && it != suspect_aligns.end(); ) {
        if ((it->Type() & (CGeneModel::eWall | CGeneModel::eNested))!=0 || it->GoodEnoughToBeAnnotation()) {
            ++it;
            continue;
        }
        cerr << "Deleting alignment " << it->ID() << endl;
        it->Status() |= CGeneModel::eSkipped;
        it->AddComment("Bad score prediction in combination");
        bad_aligns.push_back(*it);
        it = suspect_aligns.erase(it);
        
        cerr << "Testing fragment " << left << ' ' << right << endl;
        score = m_gnomon->Run(suspect_aligns, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp, m_notbridgeable_gaps_len, m_inserted_seqs, m_pcsf_slice.get());
    }
    return score;
}

void CGnomonAnnotator::Predict(TSignedSeqPos llimit, TSignedSeqPos rlimit, TGeneModelList::const_iterator il, TGeneModelList::const_iterator ir, TGeneModelList& models,
             bool leftmostwall, bool rightmostwall, bool leftmostanchor, bool rightmostanchor, TGeneModelList& bad_aligns)
{
    TGeneModelList aligns(il, ir);

    //    TSignedSeqPos left = llimit;
    int64_t left = llimit;
    bool leftwall = leftmostwall;
    bool leftanchor = leftmostanchor;

    //    TSignedSeqPos right = llimit+window;
    int64_t right = llimit+window;
    bool rightwall = false;
    bool rightanchor = false;

    Int8 prev_bad_right = rlimit+1;
    bool do_it_again = false;
        
    m_gnomon->ResetRange(static_cast<TSignedSeqPos>(left), static_cast<TSignedSeqPos>(right));

    RemoveShortHolesAndRescore(aligns);

    TGeneModelList suspect_aligns;
    TSignedSeqRange tested_range;

    TIVec busy_spots(rlimit+1,0);
    ITERATE(TGeneModelList, it_c, aligns) {
        int a = max(0,it_c->Limits().GetFrom()-margin);
        int b = min(rlimit,it_c->Limits().GetTo()+margin);
        for(int i = a; i<=b; ++i)
            busy_spots[i] = 1;
    }

    do {
        for( ; right < rlimit && busy_spots[right] != 0; ++right);
            
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

            m_gnomon->ResetRange(static_cast<TSignedSeqPos>(left),static_cast<TSignedSeqPos>(right));

            cerr << left << ' ' << right << ' ' << m_gnomon->GetGCcontent() << endl;
        
            score = m_gnomon->Run(aligns, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp, m_notbridgeable_gaps_len, m_inserted_seqs, m_pcsf_slice.get());
        
            if(score == BadScore()) {
                cerr << "Inconsistent alignments in fragment " << left << ' ' << right << '\n';

                score = TryWithoutObviouslyBadAlignments(aligns, suspect_aligns, bad_aligns,
                                                         leftwall, rightwall, leftanchor, rightanchor,
                                                         static_cast<TSignedSeqPos>(left), static_cast<TSignedSeqPos>(right), tested_range);
            }

            if(score == BadScore()) {
        
                prev_bad_right = right;
                right = (left+right)/2;
                    
                continue;
            }
        } else {
            suspect_aligns.sort(s_AlignScoreOrder);

            score = TryToEliminateOneAlignment(suspect_aligns, bad_aligns,
                                               leftwall, rightwall, leftanchor, rightanchor);
            if (score == BadScore())
                score = TryToEliminateAlignmentsFromTail(suspect_aligns, bad_aligns,
                                                         leftwall, rightwall, leftanchor, rightanchor);
            if(score == BadScore()) {
                cerr << "!!! BAD SCORE EVEN WITH FINISHED ALIGNMENTS !!! " << endl;
                ITERATE(TGeneModelList, it, suspect_aligns) {
                    if ((it->Type() & (CGeneModel::eWall | CGeneModel::eNested))==0 && it->GoodEnoughToBeAnnotation())
                       models.push_back(*it);
                }
            }                
        }
        prev_bad_right = rlimit+1;
        
        list<CGeneModel> genes = m_gnomon->GetGenes();
        
        TSignedSeqPos partial_start = static_cast<TSignedSeqPos>(right);
 
        if (right < rlimit && !genes.empty() && !genes.back().RightComplete() && !do_it_again) {
            partial_start = genes.back().LeftComplete() ? genes.back().RealCdsLimits().GetFrom() : static_cast<TSignedSeqPos>(left);
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
            int new_left = partial_start-100; 
            for( ; new_left > left && busy_spots[new_left] != 0; --new_left);
            if(new_left > left+1000) {
                left = new_left;
                leftanchor = false;
            } else {
                do_it_again=true;
            }
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

TSignedSeqRange GetWallLimits(const CGeneModel& m, bool external = false)
{
    TSignedSeqRange model_lim_for_nested = m.Limits();
    if(m.ReadingFrame().NotEmpty()) {
        if(external)
            model_lim_for_nested = m.OpenCds() ? m.MaxCdsLimits() : m.RealCdsLimits();                // open models can harbor in 5' introns; they are not alternative varinats    
        else
            model_lim_for_nested = m.RealCdsLimits();
    }

    return  model_lim_for_nested;
}
pair<TSignedSeqRange, bool> GetGeneWallLimits(const list<TGeneModelList::iterator>& models, bool external = false)
{
    bool coding_gene = false;
    for(auto im : models) {
        if(im->ReadingFrame().NotEmpty()) {
            coding_gene = true;
            break;
        }
    }

    TSignedSeqRange gene_lim;
    for(auto im : models) {
        if(coding_gene && im->ReadingFrame().Empty())
            continue;
        gene_lim += GetWallLimits(*im, external);
    }

    return make_pair(gene_lim, coding_gene);
}

bool s_AlignSeqOrder(const CGeneModel& ap, const CGeneModel& bp)
{
    TSignedSeqRange a = GetWallLimits(ap);
    TSignedSeqRange b = GetWallLimits(bp);

    return (a.GetFrom() != b.GetFrom() ? 
            a.GetFrom() < b.GetFrom() :
            a.GetTo() > b.GetTo()
            );
}

void FindPartials(TGeneModelList& models, TGeneModelList& aligns, EStrand strand)
{
    for (TGeneModelList::iterator loop_it = models.begin(); loop_it != models.end();) {
        TGeneModelList::iterator ir = loop_it;
        ++loop_it;

        if(ir->Strand() != strand)
            continue;

        if(ir->Type()&CGeneModel::eNested) {
            CGeneModel wall_model(ir->Strand(),ir->ID(), CGeneModel::eNested);
            wall_model.SetGeneID(ir->GeneID());
            TSignedSeqRange limits = GetWallLimits(*ir);
            wall_model.AddExon(limits);
            aligns.push_back(wall_model);           
        } else if(ir->GoodEnoughToBeAnnotation()) {
            CGeneModel wall_model(ir->Strand(),ir->ID(), CGeneModel::eWall);
            wall_model.SetGeneID(ir->GeneID());
            TSignedSeqRange limits = GetWallLimits(*ir);
            wall_model.AddExon(limits);
            aligns.push_back(wall_model);           
        } else if(ir->RankInGene() == 1) {
            ir->Status() &= ~CGeneModel::eFullSupCDS;
            aligns.splice(aligns.end(), models, ir);
        }        
    }
}

void CGnomonAnnotator::Predict(TGeneModelList& models, TGeneModelList& bad_aligns)
{
    if (models.empty() && int(m_gnomon->GetSeq().size()) < mincontig)
        return;

    if (GnomonNeeded()) {
        typedef list<TGeneModelList::iterator> TIterList;
        typedef map<Int8,TIterList> TGIDIterlist;
        typedef TGIDIterlist::iterator TGIter;
        struct geneid_order {
            bool operator()(TGIter a, TGIter b) const { return a->second.front()->GeneID() < b->second.front()->GeneID(); }
        };
        typedef tuple<TSignedSeqRange, bool, TGIter> TGenomeRange;  // range, is hole, gene iterator
        struct grange_order {
            bool operator()(const TGenomeRange& a, TGenomeRange& b) const {
                if(get<0>(a) != get<0>(b))
                    return get<0>(a) <  get<0>(b);
                else if(get<1>(a) != get<1>(b))
                    return get<1>(a) <  get<1>(b);
                else
                    return geneid_order()(get<2>(a), get<2>(b));
            }
        };
        struct interval_order {
            bool operator()(const TSignedSeqRange& a, const TSignedSeqRange& b) const { return a.GetTo() < b.GetFrom(); }
        };
        struct GenomeRangeMap : public map<TSignedSeqRange, list<TGenomeRange>, interval_order> { // map argument is the range for overlapping 'introns' - could be used for any kind of ranges
            void Insert(const TGenomeRange& intron) {                                             // combine ranges overlapping with intron and insert
                list<TGenomeRange> clust(1, intron);
                TSignedSeqRange range(get<0>(intron));
                TSignedSeqRange intron_left(range.GetFrom(), range.GetFrom());
                for(auto it = lower_bound(intron_left); it != end() && it->first.IntersectingWith(range); ) {
                    range.CombineWith(it->first);
                    clust.splice(clust.end(), it->second);
                    it = erase(it);
                }
                emplace(range, clust);
            }
            void Unique() {                                                                       // remove duplicates from the lists
                for(auto& range_intronlist : *this) {
                    auto& lst = range_intronlist.second;
                    lst.sort(grange_order());
                    lst.unique();
                }
            }
        };

        TGIDIterlist genes;
        NON_CONST_ITERATE(TGeneModelList, im, models) {
            if(im->Type()&CGeneModel::eNested)
                im->SetType(im->Type()-CGeneModel::eNested);  // ignore flag set in chainer    
            genes[im->GeneID()].push_back(im);
        }

        GenomeRangeMap introns;
        for(auto ig = genes.begin(); ig != genes.end(); ++ig) {
            auto rslt = GetGeneWallLimits(ig->second, true);
            TSignedSeqRange lim_for_nested = rslt.first;
            bool coding = rslt.second;
            for(auto im : ig->second) {
                CGeneModel& m = *im;
                if(coding && m.RealCdsLimits().Empty())  // ignore nocoding variants in coding genes
                    continue;
                for(int i = 1; i < (int)m.Exons().size(); ++i) {
                    if(m.Exons()[i-1].m_ssplice_sig == "XX" || m.Exons()[i].m_fsplice_sig == "XX")            // skip genomic gaps
                        continue;
                    TSignedSeqRange range(m.Exons()[i-1].Limits().GetTo(), m.Exons()[i].Limits().GetFrom());
                    if(Include(lim_for_nested, range)) {
                        bool is_hole = !m.Exons()[i-1].m_ssplice || !m.Exons()[i].m_fsplice;
                        TGenomeRange intron(range, is_hole, ig);
                        introns.Insert(intron);
                    }
                }
            }
        }
        introns.Unique(); // remove duplicate introns

        list<TGIter> genes_hosting_partial;
        list<TGIter> nested_partial;
        GenomeRangeMap finished_intervals;
        if(!introns.empty()) {
            list<TGIter> genes_to_remove;
            for(auto ig = genes.begin(); ig != genes.end(); ++ig) {
                TIterList& modelsi = ig->second;
                auto gfront = modelsi.front();
                TSignedSeqRange lim_for_nested = GetGeneWallLimits(modelsi).first;
                auto iclust = introns.lower_bound(TSignedSeqRange(lim_for_nested.GetFrom(), lim_for_nested.GetFrom())); // first not to the left
                if(iclust != introns.end() && Include(iclust->first, lim_for_nested)) {
                    for(TGenomeRange& intron : iclust->second) {
                        TSignedSeqRange& range = get<0>(intron);
                        if(Include(range,  lim_for_nested)) {
                            bool is_hole = get<1>(intron);
                            auto host_it = get<2>(intron);
                            if(is_hole && !gfront->GoodEnoughToBeAnnotation()) {         // partial gene in a hole - one gene will be removed
                                if(host_it->second.front()->Score() > gfront->Score())   // no variants in both - using front()
                                    genes_to_remove.push_back(ig);
                                else
                                    genes_to_remove.push_back(host_it);
                            } else {
                                if(gfront->GoodEnoughToBeAnnotation()) {                 // complete gene nested in an intron/hole - assign nested flag and calculate finished interval
                                    for(auto im : modelsi)
                                        im->SetType(im->Type()|CGeneModel::eNested);
                                } else {
                                    genes_hosting_partial.push_back(host_it);
                                    nested_partial.push_back(ig);
                                }
                            }
                        }
                    }
                }

                if(gfront->GoodEnoughToBeAnnotation()) { // find finished intervals which may limit the extension of nested
                    bool found = false;
                    for( ; !found && iclust != introns.end() && iclust->first.IntersectingWith(lim_for_nested); ++iclust) {
                        for(TGenomeRange& intron : iclust->second) {
                            if(get<2>(intron) == ig)
                                continue;
                            TSignedSeqRange& range = get<0>(intron);
                            if(range.IntersectingWith(lim_for_nested) && !Include(lim_for_nested, range)) {
                                found = true;
                                TGenomeRange finished_interval(lim_for_nested, false, ig);
                                finished_intervals.Insert(finished_interval);
                                break;
                            }
                        }
                    }
                }
            }
            genes_to_remove.sort(geneid_order());
            genes_to_remove.unique();
            genes_hosting_partial.sort(geneid_order());
            genes_hosting_partial.unique();
            nested_partial.sort(geneid_order());
            nested_partial.unique();
            for(auto it : genes_to_remove) {
                genes_hosting_partial.remove(it);
                nested_partial.remove(it);
                for(auto im : it->second) {
                    im->Status() |= CGeneModel::eSkipped;
                    im->AddComment("Partial gene in a hole");
                    bad_aligns.push_back(*im);
                    models.erase(im);
                }
                genes.erase(it);
            }
        }
        
        //extend partial nested
        GenomeRangeMap hosting_intervals;
        for(auto it : genes_hosting_partial) {
            TIterList& lst = it->second;

            bool coding_gene = find_if(lst.begin(), lst.end(), [](TGeneModelList::iterator im){ return im->ReadingFrame().NotEmpty(); }) != lst.end();
            // if external model is 'open' all 5' introns can harbor
            // for nested model 'open' is ignored
            TSignedSeqRange gene_lim_for_nested;
            for(auto im : lst) {
                const CGeneModel& ai = *im;
                if(coding_gene && ai.ReadingFrame().Empty())
                    continue;
                TSignedSeqRange model_lim_for_nested = ai.Limits();
                if(ai.ReadingFrame().NotEmpty())
                    model_lim_for_nested = ai.OpenCds() ? ai.MaxCdsLimits() : ai.RealCdsLimits();   // 'open' could be only a single variant gene   
                gene_lim_for_nested += model_lim_for_nested;
            }

            vector<int> grange(gene_lim_for_nested.GetLength(),1);
            for(auto im : lst) { // exclude all positions included in CDS (any exons for not coding genes) and holes  
                const CGeneModel& ai = *im;
                if(coding_gene && ai.ReadingFrame().Empty())
                    continue;

                TSignedSeqRange model_lim_for_nested = ai.Limits();
                if(ai.ReadingFrame().NotEmpty())
                    model_lim_for_nested = ai.OpenCds() ? ai.MaxCdsLimits() : ai.RealCdsLimits();   // 'open' could be only a single variant gene   

                for(int i = 0; i < (int)ai.Exons().size(); ++i) {
                    TSignedSeqRange overlap = (model_lim_for_nested & ai.Exons()[i].Limits());
                    for(int j = overlap.GetFrom(); j <= overlap.GetTo(); ++j)
                        grange[j-gene_lim_for_nested.GetFrom()] = 0;
                }

                for(int i = 1; i < (int)ai.Exons().size(); ++i) {
                    if(!ai.Exons()[i-1].m_ssplice || !ai.Exons()[i].m_fsplice) {
                        TSignedSeqRange hole(ai.Exons()[i-1].Limits().GetTo()+1,ai.Exons()[i].Limits().GetFrom()-1);
                        _ASSERT(Include(model_lim_for_nested, hole));
                        for(int j = hole.GetFrom(); j <= hole.GetTo(); ++j)
                            grange[j-gene_lim_for_nested.GetFrom()] = 0;
                    }
                }
            }
            _ASSERT(grange.front() == 0 && grange.back() == 0);

            int left = -1;
            int right;
            for(int j = 0; j < (int)grange.size(); ++j) {
                if(left < 0) {
                    if(grange[j] == 1) {
                        left = j;
                        right = j;
                    }
                } else if(grange[j] == 1) {
                    right = j;
                } else {
                    TSignedSeqRange interval(left+gene_lim_for_nested.GetFrom(),right+gene_lim_for_nested.GetFrom());
                    TGenomeRange hosting_interval(interval, false, it);
                    hosting_intervals.Insert(hosting_interval);
                    left = -1; 
                }
            }
        }
        
        typedef map<TSignedSeqRange,TIterList> TRangeModels;
        TRangeModels nested_models;
        for(auto ig : nested_partial) {
            TGeneModelList::iterator nested_modeli = ig->second.front();                      
            _ASSERT(ig->second.size() == 1);
            TSignedSeqRange lim_for_nested = GetWallLimits(*nested_modeli);
            TSignedSeqRange hosting_interval;
            {
                auto rslt = hosting_intervals.lower_bound(TSignedSeqRange(lim_for_nested.GetFrom(), lim_for_nested.GetFrom()));
                if(rslt != hosting_intervals.end() && Include(rslt->first, lim_for_nested)) {
                    for(auto& grange : rslt->second) {
                        TSignedSeqRange& interval = get<0>(grange);
                        if(Include(interval,lim_for_nested)) {
                            if(hosting_interval.Empty())
                                hosting_interval = interval;
                            else
                                hosting_interval = (hosting_interval&interval);
                        }
                    }
                }
            }
            
            if(hosting_interval.NotEmpty()) {
                TIterList nested(1,nested_modeli);
                TSignedSeqRange left(hosting_interval.GetFrom(), hosting_interval.GetFrom());
                for(auto it = finished_intervals.lower_bound(left); it != finished_intervals.end() && it->first.IntersectingWith(hosting_interval); ++it) {
                    for(auto& grange : it->second) {
                        TSignedSeqRange& finished_interval = get<0>(grange);
                        if(!finished_interval.IntersectingWith(hosting_interval) || Include(finished_interval, hosting_interval))
                            continue;

                        if(Precede(finished_interval,lim_for_nested)) {           // before partial model
                            hosting_interval.SetFrom(finished_interval.GetTo());
                        } else if(Precede(lim_for_nested,finished_interval)) {    // after partial model
                            hosting_interval.SetTo(finished_interval.GetFrom());
                        } else if(CModelCompare::RangeNestedInIntron(finished_interval, *nested_modeli, true)) {
                            for(auto im : get<2>(grange)->second)
                                nested.push_back(im);                            
                        }
                    }
                }
                _ASSERT(hosting_interval.NotEmpty());
                nested_models[hosting_interval].splice(nested_models[hosting_interval].begin(), nested);
            }
            
        }        

        bool scaffold_wall = wall;
        wall = true;
        ITERATE(TRangeModels, i, nested_models) {
            TSignedSeqRange hosting_interval = i->first;
            
            TGeneModelList nested;
            set<Int8> included_complete_models;
            ITERATE(TIterList, im, i->second) {               
                nested.push_back(**im);

                if(!(*im)->GoodEnoughToBeAnnotation()) {
                    if(nested.back().HasStart() && !Include(hosting_interval,nested.back().MaxCdsLimits())) {
                        CCDSInfo cds = nested.back().GetCdsInfo();
                        if(nested.back().Strand() == ePlus)
                            cds.Set5PrimeCdsLimit(cds.Start().GetFrom());
                        else
                            cds.Set5PrimeCdsLimit(cds.Start().GetTo());
                        nested.back().SetCdsInfo(cds);
                    }
                    nested.back().AddComment("partialnested");
                    models.erase(*im);
                } else {
                    included_complete_models.insert((*im)->ID()); 
                }
            }

            cerr << "Interval " << hosting_interval << '\t' << nested.size() << endl;

            Predict(nested, bad_aligns, hosting_interval.GetFrom()+1,hosting_interval.GetTo()-1);

            NON_CONST_ITERATE(TGeneModelList, im, nested) {
                if(!im->Support().empty()) {
                    im->SetType(im->Type()|CGeneModel::eNested);
                    if(im->ID() == 0 || included_complete_models.find(im->ID()) == included_complete_models.end())  // include only models which we tried to extend
                        models.push_back(*im);
                }
            }
        }
        wall = scaffold_wall;
    }
    //at this point all nested models don't need ab initio any more

    Predict(models, bad_aligns, 0, TSignedSeqPos(m_gnomon->GetSeq().size())-1);

    ERASE_ITERATE(TGeneModelList, im, models) {
        CGeneModel& model = *im;
        TSignedSeqRange cds = model.RealCdsLimits();
        if(cds.Empty())
            continue;

        bool gapfilled = false;
        int genome_cds = 0;
        ITERATE(CGeneModel::TExons, ie, model.Exons()) {
            if(ie->m_fsplice_sig == "XX" || ie->m_ssplice_sig == "XX")
                gapfilled = true;
            else
                genome_cds += (cds&ie->Limits()).GetLength();
        }
        
        if(gapfilled && genome_cds < 45) {
            model.Status() |= CGeneModel::eSkipped;
            model.AddComment("Most CDS in genomic gap");
            bad_aligns.push_back(model);
            models.erase(im);
        }
    }
}

void CGnomonAnnotator::Predict(TGeneModelList& models, TGeneModelList& bad_aligns, TSignedSeqPos left, TSignedSeqPos right)
{
    if (GnomonNeeded()) {

        models.sort(s_AlignSeqOrder);

        if(!models.empty()) {
            for(auto it_loop = next(models.begin()); it_loop != models.end(); ) {
                auto it = it_loop++;
                if(it->RankInGene() != 1 || it->GoodEnoughToBeAnnotation() || it->Type()&CGeneModel::eNested)
                    continue;
                auto it_prev = prev(it);
                if(it_prev->RankInGene() != 1 || it_prev->GoodEnoughToBeAnnotation() || it_prev->Type()&CGeneModel::eNested)
                    continue;
                
                if(it->MaxCdsLimits().IntersectingWith(it_prev->MaxCdsLimits())) {
                    cerr << "Intersecting alignments " << it->ID() << " " << it_prev->ID() << " " << it->Score() << " " << it_prev->Score() << endl;
                    auto it_erase = (it->Score() < it_prev->Score()) ? it : it_prev;
                    it_erase->Status() |= CGeneModel::eSkipped;
                    it_erase->AddComment("Intersects with other partial");
                    bad_aligns.push_back(*it_erase);
                    models.erase(it_erase);
                }
            }
        }

        TGeneModelList aligns;

        FindPartials(models, aligns, ePlus);
        FindPartials(models, aligns, eMinus);

        aligns.sort(s_AlignSeqOrder);

        TGeneModelList models_tmp;
        Predict(left, right, aligns.begin(), aligns.end(), models_tmp,(left!=0 || wall), wall, left!=0, false, bad_aligns);
        ITERATE(TGeneModelList, it, models_tmp) {
            if(!it->Support().empty() || it->RealCdsLen() >= minCdsLen)
                models.push_back(*it);
        }
    }

    NON_CONST_ITERATE(TGeneModelList, it, models) {
        CCDSInfo cds_info = it->GetCdsInfo();

        // removing fshifts in UTRs  
        TInDels fs;
        TSignedSeqRange fullcds = cds_info.Cds();
        ITERATE(TInDels, i, it->FrameShifts()) {
            if(((i->IsInsertion() || i->IsMismatch()) && Include(fullcds,i->Loc())) ||
               (i->IsDeletion() && i->Loc() > fullcds.GetFrom() && i->Loc() <= fullcds.GetTo())) {
                fs.push_back(*i);
            }
        }
        it->FrameShifts() = fs;

        // removing pstops in UTRs
        CCDSInfo::TPStops pstops = cds_info.PStops();
        cds_info.ClearPStops();
        ITERATE(CCDSInfo::TPStops, ps, pstops) {
            if(Include(fullcds,*ps))
                cds_info.AddPStop(*ps);
        }
        it->SetCdsInfo(cds_info);

        if (it->PStop(false) || !it->FrameShifts().empty()) {
            it->Status() |= CGeneModel::ePseudo;
        }
        if(it->OpenCds()) {
            CCDSInfo cds_info = it->GetCdsInfo();
            cds_info.SetScore(cds_info.Score(),false);     // kill the Open flag
            it->SetCdsInfo(cds_info);
        }
    }
}

RemoveTrailingNs::RemoveTrailingNs(const CResidueVec& _seq)
    : seq(_seq)
{
}

void RemoveTrailingNs::transform_model(CGeneModel& m)
{
        CAlignMap mrnamap(m.GetAlignMap());
        CResidueVec vec;
        mrnamap.EditedSequence(seq, vec);

        int five_p, three_p;
        for(five_p=0; five_p < (int)vec.size() && vec[five_p] == 'N'; ++five_p);
        for(three_p=0; three_p < (int)vec.size() && vec[(int)vec.size()-1-three_p] == 'N'; ++three_p);

        if(five_p > 0 || three_p > 0) {
            int left = five_p;
            int right = three_p;
            if(m.Strand() == eMinus)
                swap(left,right);

            TSignedSeqRange new_lim(m.Limits());
            if(left > 0) {
                _ASSERT(m.Exons().front().Limits().GetLength() > left);
                new_lim.SetFrom(new_lim.GetFrom()+left);
            }
            if(right > 0) {
                _ASSERT(m.Exons().back().Limits().GetLength() > right);
                new_lim.SetTo(new_lim.GetTo()-right);
            }

            double score = m.Score();
            m.Clip(new_lim,CAlignModel::eDontRemoveExons);
            CCDSInfo cds_info = m.GetCdsInfo();
            cds_info.SetScore(score, false);
            m.SetCdsInfo(cds_info);
            
            if (m.Type() & (CGeneModel::eChain | CGeneModel::eGnomon)) {
                CAlignModel* a = dynamic_cast<CAlignModel*>(&m);
                if (a != NULL) {
                    a->ResetAlignMap();
                }
            }
        }
}

void CGnomonAnnotatorArgUtil::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->AddKey("param", "param",
                     "Organism specific parameters",
                     CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("pcsf_factor","pcsf_factor","Normalisation factor for phyloPCSF scores",CArgDescriptions::eDouble,"0.1");
    arg_desc->AddFlag("nognomon","Skips ab initio prediction and ab initio extension of partial chains.");
    arg_desc->AddDefaultKey("window","window","Prediction window",CArgDescriptions::eInteger,"200000");
    arg_desc->AddDefaultKey("margin","margin","The minimal distance between chains to place the end of prediction window",CArgDescriptions::eInteger,"1000");
    arg_desc->AddFlag("open","Allow partial predictions at the ends of contigs. Used for poorly assembled genomes with lots of unfinished contigs.");
    arg_desc->AddDefaultKey("mpp","mpp","Penalty for connection two protein containing chains into one model.",CArgDescriptions::eDouble,"10.0");
    arg_desc->AddFlag("nonconsens","Allows to accept nonconsensus splices starts/stops to complete partial alignmet. If not allowed some partial alignments "
                      "may be rejected if there is no way to complete them.");
    arg_desc->AddDefaultKey("ncsp","ncsp","Nonconsensus penalty",CArgDescriptions::eDouble,"25");

    arg_desc->AddFlag("norep","DO NOT mask lower case letters");
    arg_desc->AddDefaultKey("mincont","mincont","Contigs shorter than that will be skipped unless they have alignments.",CArgDescriptions::eInteger,"1000");

    arg_desc->SetCurrentGroup("Prediction tuning");
    arg_desc->AddFlag("singlest","Allow single exon EST chains as evidence");
    arg_desc->AddDefaultKey("minlen","minlen","Minimal CDS length for pure ab initio models",CArgDescriptions::eInteger,"100");
}

void CGnomonAnnotatorArgUtil::ReadArgs(CGnomonAnnotator* annot, const CArgs& args)
{
    CNcbiIfstream param_file(args["param"].AsString().c_str());
    annot->SetHMMParameters(new CHMMParameters(param_file));
    annot->m_pcsf_factor = args["pcsf_factor"].AsDouble();    

    annot->window = args["window"].AsInteger();
    annot->margin = args["margin"].AsInteger();
    annot->wall = !args["open"];
    annot->mpp = args["mpp"].AsDouble();
    bool nonconsens = args["nonconsens"];
    annot->nonconsensp = (nonconsens ? -args["ncsp"].AsDouble() : BadScore());
    annot->do_gnomon = !args["nognomon"];

    annot->mincontig = args["mincont"].AsInteger();

    annot->minCdsLen = args["minlen"].AsInteger();

    if (!args["norep"])
        annot->EnableSeqMasking();
}
END_SCOPE(gnomon)
END_NCBI_SCOPE

