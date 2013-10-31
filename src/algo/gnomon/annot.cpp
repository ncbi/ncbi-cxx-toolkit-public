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
    return m_gnomon->Run(test_align, true, false, false, false, false, mpp, nonconsensp, m_inserted_seqs);
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
            return m_gnomon->Run(suspect_aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp, m_inserted_seqs);
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
        score = m_gnomon->Run(suspect_aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp, m_inserted_seqs);
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
        score = m_gnomon->Run(suspect_aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp, m_inserted_seqs);
    }
    return score;
}

void CGnomonAnnotator::Predict(TSignedSeqPos llimit, TSignedSeqPos rlimit, TGeneModelList::const_iterator il, TGeneModelList::const_iterator ir, TGeneModelList& models,
             bool leftmostwall, bool rightmostwall, bool leftmostanchor, bool rightmostanchor, TGeneModelList& bad_aligns)
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
        
    m_gnomon->ResetRange(left, right);

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

            m_gnomon->ResetRange(left,right);

            cerr << left << ' ' << right << ' ' << m_gnomon->GetGCcontent() << endl;
        
            score = m_gnomon->Run(aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp, m_inserted_seqs);
        
            if(score == BadScore()) {
                cerr << "Inconsistent alignments in fragment " << left << ' ' << right << '\n';

                score = TryWithoutObviouslyBadAlignments(aligns, suspect_aligns, bad_aligns,
                                                         leftwall, rightwall, leftanchor, rightanchor,
                                                         left, right, tested_range);
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

TSignedSeqRange GetWallLimits(const CGeneModel& m)
{
    return m.RealCdsLimits().Empty() ? m.Limits() : m.RealCdsLimits();
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

typedef map<int,CGeneModel> TNestedGenes;

void SaveWallModel(auto_ptr<CGeneModel>& wall_model, TNestedGenes& nested_genes, TGeneModelList& aligns)
{
    if (wall_model.get() != 0 && wall_model->Type() == CGeneModel::eWall+CGeneModel::eGnomon) {
        aligns.push_back(*wall_model);
    }
    ITERATE(TNestedGenes, nested, nested_genes) {
        aligns.push_back(nested->second);
    }
    nested_genes.clear();
}

void FindPartials(TGeneModelList& models, TGeneModelList& aligns, EStrand strand)
{
    TSignedSeqPos right = -1;
    auto_ptr<CGeneModel> wall_model;
    TNestedGenes nested_genes;

    for (TGeneModelList::iterator loop_it = models.begin(); loop_it != models.end();) {
        TGeneModelList::iterator ir = loop_it;
        ++loop_it;

        if (ir->Strand() != strand) {
            continue;
        }
        
        TSignedSeqRange limits = GetWallLimits(*ir);

        if ( right < limits.GetFrom() ) { // new cluster
            SaveWallModel(wall_model, nested_genes, aligns);
        }

        if ((ir->Type() & CGeneModel::eNested) !=0) {
            //            ir->SetType( ir->Type() - CGeneModel::eNested );
            TNestedGenes::iterator nested_wall = nested_genes.find(ir->GeneID());
            if (nested_wall == nested_genes.end()) {
                CGeneModel new_nested_wall(ir->Strand(),ir->ID(),CGeneModel::eNested+CGeneModel::eGnomon);
                new_nested_wall.SetGeneID(ir->GeneID());
                new_nested_wall.AddExon(limits);
                nested_genes[ir->GeneID()] = new_nested_wall;
            } else if (limits.GetTo() - nested_wall->second.Limits().GetTo() > 0) {
                    nested_wall->second.ExtendRight(limits.GetTo()- nested_wall->second.Limits().GetTo());
            }
            continue;
        }

        if ( right < limits.GetFrom() ) { // new cluster
            wall_model.reset( new CGeneModel(ir->Strand(),ir->ID(),CGeneModel::eWall+CGeneModel::eGnomon));
            wall_model->SetGeneID(ir->GeneID());
            wall_model->AddExon(limits);
        }
        /* doesn't work for genes which onclude coding and non coding together
        else { // same cluster
            _ASSERT( wall_model.get() != 0 && wall_model->GeneID()==ir->GeneID() );
        }
        */
        
        right = max(right, limits.GetTo());
        if (ir->RankInGene() == 1 && !ir->GoodEnoughToBeAnnotation()) {
            ir->Status() &= ~CGeneModel::eFullSupCDS;
            aligns.splice(aligns.end(), models, ir);
            wall_model->SetType(CGeneModel::eGnomon);
        } else if (limits.GetTo()- wall_model->Limits().GetTo() > 0)  {
            wall_model->ExtendRight(limits.GetTo() - wall_model->Limits().GetTo());
        }
    }
    SaveWallModel(wall_model, nested_genes, aligns);
}

void CGnomonAnnotator::Predict(TGeneModelList& models, TGeneModelList& bad_aligns)
{
    if (models.empty() && int(m_gnomon->GetSeq().size()) < mincontig)
        return;

    if (GnomonNeeded()) {

        //extend partial nested

        typedef list<TGeneModelList::iterator> TIterList;
        typedef map<Int8,TIterList> TGIDIterlist;
        TGIDIterlist genes;
        NON_CONST_ITERATE(TGeneModelList, im, models) {
            genes[im->GeneID()].push_back(im);
        }

        set<TSignedSeqRange> hosting_intervals;
        ITERATE(TGIDIterlist, ig, genes) {  // first - ID; second - list    
            bool coding_gene = false;
            ITERATE(TIterList, im, ig->second) {
                if((*im)->ReadingFrame().NotEmpty()) {
                    coding_gene = true;
                    break;
                }
            }

            TSignedSeqRange gene_lim_for_nested;
            ITERATE(TIterList, im, ig->second) {
                const CGeneModel& ai = **im;
                if(coding_gene && ai.ReadingFrame().Empty())
                    continue;
                TSignedSeqRange model_lim_for_nested = ai.Limits();
                if(ai.ReadingFrame().NotEmpty())
                    model_lim_for_nested = ai.OpenCds() ? ai.MaxCdsLimits() : ai.RealCdsLimits();   // 'open' could be only a single variant gene   
                gene_lim_for_nested += model_lim_for_nested;
            }

            vector<int> grange(gene_lim_for_nested.GetLength(),1);
            ITERATE(TIterList, im, ig->second) {   // exclude all positions included in CDS (any exons for not coding genes) and holes  
                const CGeneModel& ai = **im;
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
                    hosting_intervals.insert(interval);
                    left = -1; 
                }
            }
        }

        typedef map<TSignedSeqRange,TIterList> TRangeModels;
        TRangeModels nested_models;
        ITERATE(TGIDIterlist, ig, genes) {  // first - ID; second - list
            TGeneModelList::iterator nested_modeli = ig->second.front();                      
            if(!nested_modeli->GoodEnoughToBeAnnotation()) {
                _ASSERT(ig->second.size() == 1);
                TSignedSeqRange lim_for_nested = nested_modeli->RealCdsLimits().Empty() ? nested_modeli->Limits() : nested_modeli->RealCdsLimits();
                
                TSignedSeqRange hosting_interval;
                ITERATE(set<TSignedSeqRange>, ii, hosting_intervals) {
                    TSignedSeqRange interval = *ii;
                    if(Include(interval,lim_for_nested)) {
                        if(hosting_interval.Empty())
                            hosting_interval = interval;
                        else
                            hosting_interval = (hosting_interval&interval);
                    }
                }
                //                _ASSERT((ig->second.front()->Type()&CGeneModel::eNested) == 0 || !hosting_interval.Empty());       

                if(hosting_interval.NotEmpty()) {
                    TIterList nested(1,nested_modeli);
                    ITERATE(TGIDIterlist, igg, genes) {  // first - ID; second - list 
                        const TIterList& other_gene = igg->second;
                        if(igg == ig || !other_gene.front()->GoodEnoughToBeAnnotation())
                            continue;

                        bool coding_gene = false;
                        ITERATE(TIterList, im, other_gene) {
                            if((*im)->ReadingFrame().NotEmpty()) {
                                coding_gene = true;
                                break;
                            }
                        }

                        TSignedSeqRange finished_interval;
                        ITERATE(TIterList, im, other_gene) {
                            const CGeneModel& ai = **im;
                            if(coding_gene && ai.ReadingFrame().Empty())
                                continue;
                
                            finished_interval += coding_gene ? ai.RealCdsLimits() : ai.Limits();
                        }
                        if(!finished_interval.IntersectingWith(hosting_interval) || Include(finished_interval,hosting_interval))
                            continue;

                        if(Precede(finished_interval,lim_for_nested)) {           // before partial model
                            hosting_interval.SetFrom(finished_interval.GetTo());
                        } else if(Precede(lim_for_nested,finished_interval)) {    // after partial model
                            hosting_interval.SetTo(finished_interval.GetFrom());
                        } else {                                                  // overlaps partial model
                            _ASSERT(CModelCompare::RangeNestedInIntron(finished_interval, *nested_modeli, true));
                            ITERATE(TIterList, im, other_gene) {
                                nested.push_back(*im);
                            }
                        }
                    }
                    _ASSERT(hosting_interval.NotEmpty());
                    nested_models[hosting_interval].splice(nested_models[hosting_interval].begin(),nested);
                }
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
                    if(((*im)->Type()&CGeneModel::eNested)) {
                        nested.back().SetType(nested.back().Type()-CGeneModel::eNested);  // remove flag to allow ab initio extension
                    }

                    if(nested.back().HasStart() && !Include(hosting_interval,nested.back().MaxCdsLimits())) {
                        CCDSInfo cds = nested.back().GetCdsInfo();
                        if(nested.back().Strand() == ePlus)
                            cds.Set5PrimeCdsLimit(cds.Start().GetFrom());
                        else
                            cds.Set5PrimeCdsLimit(cds.Start().GetTo());
                        nested.back().SetCdsInfo(cds);
                    }
                        

#ifdef _DEBUG 
                    nested.back().AddComment("partialnested");
#endif    
             
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

    Predict(models, bad_aligns, 0, TSignedSeqPos(m_gnomon->GetSeq().size())-1);

    ERASE_ITERATE(TGeneModelList, im, models) {
        CGeneModel& model = *im;
        TSignedSeqRange cds = model.RealCdsLimits();
        if(cds.Empty())
            continue;

        bool has_genome_cds = false;
        ITERATE(CGeneModel::TExons, ie, model.Exons()) {
            if(cds.IntersectingWith(ie->Limits()) && ie->m_fsplice_sig != "XX" && ie->m_ssplice_sig != "XX") {
                has_genome_cds = true;
                break;
            }
        }

        if(!has_genome_cds) {
            model.Status() |= CGeneModel::eSkipped;
            model.AddComment("Whole CDS in genomic gap");
            bad_aligns.push_back(model);
            models.erase(im);
        }
    }
}

void CGnomonAnnotator::Predict(TGeneModelList& models, TGeneModelList& bad_aligns, TSignedSeqPos left, TSignedSeqPos right)
{
    if (GnomonNeeded()) {

        models.sort(s_AlignSeqOrder);

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
        TInDels fs;
        TSignedSeqRange fullcds = it->GetCdsInfo().Start()+it->GetCdsInfo().ReadingFrame()+it->GetCdsInfo().Stop();
        ITERATE(TInDels, i, it->FrameShifts()) {   // removing fshifts in UTRs
            if((i->IsInsertion() && Include(fullcds,i->Loc())) ||
               (i->IsDeletion() && i->Loc() > fullcds.GetFrom() && i->Loc() <= fullcds.GetTo())) {
                fs.push_back(*i);
            }
        }
        it->FrameShifts() = fs;

#ifdef _DEBUG
        {
            string protein = it->GetProtein(m_gnomon->GetSeq());
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
    arg_desc->AddFlag("nognomon","Skips ab initio prediction and ab initio extension of partial chains.");
    arg_desc->AddKey("param", "param",
                     "Organism specific parameters",
                     CArgDescriptions::eInputFile);
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

