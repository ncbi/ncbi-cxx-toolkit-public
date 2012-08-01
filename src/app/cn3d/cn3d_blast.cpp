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
* Authors:  Paul Thiessen
*
* File Description:
*      module for aligning with BLAST and related algorithms
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>

#include <objmgr/object_manager.hpp>
#include <algo/blast/api/psibl2seq.hpp>
#include <algo/blast/api/objmgr_query_data.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seqalign/Dense_seg.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#include "remove_header_conflicts.hpp"

#include "cn3d_blast.hpp"
#include "block_multiple_alignment.hpp"
#include "cn3d_pssm.hpp"
#include "sequence_set.hpp"
#include "cn3d_tools.hpp"
#include "structure_set.hpp"
#include "molecule_identifier.hpp"
#include "asn_reader.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

class TruncatedSequence : public CObject
{
public:
    const Sequence *originalFullSequence;
    CRef < CSeq_entry > truncatedSequence;
    int fromIndex, toIndex;
};

typedef vector < CRef < TruncatedSequence > > TruncatedSequences;

static CRef < TruncatedSequence > CreateTruncatedSequence(const BlockMultipleAlignment *multiple,
    const BlockMultipleAlignment *pair, int alnNum, bool isMaster, int extension)
{
    CRef < TruncatedSequence > ts(new TruncatedSequence);

    // master sequence (only used for blast-two-sequences)
    if (isMaster) {

        ts->originalFullSequence = pair->GetMaster();
        BlockMultipleAlignment::UngappedAlignedBlockList uaBlocks;

        // use alignMasterTo/From if present and reasonable
        if (pair->alignMasterFrom >= 0 && pair->alignMasterFrom < (int)ts->originalFullSequence->Length() &&
            pair->alignMasterTo >= 0 && pair->alignMasterTo < (int)ts->originalFullSequence->Length() &&
            pair->alignMasterFrom <= pair->alignMasterTo)
        {
            ts->fromIndex = pair->alignMasterFrom;
            ts->toIndex = pair->alignMasterTo;
        }

        // use aligned footprint + extension if multiple has any aligned blocks
        else if (multiple && multiple->GetUngappedAlignedBlocks(&uaBlocks) > 0)
        {
            ts->fromIndex = uaBlocks.front()->GetRangeOfRow(0)->from - extension;
            if (ts->fromIndex < 0)
                ts->fromIndex = 0;
            ts->toIndex = uaBlocks.back()->GetRangeOfRow(0)->to + extension;
            if (ts->toIndex >= (int)ts->originalFullSequence->Length())
                ts->toIndex = ts->originalFullSequence->Length() - 1;
        }

        // otherwise, just use the whole sequence
        else {
            ts->fromIndex = 0;
            ts->toIndex = ts->originalFullSequence->Length() - 1;
        }
    }

    // dependent sequence
    else {

        ts->originalFullSequence = pair->GetSequenceOfRow(1);

        // use alignDependentTo/From if present and reasonable
        if (pair->alignDependentFrom >= 0 && pair->alignDependentFrom < (int)ts->originalFullSequence->Length() &&
            pair->alignDependentTo >= 0 && pair->alignDependentTo < (int)ts->originalFullSequence->Length() &&
            pair->alignDependentFrom <= pair->alignDependentTo)
        {
            ts->fromIndex = pair->alignDependentFrom;
            ts->toIndex = pair->alignDependentTo;
        }

        // otherwise, just use the whole sequence
        else {
            ts->fromIndex = 0;
            ts->toIndex = ts->originalFullSequence->Length() - 1;
        }
    }

    // create new Bioseq (contained in a Seq-entry) with the truncated sequence
    ts->truncatedSequence.Reset(new CSeq_entry);
    CBioseq& bioseq = ts->truncatedSequence->SetSeq();
    CRef < CSeq_id > id(new CSeq_id);
    id->SetLocal().SetId(alnNum);
    bioseq.SetId().push_back(id);
    bioseq.SetInst().SetRepr(CSeq_inst::eRepr_raw);
    bioseq.SetInst().SetMol(CSeq_inst::eMol_aa);
    bioseq.SetInst().SetLength(ts->toIndex - ts->fromIndex + 1);
    TRACEMSG("truncated " << ts->originalFullSequence->identifier->ToString()
        << " from " << (ts->fromIndex+1) << " to " << (ts->toIndex+1) << "; length " << bioseq.GetInst().GetLength());
    bioseq.SetInst().SetSeq_data().SetNcbistdaa().Set().resize(ts->toIndex - ts->fromIndex + 1);
    for (int j=ts->fromIndex; j<=ts->toIndex; ++j)
        bioseq.SetInst().SetSeq_data().SetNcbistdaa().Set()[j - ts->fromIndex] =
            LookupNCBIStdaaNumberFromCharacter(ts->originalFullSequence->sequenceString[j]);

    return ts;
}

