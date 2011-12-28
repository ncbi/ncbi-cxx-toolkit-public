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
 * Authors:  Chris Lanczycki
 *
 * File Description:
 *           Use data from a Fasta I/O object to construct a CSeq_annot
 *           intended for eventual installation in a CCdd-derived object.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <serial/serial.hpp>

#include <serial/objistrasn.hpp>
#include <serial/objostrasn.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>

#include <objects/cdd/Cdd.hpp>
#include <objects/cdd/Update_align.hpp>
#include <objects/cdd/Update_comment.hpp>
#include <objects/cdd/Cdd_id.hpp>
#include <objects/cdd/Cdd_id_set.hpp>
#include <objects/cdd/Global_id.hpp>

#include <objects/seq/Bioseq.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/ncbimime/Biostruc_seq.hpp>
#include <objects/mmdb2/Model_type.hpp>
#include <objects/mmdb2/Biostruc_model.hpp>
#include <objects/mmdb1/Biostruc_graph.hpp>
#include <objects/mmdb1/Biostruc_descr.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/mmdb1/Biostruc_id.hpp>
#include <objects/mmdb1/Mmdb_id.hpp>
#include <objects/mmdb1/Biostruc_annot_set.hpp>
#include <objects/mmdb3/Biostruc_feature_set.hpp>
#include <objects/mmdb3/Chem_graph_alignment.hpp>
#include <objects/mmdb3/Chem_graph_pntrs.hpp>
#include <objects/mmdb3/Residue_pntrs.hpp>
#include <objects/mmdb3/Residue_interval_pntr.hpp>
#include <objects/mmdb1/Molecule_id.hpp>
#include <objects/mmdb1/Residue_id.hpp>
#include <objects/mmdb3/Biostruc_feature_set_id.hpp>
#include <objects/mmdb3/Biostruc_feature_id.hpp>

#include <algo/structure/cd_utils/cuCdReadWriteASN.hpp>
#include <algo/structure/cd_utils/cuSequence.hpp>

#include <algo/structure/cd_utils/cuSeqAnnotFromFasta.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

bool CSeqAnnotFromFasta::isNotAlpha(char c) {
    return !isalpha(c);
}

void CSeqAnnotFromFasta::CountNonAlphaToPositions(const vector<unsigned int>& positions, const string& sequence, map<unsigned int, unsigned int>& numNonAlpha) {

    unsigned char aa;
    unsigned int len = sequence.length();
    unsigned int nGaps = 0;
    vector<unsigned int>::const_iterator posBeg = positions.begin(), posEnd = positions.end();

    numNonAlpha.clear();
    for (unsigned int i = 0; i < len; ++i) {
        if (find(posBeg, posEnd, i) != posEnd) {
            numNonAlpha[i] = nGaps;
        }

        aa = toupper((unsigned char) sequence[i]);
        if (!isalpha(aa)) {
            ++nGaps;
        }
    }
    numNonAlpha[len] = nGaps;

}

bool PurgeNonAlpha(string& s) {
    bool result = false;
    if (s.length() > 0 && find_if(s.begin(), s.end(), CSeqAnnotFromFasta::isNotAlpha) != s.end()) {

        s.erase(remove_if(s.begin(), s.end(), CSeqAnnotFromFasta::isNotAlpha), s.end());
        result = true;
    }
    return result;
}

CSeqAnnotFromFasta::CSeqAnnotFromFasta(bool doIbm, bool preferStructureMaster, bool caseSensitive) : m_doIBM(doIbm), m_preferStructureMaster(preferStructureMaster), m_caseSensitive(caseSensitive)
{
    m_masterIndex = (unsigned int) eUnassignedMaster;
}

bool CSeqAnnotFromFasta::IsSeqAnnotValid() const
{
    return (m_seqAnnot.NotEmpty());
}

string CSeqAnnotFromFasta::GetSequence(unsigned int index) const
{
    return (index < m_sequences.size()) ? m_sequences[index] : "";
}


