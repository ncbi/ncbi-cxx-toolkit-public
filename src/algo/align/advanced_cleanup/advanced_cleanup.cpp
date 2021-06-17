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
 * Author:  Mike DiCuccio, Yuri Kapustin
 *
 * File Description: Splign worker node for parallel NetSchedule processing
 *
 */

#include <ncbi_pch.hpp>

#include <util/range.hpp>
#include <util/stream_source.hpp>

#include <objects/seqalign/Score_set.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqalign/Product_pos.hpp>
#include <objects/seqalign/Spliced_seg.hpp>
#include <objects/seqalign/Spliced_exon.hpp>
#include <objects/seqalign/Spliced_exon_chunk.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Align_def.hpp>

#include <objmgr/seq_vector.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/sequence/align_cleanup.hpp>
#include <algo/align/util/score_builder.hpp>
#include <algo/align/util/genomic_compart.hpp>
#include <algo/align/util/best_placement.hpp>
#include <algo/align/splign/splign_formatter.hpp>
#include <algo/align/splign/splign_cmdargs.hpp>

#include <algo/align/advanced_cleanup/advanced_cleanup.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

const size_t kMaxIntron = 2000000;

///////////////////////////////////////////////////////////////////////

unsigned CAdvancedAlignCleanup::CSplignCompartment::s_MaxRnaIntronSize = UINT_MAX;
unsigned CAdvancedAlignCleanup::CSplignCompartment::s_MinRnaTotalCoverage = 0;

void CAdvancedAlignCleanup::SetupArgDescriptions(CArgDescriptions &arg_desc)
{
    arg_desc.SetCurrentGroup("Splign-Specific Arguments");
    CSplignArgUtil::SetupArgDescriptions(&arg_desc);

    arg_desc.AddDefaultKey("max-rna-intron", "MaxRnaIntron",
                           "Maximum intron size for a Splign compartment to "
                           "identify its query as RNA",
                           CArgDescriptions::eInteger, "20000");

    arg_desc.AddDefaultKey("min-rna-total-coverage", "MinRnaTotalCoverage",
                           "Minimum total query percent coverage for a Splign "
                           "compartment to be identified as RNA",
                           CArgDescriptions::eInteger, "35");

    arg_desc.SetCurrentGroup("Genomic Compartment Options");
    arg_desc.AddFlag("allow-consistent-intersection",
                      "Allow intersecting alignments in genomic compartments, "
                      "but enforce consistency");
    arg_desc.AddFlag("allow-inconsistent-intersection",
                      "Allow intersecting alignments in genomic compartments, "
                      "and don't enforce consistency");
    arg_desc.SetDependency("allow-inconsistent-intersection",
                           CArgDescriptions::eExcludes,
                           "allow-consistent-intersection");

    arg_desc.AddFlag("allow-large-compart-gaps",
                      "Permit genomic compartments to contain large gaps between "
                      "alignments; default limits gaps to 3 x alignment size");

    arg_desc.AddDefaultKey("max-compartment-failures", "MaxCompartmentFailures",
                           "Fail if we have failure on more than this number of compartments",
                           CArgDescriptions::eInteger, "5");

    arg_desc.SetCurrentGroup("ProSplign-Specific Arguments");

    CProSplignScoring::SetupArgDescriptions(&arg_desc);
    CProSplignOutputOptions::SetupArgDescriptions(&arg_desc);
    arg_desc.AddFlag("no-prosplign-introns",
                     "Generate ProSplign alignment without introns");

    prosplign::CCompartOptions::SetupArgDescriptions(&arg_desc);
    arg_desc.AddOptionalKey("prosplign-size-threshold", "ProsplignSizeThreshold",
                            "Maximum compartment size - protein length x "
                            "genomic range length - on which to run prosplign",
                            CArgDescriptions::eInteger);
    
    arg_desc.AddFlag("prosplign-gaps", "Precalculate un-bridgeable gaps. Prohibit compartment to go over un-bridgeable gaps.");
    arg_desc.AddFlag("prosplign-unk-gaps", "Prohibit compartment to go over gaps of unknown length.");
    arg_desc.SetDependency("prosplign-unk-gaps", CArgDescriptions::eRequires, "prosplign-gaps");

    CInputStreamSource::SetStandardInputArgs(arg_desc, "gc-assembly",
                            "GenColl ASN.1 to process");
}