static inline bool IsLocalID(const CSeq_id& sid, int localID)
{
    return (sid.IsLocal() && (
        (sid.GetLocal().IsStr() && sid.GetLocal().GetStr() == NStr::IntToString(localID)) ||
        (sid.GetLocal().IsId() && sid.GetLocal().GetId() == localID)));
}

static inline bool GetLocalID(const CSeq_id& sid, int *localID)
{
    *localID = kMin_Int;
    if (!sid.IsLocal())
        return false;
    if (sid.GetLocal().IsId())
        *localID = sid.GetLocal().GetId();
    else try {
        *localID = NStr::StringToInt(sid.GetLocal().GetStr());
    } catch (...) {
        return false;
    }
    return true;
}

static inline bool SeqIdMatchesMaster(const CSeq_id& sid, bool usePSSM)
{
    // if blast-sequence-vs-pssm, master will be consensus
    if (usePSSM)
        return (sid.IsLocal() && sid.GetLocal().IsStr() && sid.GetLocal().GetStr() == "consensus");

    // if blast-two-sequences, master will be local id -1
    else
        return IsLocalID(sid, -1);
}

static void MapBlockFromConsensusToMaster(int consensusStart, int dependentStart, int length,
    BlockMultipleAlignment *newAlignment, const BlockMultipleAlignment *multiple)
{
    // get mapping of each position of consensus -> master on this block
    vector < int > masterLoc(length);
    int i;
    for (i=0; i<length; ++i)
        masterLoc[i] = multiple->GetPSSM().MapConsensusToMaster(consensusStart + i);

    UngappedAlignedBlock *subBlock = NULL;
    for (i=0; i<length; ++i) {

        // is this the start of a sub-block?
        if (!subBlock && masterLoc[i] >= 0) {
            subBlock = new UngappedAlignedBlock(newAlignment);
            subBlock->SetRangeOfRow(0, masterLoc[i], masterLoc[i]);
            subBlock->SetRangeOfRow(1, dependentStart + i, dependentStart + i);
            subBlock->width = 1;
        }

        // continue existing sub-block
        if (subBlock) {

            // is this the end of a sub-block?
            if (i == length - 1 ||                      // last position of block
                masterLoc[i + 1] < 0 ||                 // next position is unmapped
                masterLoc[i + 1] != masterLoc[i] + 1)   // next position is discontinuous
            {
                newAlignment->AddAlignedBlockAtEnd(subBlock);
                subBlock = NULL;
            }

            // extend block by one
            else {
                const Block::Range *range = subBlock->GetRangeOfRow(0);
                subBlock->SetRangeOfRow(0, range->from, range->to + 1);
                range = subBlock->GetRangeOfRow(1);
                subBlock->SetRangeOfRow(1, range->from, range->to + 1);
                ++(subBlock->width);
            }
        }
    }

    if (subBlock)
        ERRORMSG("MapBlockFromConsensusToMaster() - unterminated sub-block");
}

static void RemoveAllDataLoaders() {
    CRef<CObjectManager> om = CObjectManager::GetInstance();
    CObjectManager::TRegisteredNames loader_names;
    om->GetRegisteredNames(loader_names);
    ITERATE(CObjectManager::TRegisteredNames, itr, loader_names) {
        om->RevokeDataLoader(*itr);
    }
}

