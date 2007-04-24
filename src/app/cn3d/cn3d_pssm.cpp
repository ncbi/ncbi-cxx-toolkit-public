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
*      new C++ PSSM construction
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>
#include <serial/objostrasn.hpp>

#include <objects/scoremat/scoremat__.hpp>

#include <algo/structure/cd_utils/cuCdCore.hpp>
#include <algo/structure/cd_utils/cuPssmMaker.hpp>

#include "remove_header_conflicts.hpp"

#include "cn3d_pssm.hpp"
#include "block_multiple_alignment.hpp"
#include "sequence_set.hpp"
#include "cn3d_tools.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);


BEGIN_SCOPE(Cn3D)

//#define DEBUG_PSSM 1 // for testing/debugging PSSM data

#define PTHROW(stream) NCBI_THROW(CException, eUnknown, stream)

PSSMWrapper::PSSMWrapper(const BlockMultipleAlignment *bma) : multiple(bma)
{
#ifdef DEBUG_PSSM
    {{
        CNcbiOfstream ofs("pssm.txt", IOS_BASE::out);
    }}
#endif

    try {
        TRACEMSG("Creating PSSM...");

        // construct a "fake" CD to pass to PssmMaker
        cd_utils::CCdCore c;
        c.SetName("fake");

        // construct Seq-entry from sequences in current alignment
        CRef < CSeq_entry > seq(new CSeq_entry);
        seq->SetSeq().Assign(multiple->GetMaster()->bioseqASN.GetObject());
        c.SetSequences().SetSet().SetSeq_set().push_back(seq);

        // construct Seq-annot from rows in the alignment
        c.SetSeqannot().push_back(CRef<CSeq_annot>(new CSeq_annot));
        BlockMultipleAlignment::UngappedAlignedBlockList blocks;
        bma->GetUngappedAlignedBlocks(&blocks);

        // fill out Seq-entry and Seq-annot based on BMA (row order is irrelevant here)
        for (unsigned int i=((bma->NRows() > 1) ? 1 : 0); i<bma->NRows(); ++i) {
            seq.Reset(new CSeq_entry);
            seq->SetSeq().Assign(multiple->GetSequenceOfRow(i)->bioseqASN.GetObject());
            c.SetSequences().SetSet().SetSeq_set().push_back(seq);
            CRef < CSeq_align > seqAlign(CreatePairwiseSeqAlignFromMultipleRow(bma, blocks, i));
            c.SetSeqannot().front()->SetData().SetAlign().push_back(seqAlign);
        }

        // use PssmMaker to create PSSM using consensus
        cd_utils::PssmMaker pm(&c, true, true);
        cd_utils::PssmMakerOptions options;     // comes with defaults
        options.requestFrequencyRatios = true;  // necessary for psi-blast
//        options.scalingFactor = 100;          // do *NOT* use SF other than 1 for psi-blast
        pm.setOptions(options);
        pssm = pm.make();

        // blast functions require a master (query) sequence to be present; give it a recognizable id
        if (!pssm->GetPssm().IsSetQuery() || !pssm->GetPssm().GetQuery().IsSeq())
            PTHROW("PssmWithParameters from cd_utils::PssmMaker() doesn't contain the master/query sequence");
        CRef < CSeq_id > id(new CSeq_id);
        id->SetLocal().SetStr("consensus");
        pssm->SetPssm().SetQuery().SetSeq().SetId().push_front(id);

        // for efficient score lookup
        UnpackMatrix(pm);

#ifdef DEBUG_PSSM
        CNcbiOfstream ofs("pssm.txt", IOS_BASE::out | IOS_BASE::app);
        if (ofs) {
            CObjectOStreamAsn oosa(ofs, false);
            oosa << *pssm;

/*
            if (pssm->GetPssm().IsSetIntermediateData() && pssm->GetPssm().GetIntermediateData().IsSetResFreqsPerPos()) {
                vector < int >
                    freqs(pssm->GetPssm().GetIntermediateData().GetResFreqsPerPos().size()),
                    nNonGap(pssm->GetPssm().GetNumColumns(), 0);
                unsigned int i;
                CPssmIntermediateData::TResFreqsPerPos::const_iterator
                    l = pssm->GetPssm().GetIntermediateData().GetResFreqsPerPos().begin();
                for (i=0; i<pssm->GetPssm().GetIntermediateData().GetResFreqsPerPos().size(); ++i, ++l)
                    freqs[i] = *l;
                int freq, n;
                ofs << "observed frequencies:\n";
                for (unsigned int c=0; c<pssm->GetPssm().GetNumColumns(); ++c) {
                    ofs << "column " << (c+1) << ": ";
                    n = 0;
                    for (unsigned int r=0; r<pssm->GetPssm().GetNumRows(); ++r) {
                        if (pssm->GetPssm().GetByRow())
                            freq = freqs[r * pssm->GetPssm().GetNumColumns() + c];
                        else
                            freq = freqs[c * pssm->GetPssm().GetNumRows() + r];
                        if (freq > 0) {
                            ofs << LookupCharacterFromNCBIStdaaNumber(r) << '(' << freq << ") ";
                            n += freq;
                            if (r != 0)
                                nNonGap[c] += freq;
                        }
                    }
                    ofs << "total: " << n << " non-gap: " << nNonGap[c] << '\n';
                }

                if (pssm->GetPssm().IsSetIntermediateData() && pssm->GetPssm().GetIntermediateData().IsSetWeightedResFreqsPerPos()) {
                    vector < double > wfreqs(pssm->GetPssm().GetIntermediateData().GetWeightedResFreqsPerPos().size());
                    CPssmIntermediateData::TWeightedResFreqsPerPos::const_iterator
                        m = pssm->GetPssm().GetIntermediateData().GetWeightedResFreqsPerPos().begin();
                    for (i=0; i<pssm->GetPssm().GetIntermediateData().GetWeightedResFreqsPerPos().size(); ++i, ++m)
                        wfreqs[i] = *m;
                    double wfreq, s;
                    ofs << "weighted frequencies:\n";
                    for (unsigned int c=0; c<pssm->GetPssm().GetNumColumns(); ++c) {
                        ofs << "column " << (c+1) << ": ";
                        s = 0.0;
                        for (unsigned int r=0; r<pssm->GetPssm().GetNumRows(); ++r) {
                            if (pssm->GetPssm().GetByRow())
                                wfreq = wfreqs[r * pssm->GetPssm().GetNumColumns() + c];
                            else
                                wfreq = wfreqs[c * pssm->GetPssm().GetNumRows() + r];
                            if (wfreq != 0.0) {
                                ofs << LookupCharacterFromNCBIStdaaNumber(r) << '(' << wfreq << ") ";
                                s += wfreq;
                            }
                        }
                        ofs << "sum: " << s << '\n';
                    }
                }

                if (pssm->GetPssm().IsSetIntermediateData() && pssm->GetPssm().GetIntermediateData().IsSetFreqRatios()) {
                    vector < double > ratios(pssm->GetPssm().GetIntermediateData().GetFreqRatios().size());
                    CPssmIntermediateData::TFreqRatios::const_iterator
                        n = pssm->GetPssm().GetIntermediateData().GetFreqRatios().begin();
                    for (i=0; i<pssm->GetPssm().GetIntermediateData().GetFreqRatios().size(); ++i, ++n)
                        ratios[i] = *n;
                    double ratio, s;
                    ofs << "frequency ratios:\n";
                    for (unsigned int c=0; c<pssm->GetPssm().GetNumColumns(); ++c) {
                        ofs << "column " << (c+1) << ": ";
                        s = 0.0;
                        for (unsigned int r=0; r<pssm->GetPssm().GetNumRows(); ++r) {
                            if (pssm->GetPssm().GetByRow())
                                ratio = ratios[r * pssm->GetPssm().GetNumColumns() + c];
                            else
                                ratio = ratios[c * pssm->GetPssm().GetNumRows() + r];
                            if (ratio != 0.0) {
                                ofs << LookupCharacterFromNCBIStdaaNumber(r) << '(' << ratio << ") ";
                                s += ratio;
                            }
                        }
                        ofs << "sum: " << s << '\n';
                    }
                }
            }
*/
        }
#endif

    } catch (exception& e) {
        ERRORMSG("PSSMWrapper::PSSMWrapper() failed with exception: " << e.what());
    } catch (...) {
        ERRORMSG("PSSMWrapper::PSSMWrapper() failed with unknown exception");
    }
}