CAdvancedAlignCleanup::CAdvancedAlignCleanup()
    : m_Penalty(0.55)
    , m_MinIdty(0.7)
    , m_MinSingletonIdty(0.7)
    , m_MinSingletonIdtyBps(9999999)
    , m_MaxIntron(CCompartmentFinder<CSplign::THit>::s_GetDefaultMaxIntron())
    , m_NoXF(false)
    , m_GenomicCompartOptions(fCompart_Defaults)
    , m_MaxCompartmentFailures(UINT_MAX)
    , m_CompartmentFailureCount(0)
    , m_ProsplignGaps(false)
    , m_ProsplignUnknownGaps(false)
    , m_ProsplignRunThreshold(UINT_MAX)
{
}

CAdvancedAlignCleanup::~CAdvancedAlignCleanup()
{
}

void CAdvancedAlignCleanup::Reset()
{
    m_CompartmentFailureCount = 0;
}

void CAdvancedAlignCleanup::SetParams(const CArgs& args)
{

    CSplignArgUtil::ArgsToSplign(&m_Splign, args);

    m_Splign.PreserveScope();
    m_Splign.SetStartModelId(1);
    /// The max_intron parameter will be used for the compartment finder;
    /// however there's no guarantee that the introns in compartments will
    /// really be less than the value, so set max-intron to large value in
    /// Splign itself to avoid failures
    m_Splign.SetMaxIntron(kMaxIntron);

    m_Penalty = args["compartment_penalty"].AsDouble();
    m_MinIdty = args["min_compartment_idty"].AsDouble();
    m_MinSingletonIdtyBps = args["min_singleton_idty_bps"].AsInteger();

    m_MinSingletonIdty = m_MinIdty;
    if (args["min_singleton_idty"]) {
        m_MinSingletonIdty = args["min_singleton_idty"].AsDouble();
    }

    m_MaxIntron = args["max_intron"].AsInteger();

    if (args["allow-consistent-intersection"] ||
        args["allow-inconsistent-intersection"])
    {
        m_GenomicCompartOptions |= fCompart_AllowIntersections;
    }
    if (args["allow-inconsistent-intersection"]) {
        m_GenomicCompartOptions |= fCompart_AllowInconsistentIntersection;
    }
    if (!args["allow-large-compart-gaps"]) {
        m_GenomicCompartOptions |= fCompart_FilterByDiffLen;
    }

    m_MaxCompartmentFailures = args["max-compartment-failures"].AsInteger();

    CRef<CGC_Assembly> assm;
    for (CInputStreamSource assm_source(args, "gc-assembly");
         assm_source; ++assm_source)
    {
        if (assm) {
            NCBI_THROW(CException, eUnknown,
                       "Can have only one gc assembly input");
        }
        _TRACE("Reading assembly...");
        assm.Reset(new CGC_Assembly);
        *assm_source >> MSerial_AsnText >> *assm;

        // don't forget to force creation of all internal indices!
        assm->CreateHierarchy();
        assm->CreateIndex();
    }

    m_ProSplignScoring.Reset(new CProSplignScoring(args));
    m_ProSplignOutputOptions.Reset(new CProSplignOutputOptions(args));
    m_CompartOptions.reset(new prosplign::CCompartOptions(args));
    m_CompartOptions->m_SubjectMol = prosplign::CCompartOptions::eNucleicAcid;

    CSplignCompartment::s_MaxRnaIntronSize = args["max-rna-intron"]
                                               . AsInteger();
    CSplignCompartment::s_MinRnaTotalCoverage = args["min-rna-total-coverage"]
                                               . AsInteger();

    /// The following three parameters exist both for splign and for prosplign,
    /// with different defaults; if not provided, need to set them explicitly
    /// for prosplign
    const CNcbiArguments raw_args = CNcbiApplication::Instance()->GetArguments();
    set<string> command_line_args;
    for (unsigned arg = 0; arg < raw_args.Size(); ++arg) {
        command_line_args.insert(raw_args[arg]);
    }
    if (!command_line_args.count("-min_hole_len")) {
       m_ProSplignOutputOptions->SetMinHoleLen(CProSplignOutputOptions::default_min_hole_len);
    }
    if (!command_line_args.count("-compartment_penalty")) {
       m_CompartOptions->m_CompartmentPenalty = prosplign::CCompartOptions::default_CompartmentPenalty;
    }
    if (!command_line_args.count("-min_compartment_idty")) {
       m_CompartOptions->m_MinCompartmentIdty = prosplign::CCompartOptions::default_MinCompartmentIdty;
    }
    m_ProSplign.Reset(new CProSplign(*m_ProSplignScoring,
                                     args["no-prosplign-introns"]));
    m_ProsplignGaps = args["prosplign-gaps"];
    m_ProsplignUnknownGaps = args["prosplign-unk-gaps"];
    if (args["prosplign-size-threshold"]) {
        m_ProsplignRunThreshold = args["prosplign-size-threshold"].AsInteger();
    }
}