bool CSeqAnnotFromFasta::MakeSeqAnnotFromFasta(CNcbiIstream& is, CFastaIOWrapper& fastaIO, MasteringMethod masterMethod, unsigned int masterIndex)
{

    // create simply to use CCdCore methods.
    CCdCore dummyCD;  
    bool builtIt = false;

    if (!fastaIO.ReadFile(is) || !fastaIO.HasSeqEntry()) {
        return builtIt;
    }

    CRef<CSeq_entry> seqEntry(new CSeq_entry);
    seqEntry->Assign(*fastaIO.GetSeqEntry());

    //  Need to make a new accession object since none exists.
    if (dummyCD.GetId().Get().size() == 0) {
        CRef< CCdd_id > cdId(new CCdd_id());
        CRef< CGlobal_id > global(new CGlobal_id());
        global->SetAccession("tempCDAcc");
        global->SetVersion(0);
        cdId->SetGid(*global);
        dummyCD.SetId().Set().push_back(cdId);
    } else {
        dummyCD.SetAccession("tempCDAcc");
    }
    dummyCD.SetName("tempCDName");
    dummyCD.SetSequences(*seqEntry);


    //  Make seq-annot to hold the seq-aligns.
    m_seqAnnot.Reset(new CSeq_annot());
    if (m_seqAnnot.Empty()) return false;
    m_seqAnnot->SetData().Select(CSeq_annot::C_Data::e_Align);

    //  Set the master index.
    if (masterMethod == eSpecifiedSequence) {
        if (masterIndex == (unsigned int) eUnassignedMaster || masterIndex >= (unsigned int) dummyCD.GetNumSequences()) {
            return false;
        } else { 
            //cout << "Alignment master is row " << masterIndex << endl;
            m_masterIndex = masterIndex;
        }
    }
    DetermineMasterIndex(dummyCD, masterMethod);
//    unsigned int testMasterIndex = 0;
//    testMasterIndex = DetermineMasterIndex(dummyCD, masterMethod);
//    cout << "Using sequence " << testMasterIndex+1 << " from input mFASTA as the alignment master.\n";

    //  Fill in all of the seq-aligns.
    if (m_doIBM) {
        builtIt = MakeIBMSeqAnnot(dummyCD);
    } else {
        builtIt = MakeAsIsSeqAnnot(dummyCD);
    }

    if (!builtIt) m_seqAnnot.Reset();

    return builtIt;
}

bool CSeqAnnotFromFasta::MakeIBMSeqAnnot(CCdCore& dummyCD)
{

    bool builtIt = false;
    unsigned int i, j, len, maxLen;
    unsigned int nSeq = (unsigned int) dummyCD.GetNumSequences(), maxLenIndex = 0;
    unsigned char uc;
    string masterSequence, sequence;
    vector<unsigned int> residueCount;  
    set<unsigned int> masterSeqIndices, forcedBreaks;  

    typedef map<unsigned int, vector<unsigned int> > SeqStartMap;
    typedef SeqStartMap::iterator SeqStartIt;
    typedef SeqStartMap::value_type SeqStartVT;

    unsigned int nBlocks;
    vector<unsigned int> blockStarts;
    vector<unsigned int> blockLengths;

    if (nSeq == 0) {
        return false;
    }

    //  Find the longest sequence; cache the sequence strings (which may have 
    //  gap characters at this point).
    CacheSequences(dummyCD, maxLenIndex, false);
    if (m_sequences.size() != nSeq || m_masterIndex >= nSeq) return false;
    masterSequence = m_sequences[m_masterIndex];
    maxLen = (maxLenIndex < nSeq) ? m_sequences[maxLenIndex].length() : 0;

    //  Count number of residues in each column.
    residueCount.resize(maxLen);
    for (i = 0; i < nSeq; ++i) {
        sequence = m_sequences[i];
        len = sequence.length();
        for (j = 0; j < len; ++j) {
            uc = toupper((unsigned char) sequence[j]);
            if (isalpha(uc)) {
                ++residueCount[j];
            }
        }
     }

    nBlocks = GetBlocksFromCounts(nSeq, residueCount, forcedBreaks, blockStarts, blockLengths);
    if (nBlocks == 0 || nBlocks != blockLengths.size()) return false;


    //  Make seq-aligns and add to the CD.
    //  This assumes that the first character from each of the
    //  sequences in the original fasta file were aligned.  
    //  The final 'true' argument instructs the method to look for and
    //  remove gaps in the sequence data and convert the blockStarts
    //  accordingly.  

    CRef< CSeq_id > master, slave;
    CSeq_annot::C_Data::TAlign& aligns = m_seqAnnot->SetData().SetAlign();

    if (! dummyCD.GetSeqIDForIndex(m_masterIndex, master)) return false;

    for (i = 0; i < nSeq; ++i) {

        //  If one sequence, simply align it to itself
        if (nSeq == 1) {

            CRef<CSeq_align> sa(new CSeq_align());
            if (BuildMasterSlaveSeqAlign(master, master, m_sequences[i], m_sequences[i], blockStarts, blockLengths, sa)) {
                aligns.push_back(sa);
                _TRACE("    Made IBM dummy seq-align " << i);
            }

        } else {

            if (i == m_masterIndex) continue;
            if (!dummyCD.GetSeqIDForIndex(i, slave)) return false;

            CRef<CSeq_align> sa(new CSeq_align());
            if (BuildMasterSlaveSeqAlign(master, slave, masterSequence, m_sequences[i], blockStarts, blockLengths, sa)) {
                aligns.push_back(sa);
                _TRACE("    Made IBM seq-align " << i);
            }
        }        
    }

    if (aligns.size() == nSeq - 1 || (nSeq == 1 && aligns.size() == nSeq)) {
        builtIt = true;
        PurgeNonAlphaFromCachedSequences();
        _TRACE("IBM Seq-annot installed in member variable m_seqAnnot\n");
    } else {
        _TRACE("Error:  IBM Seq-annot NOT installed in member variable m_seqAnnot\n");
    }

    return builtIt;
}