void PSSMWrapper::UnpackMatrix(ncbi::cd_utils::PssmMaker& pm)
{
    if (!pssm->GetPssm().IsSetFinalData())
        PTHROW("UnpackMatrix() - pssm must have finalData");
    unsigned int nScores = pssm->GetPssm().GetNumRows() * pssm->GetPssm().GetNumColumns();
    if (pssm->GetPssm().GetNumRows() != 28 || pssm->GetPssm().GetFinalData().GetScores().size() != nScores)
        PTHROW("UnpackMatrix() - bad matrix size");

    scalingFactor = pssm->GetPssm().GetFinalData().GetScalingFactor();

    // allocate matrix
    unsigned int i;
    scaledMatrix.resize(pssm->GetPssm().GetNumColumns());
    for (i=0; (int)i<pssm->GetPssm().GetNumColumns(); ++i)
        scaledMatrix[i].resize(28);

    // convert matrix
    unsigned int r = 0, c = 0;
    CPssmFinalData::TScores::const_iterator s = pssm->GetPssm().GetFinalData().GetScores().begin();
    for (i=0; i<nScores; ++i, ++s) {

        scaledMatrix[c][r] = *s;

        // adjust for matrix layout in pssm
        if (pssm->GetPssm().GetByRow()) {
            ++c;
            if ((int)c == pssm->GetPssm().GetNumColumns()) {
                ++r;
                c = 0;
            }
        } else {
            ++r;
            if ((int)r == pssm->GetPssm().GetNumRows()) {
                ++c;
                r = 0;
            }
        }
    }

    // map multiple's master <-> consensus position
    if ((int)pm.getConsensus().size() != pssm->GetPssm().GetNumColumns())
        PTHROW("Consensus sequence does not match PSSM size");
    TRACEMSG("master length: " << multiple->GetMaster()->Length() << ", consensus length: " << pm.getConsensus().size());
    cd_utils::BlockModelPair bmp(pm.getGuideAlignment());   // consensus is dependent
    consensus2master.resize(pm.getConsensus().size());
    for (i=0; i<pm.getConsensus().size(); ++i)
        consensus2master[i] = bmp.mapToMaster(i);
    bmp.reverse();  // so that master is consensus, dependent is multiple's master
    master2consensus.resize(multiple->GetMaster()->Length());
    for (i=0; i<multiple->GetMaster()->Length(); ++i)
        master2consensus[i] = bmp.mapToMaster(i);
}