void CAdvancedAlignCleanup::SetScope(const CRef<CScope> &scope) {
    m_Scope = scope;
    m_Splign.SetScope() = scope;
}

void CAdvancedAlignCleanup::Cleanup(const CSeq_align_set::Tdata& input_aligns,
                                    CSeq_align_set::Tdata& cleaned_aligns,
                                    EQueryType query_type,
                                    bool with_best_placement,
                                    bool one_pair,
                                    ESplignDirRun splign_direction)
{
    cleaned_aligns.clear();
    if (!one_pair) {
        TAlignsBySeqIds aligns_by_pair;
        DivideByQuerySubjectPairs(input_aligns, aligns_by_pair);
        ITERATE (TAlignsBySeqIds, query_it, aligns_by_pair) {
            CSeq_align_set::Tdata this_query_output;
            Cleanup(query_it->second, this_query_output, query_type,
                    with_best_placement, splign_direction);
            cleaned_aligns.splice(cleaned_aligns.end(), this_query_output);
        }
    } else if (!input_aligns.empty()) {
        TAlignsBySubject subject_aligns;
        subject_aligns[CSeq_id_Handle::GetHandle(
                       input_aligns.front()->GetSeq_id(1))] = input_aligns;
        Cleanup(subject_aligns, cleaned_aligns, query_type,
                with_best_placement, splign_direction);
    }
}