bool CSeqAnnotFromFasta::MakeAsIsSeqAnnot(CCdCore& dummyCD)
{
    bool builtIt = false;
    unsigned int i, j, len, maxLen, masterLen;
    unsigned int nSeq = (unsigned int) dummyCD.GetNumSequences(), maxLenIndex = 0;
    unsigned char uc;
    string sequence, masterSequence;
    vector<unsigned int> residueCount, masterResidueCount;  
    set<unsigned int> masterSeqIndices, forcedBreaks;  

    typedef map<unsigned int, vector<unsigned int> > SeqStartMap;
    typedef SeqStartMap::iterator SeqStartIt;
    typedef SeqStartMap::value_type SeqStartVT;

    unsigned int nBlocks;
    vector<unsigned int> blockStarts;
    vector<unsigned int> blockLengths;


    if (nSeq == 0) {
        return false;
    }

    //  Cache the sequence strings (keeping any gap characters at this point).
    //  Index of longest sequence returned.
    CacheSequences(dummyCD, maxLenIndex, false);
    nSeq = m_sequences.size();

    if (maxLenIndex < nSeq && m_masterIndex < nSeq) {
        masterSequence = m_sequences[m_masterIndex];
        masterLen = masterSequence.length();
        masterResidueCount.assign(masterLen, 0);
        for (j = 0; j < masterLen; ++j) {
            uc = toupper((unsigned char) masterSequence[j]);
            if (isalpha(uc)) {
                ++masterResidueCount[j];
            }
        }

        maxLen = m_sequences[maxLenIndex].length();
    } else {
        return false;
    }

    CRef< CSeq_id > master, slave;
    if (!dummyCD.GetSeqIDForIndex(m_masterIndex, master)) return false;

    CSeq_annot::C_Data::TAlign& aligns = m_seqAnnot->SetData().SetAlign();


    //  For each master-slave pair, flag the columns that are aligned.
    for (i = 0; i < nSeq; ++i) {

        //  If one sequence, simply align it to itself
        if (nSeq == 1) {

            blockStarts.clear();
            blockLengths.clear();
            nBlocks = GetBlocksFromCounts(1, masterResidueCount, forcedBreaks, blockStarts, blockLengths);
            if (nBlocks == 0) return false;

            CRef<CSeq_align> sa(new CSeq_align());
            if (BuildMasterSlaveSeqAlign(master, master, m_sequences[i], m_sequences[i], blockStarts, blockLengths, sa)) {
                aligns.push_back(sa);
                _TRACE("    Made IBM dummy seq-align " << i);
            }
            continue;
        } 

        if (i == m_masterIndex) continue;

        residueCount.assign(maxLen, 0);
        sequence = m_sequences[i];
        len = min((unsigned int)sequence.length(), masterLen);
        for (j = 0; j < len; ++j) {
            uc = toupper((unsigned char) sequence[j]);
//            if (uc != 'X' && isalpha(uc)) {
            if (isalpha(uc) && masterResidueCount[j] > 0) {
                residueCount[j] = 1;
            }
        }

        //  Find this pair's block model
        blockStarts.clear();
        blockLengths.clear();
        nBlocks = GetBlocksFromCounts(1, residueCount, forcedBreaks, blockStarts, blockLengths);
        if (nBlocks == 0) return false;

        //  Make pairwise seq-aligns for (m_masterIndex, i) and add to the CD.
        //  This assumes that the first character from each of the
        //  sequences in the original fasta file were aligned.  
        if (!dummyCD.GetSeqIDForIndex(i, slave)) return false;

        CRef<CSeq_align> pairwiseSA(new CSeq_align());
        if (BuildMasterSlaveSeqAlign(master, slave, masterSequence, sequence, blockStarts, blockLengths, pairwiseSA)) {
            aligns.push_back(pairwiseSA);

            _TRACE("    Made seq-align for master index " << m_masterIndex << " and slave index " << i);
        }

    }


    if (aligns.size() == nSeq - 1 || (nSeq == 1 && aligns.size() == nSeq)) {
        builtIt = true;
        PurgeNonAlphaFromCachedSequences();
        _TRACE("As-is Seq-annot installed in member variable m_seqAnnot\n");
    } else {
        _TRACE("Error:  As-is Seq-annot NOT installed in member variable m_seqAnnot\n");
    }

    return builtIt;
}