static bool SimpleSeqLocFromBioseq(const CRef< CBioseq>& bs, CSeq_loc& seqLoc)
{
    bool result = true;
    CSeq_interval& seqInt = seqLoc.SetInt();
    CSeq_id& seqId = seqInt.SetId();
    seqInt.SetFrom(0);
    
    //  Assign the first identifier from the bioseq
    if (bs.NotEmpty() && bs->GetFirstId() != 0) {
        seqInt.SetTo(bs->GetLength() - 1);
        seqId.Assign(*(bs->GetFirstId()));
    } else {
        result = false;
    }

    return result;
}

void BLASTer::CreateNewPairwiseAlignmentsByBlast(const BlockMultipleAlignment *multiple,
    const AlignmentList& toRealign, AlignmentList *newAlignments, bool usePSSM)
{
    newAlignments->clear();
    if (usePSSM && (!multiple || multiple->HasNoAlignedBlocks())) {
        ERRORMSG("usePSSM true, but NULL or zero-aligned block multiple alignment");
        return;
    }
    if (!usePSSM && toRealign.size() > 1) {
        ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - currently can only do single blast-2-sequences at a time");
        return;
    }
    if (toRealign.size() == 0)
        return;

    try {
        const Sequence *master = (multiple ? multiple->GetMaster() : NULL);

        int extension = 0;
        if (!RegistryGetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, &extension))
            WARNINGMSG("Can't get footprint residue extension from registry");

        //  Make sure object manager loads only data from our alignment object.
        RemoveAllDataLoaders();
        CRef<CObjectManager> objmgr = CObjectManager::GetInstance();
        CScope scope(*objmgr);
        CRef< CBioseq > queryBioseq, subjectBioseq;
        CRef<CSeq_loc> querySeqLoc(new CSeq_loc);
        blast::CBlastQueryVector queryVector, subjectVector;
        scope.ResetDataAndHistory();

        // collect subject(s) - second sequence of each realignment
        TruncatedSequences subjectTSs;
        int localID = 0;
        AlignmentList::const_iterator a, ae = toRealign.end();
        for (a=toRealign.begin(); a!=ae; ++a, ++localID) {
            if (!master)
                master = (*a)->GetMaster();
            if ((*a)->GetMaster() != master) {
                ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - all masters must be the same");
                return;
            }
            if ((*a)->NRows() != 2) {
                ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - can only realign pairwise alignments");
                return;
            }
            subjectTSs.push_back(CreateTruncatedSequence(multiple, *a, localID, false, extension));

            CRef< CSeq_loc > subjectSeqLoc(new CSeq_loc);
            subjectBioseq = &(subjectTSs.back()->truncatedSequence->SetSeq());
            scope.AddBioseq(*subjectBioseq);
            //  Set up the QueryFactory for the subject sequences
            if (SimpleSeqLocFromBioseq(subjectBioseq, *subjectSeqLoc)) {
                CRef< blast::CBlastSearchQuery > bsqSubject(new blast::CBlastSearchQuery(*subjectSeqLoc, scope));
                subjectVector.AddQuery(bsqSubject);
            }

        }
        CRef < blast::IQueryFactory > sequenceSubjects(new blast::CObjMgr_QueryFactory(subjectVector));

        // main blast engine
        CRef < blast::CPsiBl2Seq > blastEngine;

        // setup searches: blast-sequence-vs-pssm
        CRef < CPssmWithParameters > pssmQuery;
        CRef < blast::CPSIBlastOptionsHandle > pssmOptions;
        if (usePSSM) {
            pssmQuery.Reset(new CPssmWithParameters);
            pssmQuery->Assign(multiple->GetPSSM().GetPSSM());
            pssmOptions.Reset(new blast::CPSIBlastOptionsHandle);

            // NR stats at 3/21/2006
            pssmOptions->SetDbLength(1196146007);
            pssmOptions->SetDbSeqNum(3479934);
            pssmOptions->SetHitlistSize(subjectTSs.size());
            pssmOptions->SetMatrixName("BLOSUM62");
            pssmOptions->SetCompositionBasedStats(eCompositionBasedStats);
            pssmOptions->SetSegFiltering(false);

            blastEngine.Reset(new
                blast::CPsiBl2Seq(
                    pssmQuery,
                    sequenceSubjects,
                    CConstRef < blast::CPSIBlastOptionsHandle > (pssmOptions.GetPointer())));
        }

        // setup searches: blast-two-sequences
        CRef < TruncatedSequence > masterTS;
        CRef < blast::IQueryFactory > sequenceQuery;
        CRef < blast::CBlastProteinOptionsHandle > sequenceOptions;
        if (!usePSSM) {
            masterTS = CreateTruncatedSequence(multiple, toRealign.front(), -1, true, extension);

            //  Set up a QueryFactory for the query sequence
            queryBioseq = &(masterTS->truncatedSequence->SetSeq());
            scope.AddBioseq(*queryBioseq);
            if (SimpleSeqLocFromBioseq(queryBioseq, *querySeqLoc)) {
                CRef< blast::CBlastSearchQuery> bsqQuery(new blast::CBlastSearchQuery(*querySeqLoc, scope));
                queryVector.AddQuery(bsqQuery);
            }
            sequenceQuery.Reset(new blast::CObjMgr_QueryFactory(queryVector));

            sequenceOptions.Reset(new blast::CBlastProteinOptionsHandle);
            sequenceOptions->SetMatrixName("BLOSUM62");
            sequenceOptions->SetHitlistSize(subjectTSs.size());
            blastEngine.Reset(new
                blast::CPsiBl2Seq(
                    sequenceQuery,
                    sequenceSubjects,
                    CConstRef < blast::CBlastProteinOptionsHandle > (sequenceOptions.GetPointer())));
        }

        // actually do the alignment(s)
        CRef < blast::CSearchResultSet > results(blastEngine->Run());

        // parse the alignments
        if (results->size() != toRealign.size())
        {
            ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - did not get one result alignment per input sequence");
            return;
        }

        localID = 0;
        for (unsigned int i=0; i<results->size(); ++i, ++localID) {

//            string err;
//            WriteASNToFile("Seq-align-set.txt", (*results)[i].GetSeqAlign().GetObject(), false, &err);

            // create new alignment structure
            BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
            (*seqs)[0] = master;
            (*seqs)[1] = subjectTSs[localID]->originalFullSequence;
            string dependentTitle = subjectTSs[localID]->originalFullSequence->identifier->ToString();
            auto_ptr < BlockMultipleAlignment > newAlignment(
                new BlockMultipleAlignment(seqs, master->parentSet->alignmentManager));
            newAlignment->SetRowDouble(0, kMax_Double);
            newAlignment->SetRowDouble(1, kMax_Double);

            // check for valid or empty alignment
            if (!((*results)[i].HasAlignments())) {
                WARNINGMSG("BLAST did not find a significant alignment for "
                    << dependentTitle << " with " << (usePSSM ? string("PSSM") : master->identifier->ToString()));
            } else {

                // get Seq-align; use first one for this result, which assumes blast returns the highest scoring alignment first
                const CSeq_align& sa = (*results)[i].GetSeqAlign()->Get().front().GetObject();

                if (!sa.IsSetDim() || sa.GetDim() != 2 || sa.GetType() != CSeq_align::eType_partial) {
                    ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - returned alignment not in expected format (dim 2, partial)");
                } else if (sa.GetSegs().IsDenseg()) {

                    // unpack Dense-seg
                    const CDense_seg& ds = sa.GetSegs().GetDenseg();
                    if (!ds.IsSetDim() || ds.GetDim() != 2 || ds.GetIds().size() != 2 ||
                            (int)ds.GetLens().size() != ds.GetNumseg() || (int)ds.GetStarts().size() != 2 * ds.GetNumseg()) {
                        ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - returned alignment format error (denseg dims)");
                    } else if (!SeqIdMatchesMaster(ds.GetIds().front().GetObject(), usePSSM) ||
                               !IsLocalID(ds.GetIds().back().GetObject(), localID)) {
                        ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - returned alignment format error (ids)");
                    } else {

                        // unpack segs
                        CDense_seg::TStarts::const_iterator s = ds.GetStarts().begin();
                        CDense_seg::TLens::const_iterator l, le = ds.GetLens().end();
                        for (l=ds.GetLens().begin(); l!=le; ++l) {
                            int masterStart = *(s++), dependentStart = *(s++);
                            if (masterStart >= 0 && dependentStart >= 0) {  // skip gaps
                                dependentStart += subjectTSs[localID]->fromIndex;

                                if (usePSSM) {
                                    MapBlockFromConsensusToMaster(masterStart, dependentStart, *l, newAlignment.get(), multiple);
                                } else {
                                    masterStart += masterTS->fromIndex;
                                    UngappedAlignedBlock *newBlock = new UngappedAlignedBlock(newAlignment.get());
                                    newBlock->SetRangeOfRow(0, masterStart, masterStart + (*l) - 1);
                                    newBlock->SetRangeOfRow(1, dependentStart, dependentStart + (*l) - 1);
                                    newBlock->width = *l;
                                    newAlignment->AddAlignedBlockAtEnd(newBlock);
                                }
                            }
                        }
                    }

                } else {
                    ERRORMSG("CreateNewPairwiseAlignmentsByBlast() - returned alignment in unrecognized format");
                }

                // unpack score
                if (!sa.IsSetScore() || sa.GetScore().size() == 0) {
                    WARNINGMSG("BLAST did not return an alignment score for " << dependentTitle);
                } else {
                    CNcbiOstrstream oss;
                    oss << "BLAST result scores for " << dependentTitle << " vs. "
                        << (usePSSM ? string("PSSM") : master->identifier->ToString()) << ':';

                    bool haveE = false;
                    CSeq_align::TScore::const_iterator sc, sce = sa.GetScore().end();
                    for (sc=sa.GetScore().begin(); sc!=sce; ++sc) {
                        if ((*sc)->IsSetId() && (*sc)->GetId().IsStr()) {

                            // E-value (put in status line and double values)
                            if ((*sc)->GetValue().IsReal() && (*sc)->GetId().GetStr() == "e_value") {
                                haveE = true;
                                newAlignment->SetRowDouble(0, (*sc)->GetValue().GetReal());
                                newAlignment->SetRowDouble(1, (*sc)->GetValue().GetReal());
                                string status = string("E-value: ") + NStr::DoubleToString((*sc)->GetValue().GetReal());
                                newAlignment->SetRowStatusLine(0, status);
                                newAlignment->SetRowStatusLine(1, status);
                                oss << ' ' << status;
                            }

                            // raw score
                            else if ((*sc)->GetValue().IsInt() && (*sc)->GetId().GetStr() == "score") {
                                oss << " raw: " << (*sc)->GetValue().GetInt();
                            }

                            // bit score
                            else if ((*sc)->GetValue().IsReal() && (*sc)->GetId().GetStr() == "bit_score") {
                                oss << " bit score: " << (*sc)->GetValue().GetReal();
                            }
                        }
                    }

                    INFOMSG((string) CNcbiOstrstreamToString(oss));
                    if (!haveE)
                        WARNINGMSG("BLAST did not return an E-value for " << dependentTitle);
                }
            }

            // finalize and and add new alignment to list
            if (newAlignment->AddUnalignedBlocks() && newAlignment->UpdateBlockMapAndColors(false))
                newAlignments->push_back(newAlignment.release());
            else
                ERRORMSG("error finalizing alignment");
        }

    } catch (exception& e) {
        ERRORMSG("CreateNewPairwiseAlignmentsByBlast() failed with exception: " << e.what());
    }
}