void CAdvancedAlignCleanup::Cleanup(const TAlignsBySubject& query_aligns,
                                    CSeq_align_set::Tdata& cleaned_aligns,
                                    EQueryType query_type,
                                    bool with_best_placement,
                                    ESplignDirRun splign_direction)
{
    cleaned_aligns.clear();
/*
    if (with_best_placement) {
        NCBI_THROW(CException, eUnknown,
                   "Best placement not supported yet");
    }
*/

    list<CSplignCompartment> splign_compartments;
    list< CRef<CSeq_align_set> > genomic_compartments;
    if (query_type == eInfer) {
        /// Get query type from bioseq
        CSeq_id_Handle query_idh = CSeq_id_Handle::GetHandle(
                           query_aligns.begin()->second.front()->GetSeq_id(0));
        CBioseq_Handle bsh = m_Scope->GetBioseqHandle(query_idh);
        if (!bsh || !bsh.CanGetInst_Mol()) {
            NCBI_THROW(CException, eUnknown,
                       "Type information not available for "
                     + query_idh.AsString());
        }
        switch (bsh.GetInst_Mol()) {
        case CSeq_inst::eMol_dna:
            query_type = eGenomic;
            break;

        case CSeq_inst::eMol_rna:
            query_type = eRna;
            break;

        case CSeq_inst::eMol_aa:
            query_type = eProtein;
            break;

        default:
            ITERATE (TAlignsBySubject, subject_it, query_aligns) {
                GetSplignCompartments(subject_it->second,
                                      splign_compartments, true);
            }
            query_type = eGenomic;
            NON_CONST_ITERATE (list<CSplignCompartment>, compart_it,
                               splign_compartments)
            {
                if (compart_it->HasRnaCharacteristics()) {
                    query_type = eRna;
                    break;
                }
            }
            break;
        }
    }

    switch (query_type) {
    case eProtein:
    {{
         typedef map<CSeq_id_Handle, TAlignsByPos> TAlignsBySubjectAndPos;
         typedef map<CSeq_id_Handle, vector<TSeqRange> >
                 TCompartmentsBySubject;
         TAlignsBySubjectAndPos indexed_aligns;
         TCompartmentsBySubject failed_compartments;
         bool found_prosplign_alignments = false;
         ITERATE (TAlignsBySubject, subject_it, query_aligns) {
             prosplign::TCompartments compartments;
             TAlignsByPos &aligns_by_pos = indexed_aligns[subject_it->first];
             GetProsplignCompartments(subject_it->second, compartments,
                                      true, &aligns_by_pos);
             sort(aligns_by_pos.begin(), aligns_by_pos.end());
             ITERATE (prosplign::TCompartments, compart_it, compartments) {
                 CSeq_align_set::Tdata compart_aligns;
                 TSeqRange genomic_range;
                 bool ran_prosplign =
                     x_CleanupProsplignCompartment(**compart_it, aligns_by_pos,
                                            compart_aligns, genomic_range);
                 if (ran_prosplign) {
                     if (compart_aligns.empty()) {
                         failed_compartments[subject_it->first]
                             . push_back(genomic_range);
                     } else {
                         found_prosplign_alignments = true;
                     }
                 }
                 cleaned_aligns.splice(cleaned_aligns.end(), compart_aligns);
             }
         }
         if (!found_prosplign_alignments) {
             /// Prosplign found no alignments on any of the compartments; use
             /// genomic cleanup on all of them
             ITERATE (TCompartmentsBySubject, subject_it, failed_compartments) {
                 ITERATE (vector<TSeqRange>, compart_it, subject_it->second)
                 {
                     x_CleanupProsplignAsGenomic(
                       indexed_aligns[subject_it->first], *compart_it,
                       cleaned_aligns);
                 }
             }
         }
    }}
    break;

    case eRna:
        /// We may have already calculated the splign compartments in order to
        /// infer the query type. If not, calculate them now.
        if (splign_compartments.empty()) {
            ITERATE (TAlignsBySubject, subject_it, query_aligns) {
                GetSplignCompartments(subject_it->second,
                                      splign_compartments, true);
            }
        }
     
        ITERATE (list<CSplignCompartment>, it, splign_compartments) {
            if (splign_direction != eDirAntisense) {
                CRef<CSeq_align> align = RunSplignOnCompartment(*it, eDirSense);
                if (align) {
                    cleaned_aligns.push_back(align);
                }
            }
            if (splign_direction != eDirSense) {
                CRef<CSeq_align> align = RunSplignOnCompartment(*it, eDirAntisense);
                if (align) {
                    cleaned_aligns.push_back(align);
                }
            }
        }
        if (!cleaned_aligns.empty()) {
            break;
        }

        /// Splign failed to find any alignments; use genomic method instead,
        /// but use the Splign compartments
        x_SplignCompartmentsToGenomicFormat(splign_compartments,
                                            genomic_compartments);

        // fallthrough

    default:
        if (query_type == eGenomic) {
            ITERATE (TAlignsBySubject, subject_it, query_aligns) {
                GetGenomicCompartments(subject_it->second,
                                       genomic_compartments, true);
            }
        }
        ITERATE (list< CRef<CSeq_align_set> >, compart_it, genomic_compartments)
        {
            CleanupGenomicCompartment((*compart_it)->Get(), cleaned_aligns);
        }
        break;

    }

    if (with_best_placement) {
        BestPlacement(cleaned_aligns);
    }
}

void CAdvancedAlignCleanup::
DivideByQuerySubjectPairs(const CSeq_align_set::Tdata& input_aligns,
                          TAlignsBySeqIds &aligns_by_pair)
{
    ITERATE (CSeq_align_set::Tdata, align_it, input_aligns) {
        CSeq_id_Handle query = CSeq_id_Handle::GetHandle((*align_it)->GetSeq_id(0)),
                       subject = CSeq_id_Handle::GetHandle((*align_it)->GetSeq_id(1));
        aligns_by_pair[query][subject].push_back(*align_it);
    }
}