unsigned int CSeqAnnotFromFasta::DetermineMasterIndex(CCdCore& dummyCD, MasteringMethod method)
{
    unsigned int masterIndex = 0;
    unsigned int i, j, maxLen, nSeq, nStructs;
    unsigned int nAlignedMax, nGapsMin, nGaps;
    unsigned int firstCommon, lastCommon;
//    unsigned int maxLen = 0, nSeq = (unsigned int) dummyCD.GetNumSequences();
    set<unsigned int> alignedSeqs;
    vector<unsigned int> isConsidered;
    vector<unsigned int> nGapsBySeq, nAlignedBySeq, nAlignedByCol, lengths;
    vector<string> tmpSeqs;
    const CPDB_seq_id* pPDB;

    //  If one sequence, it must be the master.
    if (dummyCD.GetNumSequences() == 1) 
        method = eFirstSequence;

    switch (method) {

        //  This method will pick the sequence with the most aligned residues in the
        //  'core' region but also w/ the fewest gaps.
        //  Don't count residues outside of the footprint defined by the first and
        //  last residues aligned in all sequences.
        //  Only the eMostAlignedAndFewestGaps respects the preference for a structure master.
    case eMostAlignedAndFewestGaps:
        m_masterIndex = masterIndex;
        nSeq = (unsigned int) dummyCD.GetNumSequences();
        nStructs = (m_preferStructureMaster) ? (unsigned int) dummyCD.Num3DAlignments() : 0;

        maxLen = 0;

        for (i = 0; i < nSeq; ++i) {
            isConsidered.push_back((nStructs == 0 || dummyCD.GetPDB(i, pPDB)) ? 1 : 0);
            tmpSeqs.push_back(dummyCD.GetSequenceStringByIndex(i));
            lengths.push_back(tmpSeqs.back().length());
            if (lengths.back() > maxLen) maxLen = lengths.back();
        }

        //  Count number of non-gaps in each column.
        nAlignedBySeq.resize(nSeq);
        nGapsBySeq.resize(nSeq);
        nAlignedByCol.resize(maxLen);
        for (j = 0; j < maxLen; ++j) {

            for (i = 0; i < nSeq; ++i) {
                if (isConsidered[i] > 0 && j < lengths[i] && isalpha((unsigned char) tmpSeqs[i][j])) {
                    ++nAlignedByCol[j];
                }
            }
            //cout << "j = " << j << ";  naligned " << nAlignedByCol[j] << endl;
        }


        //  Define the range of indices overwhich we count # aligned and # gaps.
        //  If there's no column aligned in all sequences, use the criteria to 
        //  get as many commonly aligned residues as possible (at least 2)
        firstCommon = maxLen - 1;
        lastCommon = 0;

        for (i = (nStructs > 0) ? nStructs : nSeq; i > 1 && lastCommon < firstCommon; --i) {
            j = 0;
            while (j < maxLen && nAlignedByCol[j] < i) {
                ++j;
            }
            firstCommon = j;

            j = maxLen;
            while (j > 0 && nAlignedByCol[j - 1] < i) {
                --j;
            }
            lastCommon = j;

//            cout << "i = " << i << "; fc = " << firstCommon << "; lc = " << lastCommon << endl;
        }

        //  If couldn't find a decent footprint, just use the existing master (row 0).
        if (lastCommon <= firstCommon) {
            break;  
        } else {
            cout << "Pick the best master based on largest footprint [" << firstCommon << ", " << lastCommon-1 << "] where " << i+1 << " of the " << nSeq << " sequences are aligned:\n";
            if (nStructs > 0) cout << "(only the structured sequences were candidates)\n";
        }
            

        //  Count number of aligned residues over the footprint just determined.
        for (j = firstCommon; j < lastCommon; ++j) {
            alignedSeqs.clear();

            //  Count number of non-gaps in the j-th column; record which sequences.
            for (i = 0; i < nSeq; ++i) {
                if (isConsidered[i] > 0 && j < lengths[i]) {
                    if (isalpha((unsigned char) tmpSeqs[i][j])) {
                        alignedSeqs.insert(i);
                    } else {
                        ++nGapsBySeq[i];
                    }
                }
            }
            

            //  When there's any aligned sequences in this column, increment nAlignedBySeq 
            //  for the participating sequences.
            if (nAlignedByCol[j] > 1) {
                for (set<unsigned int>::iterator alignedSeqsIt = alignedSeqs.begin();
                     alignedSeqsIt != alignedSeqs.end(); ++alignedSeqsIt) {
                    ++nAlignedBySeq[*alignedSeqsIt];
                }
            }
        }

        //  Finally, select the master index based on max number of aligned residues...
        nAlignedMax = 0;
//        cout << "Test number aligned/number of gaps calculation...\n";
        for (i = 0; i < nSeq; ++i) {

            if (isConsidered[i] == 0) continue;

//            cout << "    " << i << "    " << nAlignedBySeq[i] << "/" << nGapsBySeq[i] << endl;  //"  (full seq has " << nGaps << " gaps)\n";

            if (nAlignedBySeq[i] > nAlignedMax) {
                alignedSeqs.clear();
                nAlignedMax = nAlignedBySeq[i];
                alignedSeqs.insert(i);
                _TRACE("Current longest aligned row " << i << " with " << nAlignedMax << " residues.\n");
            } else if (nAlignedBySeq[i] == nAlignedMax) {
                alignedSeqs.insert(i);
                _TRACE("Duplicate longest aligned row " << i << " with " << nAlignedMax << " residues.\n");
            }
        }

        _TRACE("Naligned seqs " << alignedSeqs.size() << "; tmpSeqs size = " << tmpSeqs.size());

        //  ...number of gaps is the tie-breaker.
        //  (If alignedSeqs.size == 0, will use 1st sequence.)
        if (alignedSeqs.size() == 1) {
            m_masterIndex = *(alignedSeqs.begin());
        } else if (alignedSeqs.size() > 1) {
            nGapsMin = 1000000000;
            for (set<unsigned int>::iterator alignedSeqsIt = alignedSeqs.begin();
                 alignedSeqsIt != alignedSeqs.end(); ++alignedSeqsIt) {

                //  If a sequence is shorter than the max length, would need extra
                //  ending gaps to accommodate longest sequence if made master.
//                const string& sequence = tmpSeqs[*alignedSeqsIt];
//                unsigned int len = sequence.length();
//                nGaps = count_if(sequence.begin(), sequence.end(), isNotAlpha) + (maxLen - len);
                nGaps = nGapsBySeq[*alignedSeqsIt];
                if (nGaps < nGapsMin) {
                    nGapsMin = nGaps;
                    masterIndex = *alignedSeqsIt;
                }
            }
            m_masterIndex = masterIndex;
        } 
        cout << "    ->  master sequence index (zero-based) determined to be " << m_masterIndex << endl;

        break;
    case eFirstSequence:
        m_masterIndex = 0;
        break;
    //  Use whatever had been explicitly specified previously.
    case eSpecifiedSequence:
        break;
    default:
        m_masterIndex = 0;
        break;
    };

    _TRACE("Master sequence index (zero-based) determined to be " << m_masterIndex << "\n");
        
    return m_masterIndex;
}