void BLASTer::CalculateSelfHitScores(const BlockMultipleAlignment *multiple)
{
    if (!multiple) {
        ERRORMSG("NULL multiple alignment");
        return;
    }

    int extension = 0;
    if (!RegistryGetInteger(REG_ADVANCED_SECTION, REG_FOOTPRINT_RES, &extension))
        WARNINGMSG("Can't get footprint residue extension from registry");

    BlockMultipleAlignment::UngappedAlignedBlockList uaBlocks;
    multiple->GetUngappedAlignedBlocks(&uaBlocks);
    if (uaBlocks.size() == 0) {
        ERRORMSG("Can't calculate self-hits with no aligned blocks");
        return;
    }

    // do BLAST-vs-pssm on all rows, using footprint for each row
    AlignmentList rowPairs;
    unsigned int row;
    for (row=0; row<multiple->NRows(); ++row) {
        BlockMultipleAlignment::SequenceList *seqs = new BlockMultipleAlignment::SequenceList(2);
        (*seqs)[0] = multiple->GetMaster();
        (*seqs)[1] = multiple->GetSequenceOfRow(row);
        BlockMultipleAlignment *newAlignment = new BlockMultipleAlignment(seqs, multiple->GetMaster()->parentSet->alignmentManager);
        const Block::Range *range = uaBlocks.front()->GetRangeOfRow(row);
        newAlignment->alignDependentFrom = range->from - extension;
        if (newAlignment->alignDependentFrom < 0)
            newAlignment->alignDependentFrom = 0;
        range = uaBlocks.back()->GetRangeOfRow(row);
        newAlignment->alignDependentTo = range->to + extension;
        if (newAlignment->alignDependentTo >= (int)multiple->GetSequenceOfRow(row)->Length())
            newAlignment->alignDependentTo = multiple->GetSequenceOfRow(row)->Length() - 1;
        rowPairs.push_back(newAlignment);
    }
    AlignmentList results;
    CreateNewPairwiseAlignmentsByBlast(multiple, rowPairs, &results, true);
    DELETE_ALL_AND_CLEAR(rowPairs, AlignmentList);
    if (results.size() != multiple->NRows()) {
        DELETE_ALL_AND_CLEAR(results, AlignmentList);
        ERRORMSG("CalculateSelfHitScores() - CreateNewPairwiseAlignmentsByBlast() didn't return right # alignments");
        return;
    }

    // extract scores, assumes E-value is in RowDouble
    AlignmentList::const_iterator r = results.begin();
    for (row=0; row<multiple->NRows(); ++row, ++r) {
        double score = (*r)->GetRowDouble(1);
        multiple->SetRowDouble(row, score);
        string status;
        if (score >= 0.0 && score < kMax_Double)
            status = string("Self hit E-value: ") + NStr::DoubleToString(score);
        else
            status = "No detectable self hit";
        multiple->SetRowStatusLine(row, status);
    }
    DELETE_ALL_AND_CLEAR(results, AlignmentList);

    // print out overall self-hit rate
    static const double threshold = 0.01;
    unsigned int nSelfHits = 0;
    for (row=0; row<multiple->NRows(); ++row) {
        if (multiple->GetRowDouble(row) >= 0.0 && multiple->GetRowDouble(row) <= threshold)
            ++nSelfHits;
    }
    INFOMSG("Self hits with E-value <= " << setprecision(3) << threshold << ": "
        << (100.0*nSelfHits/multiple->NRows()) << "% ("
        << nSelfHits << '/' << multiple->NRows() << ')' << setprecision(6));
}

double GetStandardProbability(char ch)
{
    typedef map < char, double > CharDoubleMap;
    static CharDoubleMap standardProbabilities;

    if (standardProbabilities.size() == 0) {  // initialize static stuff
        if (BLASTAA_SIZE != 28) {
            ERRORMSG("GetStandardProbability() - confused by BLASTAA_SIZE != 28");
            return 0.0;
        }
        double *probs = BLAST_GetStandardAaProbabilities();
        for (unsigned int i=0; i<28; ++i) {
            standardProbabilities[LookupCharacterFromNCBIStdaaNumber(i)] = probs[i];
//            TRACEMSG("standard probability " << LookupCharacterFromNCBIStdaaNumber(i) << " : " << probs[i]);
        }
        sfree(probs);
    }

    CharDoubleMap::const_iterator f = standardProbabilities.find(toupper((unsigned char) ch));
    if (f != standardProbabilities.end())
        return f->second;
    WARNINGMSG("GetStandardProbability() - unknown residue character " << ch);
    return 0.0;
}

END_SCOPE(Cn3D)