void
CAdvancedAlignCleanup::GetSplignCompartments(const CSeq_align_set::Tdata& input_aligns,
                                             list<CSplignCompartment> &compartments,
                                             bool one_pair)
{
    if (!one_pair) {
        TAlignsBySeqIds aligns_by_pair;
        DivideByQuerySubjectPairs(input_aligns, aligns_by_pair);
        ITERATE (TAlignsBySeqIds, query_it, aligns_by_pair) {
            ITERATE (TAlignsBySubject, subject_it, query_it->second) {
                GetSplignCompartments(subject_it->second, compartments, true);
            }
        }
    } else if (!input_aligns.empty()) {
        CSeq_id_Handle query;
        CSeq_id_Handle subject;
        CSplign::THitRefs hit_refs;
        {{
             ITERATE (list< CRef<CSeq_align> >, iter, input_aligns) {
                 CRef<CBlastTabular> hitref (new CSplignAlignmentHit(**iter));
                 if (hitref->GetQueryStrand() == false) {
                     hitref->FlipStrands();
                 }
                 hit_refs.push_back(hitref);

                 if ( !query ) {
                     query = CSeq_id_Handle::GetHandle((*iter)->GetSeq_id(0));
                     subject = CSeq_id_Handle::GetHandle((*iter)->GetSeq_id(1));
                 }
             }
         }}

        TSeqPos qlen = m_Scope->GetSequenceLength(query, CScope::fThrowOnMissing);

        const TCoord penalty_bps (TCoord(m_Penalty * qlen + 0.5));
        const TCoord min_matches (TCoord(m_MinIdty * qlen + 0.5));
        const TCoord msm1        (TCoord(m_MinSingletonIdty * qlen + 0.5));
        const TCoord msm2        (m_MinSingletonIdtyBps);
        const TCoord min_singleton_matches (min(msm1, msm2));

        TAccessor ca(penalty_bps, min_matches, min_singleton_matches, !m_NoXF);
        ca.SetMaxIntron(m_MaxIntron);
        ca.Run(hit_refs.begin(), hit_refs.end(), m_Scope.GetPointer());

        const TCoord kCoordMax (numeric_limits<TCoord>::max());
        TCoord smin (0), smax (kCoordMax);
        bool same_strand (false);

        for(size_t i(0), n(ca.GetCounts().first); i < n; ++i) {
            const TCoord* box (ca.GetBox(i));
            if(i + 1 == n) {
                smax = kCoordMax;
                same_strand = false;
            }
            else {            
                bool strand_this (ca.GetStrand(i));
                bool strand_next (ca.GetStrand(i+1));
                same_strand = strand_this == strand_next;
                smax = same_strand? (box + 4)[2]: kCoordMax;
            }

            if(ca.GetStatus(i)) {
                CSplignCompartment comp;
		comp.query = query;
                comp.query_length = qlen;
                comp.subject = subject;
                ca.Get(i, comp.hits);
                if(smax < box[3]) smax = box[3];
                if(smin > box[2]) smin = box[2];
                comp.coord_range = TCoordRange(smin,smax);
                comp.range.Set(box[2],box[3]);
                compartments.push_back(comp);
            }

            smin = same_strand? box[3]: 0;
        }
    }
}

void
CAdvancedAlignCleanup::GetGenomicCompartments(const CSeq_align_set::Tdata& input_aligns,
                                              list< CRef<CSeq_align_set> > &compartments,
                                              bool one_pair)
{
    if (!one_pair) {
        TAlignsBySeqIds aligns_by_pair;
        DivideByQuerySubjectPairs(input_aligns, aligns_by_pair);
        ITERATE (TAlignsBySeqIds, query_it, aligns_by_pair) {
            ITERATE (TAlignsBySubject, subject_it, query_it->second) {
                GetGenomicCompartments(subject_it->second, compartments, true);
            }
        }
    } else {
        FindCompartments(input_aligns, compartments, m_GenomicCompartOptions);
    }
}