//  Break up the vector 'counts' into blocks which contain no entries less than the 'threshold'.
//  Resulting blocks are defined by their start index in count and length of the block in terms
//  of number of consecutive indices that satisfy the threshold criteria.
//  (algorithm adapted from cd_utils::BlockIntersector::getIntersectedAlignment)
unsigned int CSeqAnnotFromFasta::GetBlocksFromCounts(unsigned int threshold, const vector<unsigned int>& counts, const set<unsigned int>& forcedBreak, vector<unsigned int>& starts, vector<unsigned int>& lengths) {

    //  Find block starts/lengths 
	bool inBlock = false, forcedNewBlock = false;
    unsigned int i, n = counts.size();
	unsigned int start = 0;
	unsigned int blockId = 0;
    set<unsigned int>::const_iterator setEnd = forcedBreak.end();

	for (i = 0; i < n; i++)
	{
		if (!inBlock)
		{
			if (counts[i] >= threshold)
			{
				start = i;
				inBlock = true;
			}
		}
		else
		{
            //  was the previous position a forced C-terminus?
            forcedNewBlock = (i > 0 && forcedBreak.find(i-1) != setEnd);

			if (counts[i] < threshold)
			{
				inBlock = false;
                starts.push_back(start);
                lengths.push_back(i - start);
//                _TRACE("    Made block " << blockId + 1 << " with length " << i - start << " at start pos. " << start);
				blockId++;
			} else if (forcedNewBlock) {
                starts.push_back(start);
                lengths.push_back(i - start);
                start = i;
            }
		}
	}
	if (inBlock) {//block goes to the end of the sequence
        starts.push_back(start);
        lengths.push_back(i - start);
//        _TRACE("    Made block " << blockId + 1 << " with length " << i - start << " at start pos. " << start);
    }

    return starts.size();
}