void PSSMWrapper::OutputPSSM(ncbi::CNcbiOstream& os, const string& title) const
{
    // create a copy of the pssm, massaged a bit so that it'll work correctly with psi-blast, rps-blast
    CPssmWithParameters copy;
    copy.Assign(*pssm);
    if (!copy.GetPssm().IsSetQuery() || !copy.GetPssm().GetQuery().IsSeq()) {
        ERRORMSG("PssmWithParameters from cd_utils::PssmMaker() doesn't contain the master/query sequence");
        return;
    }

    CBioseq::TId keep;
    CBioseq::TId::iterator i, ie = copy.SetPssm().SetQuery().SetSeq().SetId().end();
    for (i=copy.SetPssm().SetQuery().SetSeq().SetId().begin(); i!=ie; ++i) {
        if ((*i)->IsLocal() && (*i)->GetLocal().IsStr())
            (*i)->SetLocal().SetStr(title);
        if (!(*i)->IsGeneral() || (*i)->GetGeneral().GetDb() != "Cdd")
            keep.push_back(*i);
    }
    copy.SetPssm().SetQuery().SetSeq().SetId() = keep;

    CSeq_descr::Tdata::iterator d, de = copy.SetPssm().SetQuery().SetSeq().SetDescr().Set().end();
    for (d=copy.SetPssm().SetQuery().SetSeq().SetDescr().Set().begin(); d!=de; ++d) {
        if ((*d)->IsTitle()) {
            (*d)->SetTitle(title);
            break;
        }
    }
    if (d == de) {
        CRef < CSeqdesc > descr(new CSeqdesc);
        descr->SetTitle(title);
        copy.SetPssm().SetQuery().SetSeq().SetDescr().Set().push_front(descr);
    }

    // do not put scores in output pssm, only freq ratios
    copy.SetPssm().ResetFinalData();
    if (!copy.GetPssm().IsSetIntermediateData() || !copy.GetPssm().GetIntermediateData().IsSetFreqRatios())
        ERRORMSG("PSSM is missing frequency ratios");

    CObjectOStreamAsn osa(os, false);
    osa << copy;
}

static inline int Round(double Num)
{
  if (Num >= 0)
    return((int)(Num + 0.5));
  else
    return((int)(Num - 0.5));
}

int PSSMWrapper::GetPSSMScore(unsigned char ncbistdaa, unsigned int realMasterIndex) const
{
    if (ncbistdaa >= 28 || realMasterIndex > multiple->GetMaster()->Length()) {
        ERRORMSG("PSSMWrapper::GetPSSMScore() - invalid parameters");
        return kMin_Int;
    }

    // maps to a position in the consensus/pssm
    int consensusIndex = master2consensus[realMasterIndex];
    if (consensusIndex >= 0) {
        double scaledScore;
        switch (ncbistdaa) {
            case 2:  // B -> average D/N
                scaledScore = ((double) (scaledMatrix[consensusIndex][4] + scaledMatrix[consensusIndex][13])) / 2;
                break;
            case 23: // Z -> average E/Q
                scaledScore = ((double) (scaledMatrix[consensusIndex][5] + scaledMatrix[consensusIndex][15])) / 2;
                break;
            case 24: // U -> C
                scaledScore = scaledMatrix[consensusIndex][3];
                break;
            default:
                scaledScore = scaledMatrix[consensusIndex][ncbistdaa];
        }
        return Round(scaledScore / scalingFactor);
    }

    // use simple blosum62 score if outside the consensus/pssm
    return GetBLOSUM62Score(LookupCharacterFromNCBIStdaaNumber(ncbistdaa), multiple->GetMaster()->sequenceString[realMasterIndex]);
}

END_SCOPE(Cn3D)