void CAdvancedAlignCleanup::
GetProsplignCompartments(const CSeq_align_set::Tdata& input_aligns,
                         prosplign::TCompartments &compartments,
                         bool one_pair,
                         TAlignsByPos *aligns_by_pos)
{
    if (!one_pair) {
        TAlignsBySeqIds aligns_by_pair;
        DivideByQuerySubjectPairs(input_aligns, aligns_by_pair);
        ITERATE (TAlignsBySeqIds, query_it, aligns_by_pair) {
            ITERATE (TAlignsBySubject, subject_it, query_it->second) {
               GetProsplignCompartments(subject_it->second, compartments,
                                        true, aligns_by_pos);
            }
        }
    } else if (!input_aligns.empty()) {
        CSeq_id_Handle subject =
            CSeq_id_Handle::GetHandle(input_aligns.front()->GetSeq_id(1));
        CSplign::THitRefs hit_refs;
        ITERATE (list< CRef<CSeq_align> >, iter, input_aligns) {
            CRef<CBlastTabular> hitref (new CBlastTabular(**iter));
            hit_refs.push_back(hitref);
            if (aligns_by_pos) {
              aligns_by_pos->push_back(make_pair(hitref->GetSubjMin(), *iter));
            }
        }
        vector<pair<TSeqPos, TSeqPos> > *pgap = NULL;
    
        if( m_ProsplignGaps ) {
            bool unk_gaps = m_ProsplignUnknownGaps;
            if(!m_seq_gaps.count(subject)) {
                CBioseq_Handle bsh = m_Scope->GetBioseqHandle(subject);
                vector<pair<TSeqPos, TSeqPos> >& gaps = m_seq_gaps[subject];
                SSeqMapSelector sel;
                sel.SetFlags(CSeqMap::fFindGap);
                sel.SetResolveCount(size_t(-1));
                CSeqMap_CI smit(bsh, sel);
                for(;smit; ++smit) { 
                    if(smit.GetType() == CSeqMap::eSeqGap) { 
                        CConstRef<CSeq_literal> slit = smit.GetRefGapLiteral(); 
                        if(slit && (slit->GetBridgeability() == CSeq_literal::e_NotBridgeable ||
                                    (unk_gaps && slit->CanGetFuzz() && slit->GetFuzz().IsLim() &&
                                     slit->GetFuzz().GetLim() == CInt_fuzz::eLim_unk)
                                    )
                           ) { 
                            //GetEndPosition() is exclusive 
                            gaps.push_back(make_pair(smit.GetPosition(), smit.GetEndPosition() - 1));
                        }
                    }
                }        
            }
    
            if(m_seq_gaps[subject].size()) {
                pgap = & m_seq_gaps[subject];
            }
        }
        {
            typedef CHitComparator<CBlastTabular> THitComparator;
            THitComparator sorter (THitComparator::eSubjMinSubjMax);
            stable_sort(hit_refs.begin(), hit_refs.end(), sorter);
        }
        prosplign::TCompartments compartments_for_aligns =
          prosplign::SelectCompartmentsHits(hit_refs, *m_CompartOptions, pgap);
        compartments.splice(compartments.end(), compartments_for_aligns);
    }
}

CRef<CSeq_align>
CAdvancedAlignCleanup::RunSplignOnCompartment(const CSplignCompartment &compart,
                                               ESplignDirRun dir)
{
    if (dir == eDirBoth) {
        NCBI_THROW(CException, eUnknown,
                   "Function RunSplignOnCompartment() supported only "
                   "for one direction");
    }

    CRef<CSeq_align> result;

    CSplign::THitRefs hits0;
    ITERATE (CSplign::THitRefs, ii, compart.hits) {
        CSplign::THitRef h1 (new CSplign::THit(**ii));
        hits0.push_back(h1);
    }

    CSplign::SAlignedCompartment comp_align;
    m_Splign.SetStrand(dir == eDirSense);
    m_Splign.SetStartModelId(1);
    m_Splign.AlignSingleCompartment(&hits0,
                                    compart.coord_range.first,
                                    compart.coord_range.second,
                                    &comp_align);
    if (comp_align.m_Status == CSplign::SAlignedCompartment::eStatus_Ok) {
        CSplign::TResults splign_results(1, comp_align);
        CSplignFormatter formatter(m_Splign);

        formatter.SetSeqIds(compart.query.GetSeqId(),
                            compart.subject.GetSeqId());

        CRef<CSeq_align_set> ses =
            formatter.AsSeqAlignSet
            (&splign_results, CSplignFormatter::eAF_SplicedSegWithParts);
        result = ses->Get().front();

        x_AddStandardAlignmentScores(*result);
    } else if(comp_align.m_Status == CSplign::SAlignedCompartment::eStatus_Error) {
        GetDiagContext().Extra()
            .Print("splign_error", comp_align.m_Msg )
            .Print("compart_query", compart.query.AsString())
            .Print("compart_subject", compart.subject.AsString())
            .Print("compart_from", NStr::NumericToString(compart.range.GetFrom()))
            .Print("compart_to", NStr::NumericToString(compart.range.GetTo()))
            .Print("compart_strand", dir == eDirSense ? "plus" : "minus");
        if (comp_align.m_Msg.find("Space limit exceeded") == string::npos &&
            ++m_CompartmentFailureCount > m_MaxCompartmentFailures)
        {
            NCBI_THROW(CException, eUnknown,
                       "Failures on too many compartments");
        }
    }
    return result;
}


void CAdvancedAlignCleanup::
CleanupGenomicCompartment(const CSeq_align_set::Tdata& compart,
                          CSeq_align_set::Tdata& cleaned_aligns,
                          bool add_scores)
{
    CSeq_align_set::Tdata compart_cleaned_aligns;
    CAlignCleanup cleanup(*m_Scope);
    cleanup.FillUnaligned(true);
    cleanup.Cleanup(compart, compart_cleaned_aligns);
    if (add_scores) {
        NON_CONST_ITERATE (CSeq_align_set::Tdata, it, compart_cleaned_aligns) {
            x_AddStandardAlignmentScores(**it);
        }
    }
    cleaned_aligns.splice(cleaned_aligns.end(), compart_cleaned_aligns);
}