bool CSeqAnnotFromFasta::PurgeNonAlphaFromSequence(CBioseq& bioseq) {

    bool result = false;
    CSeq_inst::TLength newLength;

    string originalSequence;
//    CRef< CBioseq > bioseq;
//    if (cd.GetBioseqForIndex(index, bioseq)) {

    if (bioseq.GetInst().IsSetSeq_data()) {

        CSeq_data& seqData = bioseq.SetInst().SetSeq_data();

        if (seqData.IsNcbieaa()) {
            originalSequence = seqData.SetNcbieaa().Set();
        } else if (seqData.IsIupacaa()) {
            originalSequence = seqData.SetIupacaa().Set();
        } else if (seqData.IsNcbistdaa()) {
            std::vector < char >& vec = seqData.SetNcbistdaa().Set();
            NcbistdaaToNcbieaaString(vec, &originalSequence);
        }

        if (PurgeNonAlpha(originalSequence)) {
//            if (originalSequence.length() > 0 && find_if(originalSequence.begin(), originalSequence.end(), isNotAlpha) != originalSequence.end()) {

//                originalSequence.erase(remove_if(originalSequence.begin(), originalSequence.end(), isNotAlpha), originalSequence.end());
            
//            _TRACE("after remove non-alpha:  \n" << originalSequence);
                
            seqData.Select(CSeq_data::e_Ncbieaa);
            seqData.SetNcbieaa().Set(originalSequence);
            result = true;
        }
        newLength = originalSequence.length();
        bioseq.SetInst().SetLength(newLength);

    }
//    }
    return result;
}

