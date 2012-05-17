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
    cout << "Testing alignment " << chain.ID() << " in fragment " << l << ' ' << r << endl;
                    
    m_gnomon->ResetRange(l,r);
    return m_gnomon->Run(test_align, true, false, false, false, false, mpp, nonconsensp);
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
                cout << "Deleting alignment " << it->ID() << endl;
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
            cout << "Testing w/o bad alignments in fragment " << left << ' ' << right << endl;
            return m_gnomon->Run(suspect_aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp);
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
        
        cout << "Testing w/o " << algn.ID();
        score = m_gnomon->Run(suspect_aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp);
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
                                      bool leftwall, bool rightwall, bool leftanchor, bool rightanchor)
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
        score = m_gnomon->Run(suspect_aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp);
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
        
    typedef set<TSignedSeqRange> TSRAIntronsSet;
    if(m_gnomon->GetSRAIntrons() != 0) {
        ITERATE(TSRAIntronsSet, it, *m_gnomon->GetSRAIntrons()) {
            int a = max(0,it->GetFrom()-margin);
            int b = min(rlimit,it->GetTo()+margin);
            for(int i = a; i<=b; ++i)
                busy_spots[i] = 1;
        }
    }
        
    if(m_gnomon->GetSRAIslands() != 0) {
        ITERATE(TSRAIntronsSet, it, *m_gnomon->GetSRAIslands()) {
            int a = max(0,it->GetFrom()-margin);
            int b = min(rlimit,it->GetTo()+margin);
            for(int i = a; i<=b; ++i)
                busy_spots[i] = 1;
        }
    }

    do {
        /*
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
        */
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

            cout << left << ' ' << right << ' ' << m_gnomon->GetGCcontent() << endl;
        
            score = m_gnomon->Run(aligns, true, leftwall, rightwall, leftanchor, rightanchor, mpp, nonconsensp);
        
            if(score == BadScore()) {
                cout << "Inconsistent alignments in fragment " << left << ' ' << right << '\n';

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
                cout << "!!! BAD SCORE EVEN WITH FINISHED ALIGNMENTS !!! " << endl;
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
            ir->SetType( ir->Type() - CGeneModel::eNested );
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
        } else { // same cluster
            _ASSERT( wall_model.get() != 0 && wall_model->GeneID()==ir->GeneID() );
        }
        
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
    Predict(models, bad_aligns, 0, TSignedSeqPos(m_gnomon->GetSeq().size())-1);
}

void CGnomonAnnotator::Predict(TGeneModelList& models, TGeneModelList& bad_aligns, TSignedSeqPos left, TSignedSeqPos right)
{
    if (GnomonNeeded()) {

        models.sort(s_AlignSeqOrder);

        TGeneModelList aligns;

        FindPartials(models, aligns, ePlus);
        FindPartials(models, aligns, eMinus);

        aligns.sort(s_AlignSeqOrder);

        TGeneModelList::const_iterator il = aligns.begin();

    /*
    TSignedSeqPos left = 0;
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

        Predict(left, right, il, aligns.end(), models,(left!=0 || wall), wall, left!=0, false, bad_aligns);
    }

    NON_CONST_ITERATE(TGeneModelList, it, models) {
        TInDels fs;
        ITERATE(TInDels, i, it->FrameShifts()) {   // removing fshifts in UTRs
            if (i->IntersectingWith(it->ReadingFrame().GetFrom(), it->ReadingFrame().GetTo()))
                fs.push_back(*i);
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

void CGeneSelectorArgUtil::SetupArgDescriptions(CArgDescriptions* arg_desc)
{
    arg_desc->AddDefaultKey("intergenic","intergenic","Minimum intergenic distance",CArgDescriptions::eInteger,"20");
    arg_desc->AddDefaultKey("altfrac","altfrac","The CDS length of the principal model in the gene is multiplied by this fraction. Alt variants with the CDS length above "
                            "this are included in gene",CArgDescriptions::eDouble,"80.0");
    arg_desc->AddDefaultKey("composite","composite","Maximal composite number in alts",CArgDescriptions::eInteger,"1");
    arg_desc->AddFlag("opposite","Allow overlap of complete multiexon genes with opposite strands");
    arg_desc->AddFlag("partialalts","Allows partial alternative variants. In combination with -nognomon will allow partial genes");
}

void CGeneSelectorArgUtil::ReadArgs(CGeneSelector* selector, const CArgs& args)
{
    selector->minIntergenic = args["intergenic"].AsInteger();
    selector->altfrac = args["altfrac"].AsDouble();
    selector->composite = args["composite"].AsInteger();
    selector->allow_opposite_strand = args["opposite"];
    selector->allow_partialalts = args["partialalts"];
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

    arg_desc->AddDefaultKey("sraintronpenalty","sraintronpenalty","Penalty for intron not included in SRA",CArgDescriptions::eDouble,"5");
    arg_desc->AddOptionalKey("sraintrons", "sraintrons","Short reads introns",CArgDescriptions::eInputFile);
    arg_desc->AddDefaultKey("sraislandpenalty","sraislandpenalty","Penalty for exon not included in SRA island",CArgDescriptions::eDouble,"5");
    arg_desc->AddOptionalKey("sraislands", "sraislands","Short reads islands",CArgDescriptions::eInputFile);

    arg_desc->AddFlag("norep","DO NOT mask lower case letters");
    arg_desc->AddDefaultKey("mincont","mincont","Contigs shorter than that will be skipped unless they have alignments.",CArgDescriptions::eInteger,"1000");

    arg_desc->SetCurrentGroup("Prediction tuning");
    arg_desc->AddFlag("singlest","Allow single exon EST chains as evidence");
    arg_desc->AddDefaultKey("tolerance","tolerance","if models exon boundary differ only this much only one model will survive",CArgDescriptions::eInteger,"5");
    arg_desc->AddDefaultKey("minsupport","minsupport","Minimal number of support lines for short CDS or noncoding models",CArgDescriptions::eInteger,"5");
    arg_desc->AddDefaultKey("minlen","minlen","Minimal CDS length to be excluded from the above rule.",CArgDescriptions::eInteger,"300");
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
    annot->tolerance = args["tolerance"].AsInteger();

    annot->minCdsLen = args["minlen"].AsInteger();
    annot->minsupport = args["minsupport"].AsInteger();
    if (!args["norep"])
        annot->EnableSeqMasking();
}
END_SCOPE(gnomon)
END_NCBI_SCOPE