bool CAdvancedAlignCleanup::
x_CleanupProsplignCompartment(const CSeq_annot &compartment,
                              const TAlignsByPos &aligns_by_pos,
                              CSeq_align_set::Tdata& cleaned_aligns,
                              TSeqRange &genomic_range)
{
	CSeq_id_Handle protein;
	CConstRef<CSeq_loc> genomic_loc;

	ITERATE (CSeq_annot::TDesc::Tdata, desc_it, compartment.GetDesc().Get()) {
		const CAnnotdesc& desc = **desc_it;
		switch (desc.Which()) {
			case CAnnotdesc::e_Align:
				//
				// protein id
				//
				if (desc.GetAlign().GetIds().size() != 1) {
					NCBI_THROW(CException, eUnknown,
							"unexpected number of IDs in align-ref");
				}
				protein = CSeq_id_Handle::GetHandle
					(*desc.GetAlign().GetIds().front());
				break;

			case CAnnotdesc::e_Region:
				//
				// genomic region
				//
				genomic_loc.Reset(&desc.GetRegion());
				break;

			default:
				break;
		}
	}

	if ( !protein ) {
		NCBI_THROW(CException, eUnknown,
				"failed to find protein");
	}

	if ( !genomic_loc ) {
		NCBI_THROW(CException, eUnknown,
				"failed to find genomic location");
	}

	genomic_range = genomic_loc->GetTotalRange();

	if (m_Scope->GetSequenceLength(protein) * genomic_range.GetLength()
			<= m_ProsplignRunThreshold)
	{
		CRef<CSeq_align> prosplign_output = m_ProSplign->FindAlignment(
				*m_Scope, *protein.GetSeqId(),
				*genomic_loc, *m_ProSplignOutputOptions);
		if (!prosplign_output->GetSegs().GetSpliced().GetExons().empty()) {
			x_AddStandardAlignmentScores(*prosplign_output);
			cleaned_aligns.push_back(prosplign_output);
		}
		return true;
	} else {
		/// Compartment too large to run Prosplign; merge as if these were
		/// genomic alignments. Don't add scores, since scores don't work for
		/// protein Dense-seg alignments
		x_CleanupProsplignAsGenomic(aligns_by_pos, genomic_range, cleaned_aligns);
		return false;
	}
}

void CAdvancedAlignCleanup::
x_CleanupProsplignAsGenomic(const TAlignsByPos &aligns_by_pos,
                            const TSeqRange &genomic_range,
                            CSeq_align_set::Tdata& cleaned_aligns)
{
    CSeq_align_set::Tdata compart_members;
    for (TAlignsByPos::const_iterator it =
        lower_bound(aligns_by_pos.begin(), aligns_by_pos.end(),
                    make_pair(genomic_range.GetFrom(), CRef<CSeq_align>()));
        it != lower_bound(aligns_by_pos.begin(), aligns_by_pos.end(),
                    make_pair(genomic_range.GetToOpen(), CRef<CSeq_align>()));
        ++it)
    {
        compart_members.push_back(it->second);
    }
     /// . Don't add scores, since scores don't work for
     /// protein Dense-seg alignments
    CleanupGenomicCompartment(compart_members, cleaned_aligns, false);
}

void CAdvancedAlignCleanup::x_SplignCompartmentsToGenomicFormat(
     const list<CSplignCompartment> &splign_compartments,
     list< CRef<CSeq_align_set> > &genomic_compartments)
{
    ITERATE (list<CSplignCompartment>, compart_it, splign_compartments) {
        CRef<CSeq_align_set> genomic_compart(new CSeq_align_set);
        ITERATE (CSplign::THitRefs, hit_it, compart_it->hits) {
            genomic_compart->Set().push_back(
                dynamic_cast<const CSplignAlignmentHit *>(hit_it->GetPointer())
                    -> GetAlign());
        }
        genomic_compartments.push_back(genomic_compart);
    }
}