void CSeqAnnotFromFasta::PurgeNonAlphaFromCachedSequences() 
{
    unsigned int i, nSeq = m_sequences.size();
    for (i = 0; i < nSeq; ++i) {
        PurgeNonAlpha(m_sequences[i]);
    }
}

void CSeqAnnotFromFasta::CacheSequences(CCdCore& dummyCD, unsigned int& longestSequenceIndex, bool degapSequences)
{
    unsigned int i, len;
    unsigned int maxLen = 0, nSeq = (unsigned int) dummyCD.GetNumSequences();
    string s;

    longestSequenceIndex = 0;
    m_sequences.clear();
    for (i = 0; i < nSeq; ++i) {
        s = dummyCD.GetSequenceStringByIndex(i);
        if (degapSequences) PurgeNonAlpha(s);
        m_sequences.push_back(s);
        len = s.length();

        if (len > maxLen) {
            _TRACE("New longest sequence " << i+1 << ": new max len = " << len << ";  old max len = " << maxLen);
            maxLen = len;
            longestSequenceIndex = i;
        }

        if (len == 0) {
            cerr << "len = 0 in CacheSequences for i = " << i << ", maxLen = " << maxLen << "; gi = " << dummyCD.GetGIFromSequenceList(i) << ":\n" << s << endl;
        }
    }
}

bool  CSeqAnnotFromFasta::BuildMasterSlaveSeqAlign(const CRef<CSeq_id>& masterSeqid, const CRef<CSeq_id>& slaveSeqid, const string& masterSequence, const string& slaveSequence, const vector<unsigned int>& blockStarts, const vector<unsigned int>& blockLengths, CRef<CSeq_align>& pairwiseSA) 
{
    typedef CSeq_align::C_Segs::TDendiag TDD;

    map<unsigned int, unsigned int> nGapsPriorToBlockStart, nGapsPriorToBlockStartMaster;
    unsigned int j, masterStart, slaveStart;
    unsigned int nBlocks = blockStarts.size();

    if (masterSequence.length() == 0 || slaveSequence.length() == 0) return false;
    if (masterSeqid.Empty() || slaveSeqid.Empty()) return false;
    if (nBlocks != blockLengths.size()) return false;

    CountNonAlphaToPositions(blockStarts, masterSequence, nGapsPriorToBlockStartMaster);
    CountNonAlphaToPositions(blockStarts, slaveSequence, nGapsPriorToBlockStart);

    if (pairwiseSA.Empty()) {
        pairwiseSA.Reset(new CSeq_align());
        if (pairwiseSA.Empty()) {
            return false;
        }
    }

    pairwiseSA->SetType(CSeq_align::eType_partial);
    pairwiseSA->SetDim(2);
        
    TDD& ddl = pairwiseSA->SetSegs().SetDendiag();
    for (j = 0; j < nBlocks; ++j) {

        CRef< CDense_diag > dd(new CDense_diag());
        dd->SetDim(2);

        //  Fill in the Seq-ids.
        CRef< CSeq_id > masterCopy = CopySeqId(masterSeqid);
        CRef< CSeq_id > slaveCopy = CopySeqId(slaveSeqid);
        CDense_diag::TIds& ids = dd->SetIds();
        ids.push_back(masterCopy);
        ids.push_back(slaveCopy);

        //  Fill in the block starts & length
        masterStart = blockStarts[j] - nGapsPriorToBlockStartMaster[blockStarts[j]];
        slaveStart  = blockStarts[j] - nGapsPriorToBlockStart[blockStarts[j]];
        dd->SetStarts().push_back(masterStart);
        dd->SetStarts().push_back(slaveStart);
        dd->SetLen(blockLengths[j]);

        //cerr << "seq " << i+1 << " block " << j+1 << endl;
//            cerr << "    blockStarts[j] = " << blockStarts[j] << "; nGaps-master = " << nGapsPriorToBlockStartMaster[blockStarts[j]] << ", nGaps-slave = " << nGapsPriorToBlockStart[blockStarts[j]] << endl;
        //cerr << "    master Start " << masterStart << "; slave Start " << slaveStart << endl;

        ddl.push_back(dd);
    }

    return true;
}


END_SCOPE(cd_utils)
END_NCBI_SCOPE