void CAdvancedAlignCleanup::x_AddStandardAlignmentScores(CSeq_align& align)
{
    CScoreBuilderBase sb;

    sb.AddScore(*m_Scope, align,
                CSeq_align::eScore_PercentIdentity_Gapped);
    sb.AddScore(*m_Scope, align,
                CSeq_align::eScore_PercentIdentity_Ungapped);

    int gap_count = sb.GetGapCount(align);
    align.SetNamedScore("gap_count", gap_count);

    sb.AddScore(*m_Scope, align, CSeq_align::eScore_PercentCoverage);
    /// High-quality coverage not supported for standard-seg alignments; calculate it for all others
    if(align.GetSegs().Which() != CSeq_align::TSegs::e_Std)
        sb.AddScore(*m_Scope, align, CSeq_align::eScore_HighQualityPercentCoverage);
}

struct SAlignSortByQueryId
{
    bool operator()(const CRef<CSeq_align>& al1,
                    const CRef<CSeq_align>& al2) const
    {
        {{
            const CSeq_id& id1 = al1->GetSeq_id(0);
            const CSeq_id& id2 = al2->GetSeq_id(0);
            TIntId result = id1.CompareOrdered(id2);
            if (result) return result < 0;
        }}

        {{
            const CSeq_id& id1 = al1->GetSeq_id(1);
            const CSeq_id& id2 = al2->GetSeq_id(1);
            TIntId result = id1.CompareOrdered(id2);
            if (result) return result < 0;
        }}

        {{
            // TSeqPos is positive; subtraction always fits signed version.
            TSignedSeqPos result;
            result = al1->GetSeqStart(0) - al2->GetSeqStart(0);
            if (result) return result < 0;
            result = al1->GetSeqStop(0) - al2->GetSeqStop(0);
            if (result) return result < 0;
            result = al1->GetSeqStart(1) - al2->GetSeqStart(1);
            if (result) return result < 0;
            result = al1->GetSeqStop(1) - al2->GetSeqStop(1);
            return result < 0;
        }}
    }
};

void CAdvancedAlignCleanup::BestPlacement(CSeq_align_set::Tdata &aligns)
{
    CSeq_align_set align_set;
    align_set.Set().splice(align_set.Set().end(), aligns);

    NBestPlacement::Rank(align_set);

    aligns.splice(aligns.end(), align_set.Set());
}

class COrderByQueryStart {
public:
    bool operator()(const CSplign::THitRef &hit1, const CSplign::THitRef &hit2)
        const
    { return hit1->GetQueryMin() < hit2->GetQueryMin(); }
};

bool CAdvancedAlignCleanup::CSplignCompartment::HasRnaCharacteristics()
{
    if (hits.size() < 2) {
        /// A compartment has to have multiple alignments to be considered RNA
        return false;
    }
    sort(hits.begin(), hits.end(), COrderByQueryStart());
    CSplign::THitRef prev_hit = hits.front();
    CRangeCollection<TSeqPos> query_coverage;
    ITERATE (CSplign::THitRefs, hit_it, hits) {
        if ((*hit_it)->GetSubjStrand() != prev_hit->GetSubjStrand()) {
            /// Inconsistent strands; not RNA compartmkent
            return false;
        }
        if ((*hit_it)->GetSubjStrand()
          ? ((*hit_it)->GetSubjMin() < prev_hit->GetSubjMin())
          : ((*hit_it)->GetSubjMax() > prev_hit->GetSubjMax()))
        {
            /// Order of hits on subject is not consistent with strand
            return false;
        }

        TSeqRange subject_hit1(prev_hit->GetSubjMin(), prev_hit->GetSubjMax()),
                  subject_hit2((*hit_it)->GetSubjMin(), (*hit_it)->GetSubjMax());
        CRangeCollection<TSeqPos> intron(subject_hit1 + subject_hit2);
        intron -= subject_hit1;
        intron -= subject_hit2;
        if (intron.GetCoveredLength() > s_MaxRnaIntronSize) {
            /// Intron between these two hits is too large
            return false;
        }

        query_coverage += TSeqRange((*hit_it)->GetQueryMin(),
                                    (*hit_it)->GetQueryMax());
        prev_hit = *hit_it;
    }
    
    /// Consider this a RNA compartment is total coverage on query meets threshold
    return query_coverage.GetCoveredLength() * 100
        >= query_length * s_MinRnaTotalCoverage;
}

CAdvancedAlignCleanup::CSplignAlignmentHit::
CSplignAlignmentHit(const CSeq_align &align)
: CBlastTabular(align)
, m_AlignRef(const_cast<CSeq_align *>(&align))
{
}

CAdvancedAlignCleanup::CSplignAlignmentHit::
~CSplignAlignmentHit()
{
}




END_NCBI_SCOPE


