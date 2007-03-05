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
* ===========================================================================
*/

#ifndef CU_SEQANNOT_FROM_FASTA__HPP
#define CU_SEQANNOT_FROM_FASTA__HPP

#include <map>
#include <list>
#include <vector>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiapp.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Bioseq.hpp>
#include <algo/structure/cd_utils/cuCdCore.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(cd_utils)

class NCBI_CDUTILS_EXPORT CSeqAnnotFromFasta
{

public:

    enum MasteringMethod {
        eFirstSequence = 0,
        eSpecifiedSequence,
        eMostAlignedAndFewestGaps,   //  looking in the IBM footprint
        eUnassignedMaster = 99999999
    };

    CSeqAnnotFromFasta(bool doIbm = true, bool preferStructureMaster = false, bool caseSensitive = false);
    virtual ~CSeqAnnotFromFasta() {
        m_seqAnnot.Reset();
    }

    virtual bool IsSeqAnnotValid() const;

    //  The 'masterIndex' parameter is ignored unless masterMethod == eSpecifiedSequence.
    //  Makes a Seq-annot that has been indexed to the de-gapped sequence data from the
    //  CFastaIOWrapper object, and caches those de-gapped sequeces in m_sequences.
    virtual bool MakeSeqAnnotFromFasta(CNcbiIstream& is, CFastaIOWrapper& fastaIO, MasteringMethod masterMethod, unsigned int masterIndex = (unsigned int) eUnassignedMaster);

    //  Return empty string if index out of range of m_sequences.
    string GetSequence(unsigned int index) const;

    const CRef<CSeq_annot>& GetSeqAnnot() const {return m_seqAnnot;}

    //  Removes any non-alphanumeric characters from the sequence data in
    //  'bioseq', adjusting the length as necessary.  Will change the encoding
    //  of the sequence to Ncbieaa if any characters were purged.
    //  Returns 'true' if any characters were removed.
    static bool PurgeNonAlphaFromSequence(CBioseq& bioseq);

    //  Same as above but works on the cached strings in m_sequences.
    void PurgeNonAlphaFromCachedSequences();
    

    unsigned int GetMasterIndex() const {return m_masterIndex;}
    //void SetMasterIndex(unsigned int masterIndex) { m_masterIndex = masterIndex; //requires remaster}

    //  Break up the vector 'counts' into blocks which contain no entries less than the 'threshold'.
    //  Resulting blocks are defined by their start index in count and length of the block in terms
    //  of number of consecutive indices that satisfy the threshold criteria.
    //  (algorithm adapted from cd_utils::BlockIntersector::getIntersectedAlignment)
    static unsigned int GetBlocksFromCounts(unsigned int threshold, const vector<unsigned int>& counts, const set<unsigned int>& forcedBreak, vector<unsigned int>& starts, vector<unsigned int>& lengths);

    static void CountNonAlphaToPositions(const vector<unsigned int>& positions, const string& sequence, map<unsigned int, unsigned int>& numNonAlpha);

    static bool isNotAlpha(char c);

private:

    //  m_doIBM = false: the seq_annot will reflect the pairwise alignments found in the fasta
    //  m_doIBM = true:  the seq_annot will contain only columns aligned in every pairwise alignment
    bool m_doIBM;  

    //  If true, use a structure as a master where possible (i.e., the preference may be
    //  overriden for various MasteringMethods).
    bool m_preferStructureMaster;

    //  A Fasta variant allows for lowercase letters in a sequence string to indicate that that
    //  residue is not intended to be aligned, even if it potentially could have been.  
    //  m_caseSensitive = true:  do not align lowercase residues in the seq_annot
    //  m_caseSensitive = false: lowercase residues will be aligned in the seq_annot if possible
    bool m_caseSensitive;

    //  The seq_annot's pairwise alignments will be indexed to a common sequence.
    //  m_masterIndex is the zero-based index of that sequence in the input fasta.
    unsigned int m_masterIndex;

    CRef<CSeq_annot> m_seqAnnot;
    vector<string> m_sequences;  //  cache for the sequences in m_seqAnnot.

    //  Cache sequence strings found in the 'sequences' field of 'dummyCD'.
    //  If 'degapSequences' is true, remove any gap characters prior to adding the string
    //  to m_sequences.  Returns index of the longest sequence in m_sequences after their
    //  addition (i.e., longest sequences after any degapping was done).
    void CacheSequences(CCdCore& dummyCD, unsigned int& longestSequenceIndex, bool degapSequences);

    bool MakeIBMSeqAnnot(CCdCore& dummyCD);
    bool MakeAsIsSeqAnnot(CCdCore& dummyCD);

    //  Make a seq_align from the block starts & lengths, where the starts are based 
    //  on the sequences passed in.  The seq-align is made by reindexing to a *gapless*
    //  version of the master and slave sequences.
    bool  BuildMasterSlaveSeqAlign(const CRef<CSeq_id>& masterSeqid, const CRef<CSeq_id>& slaveSeqid, const string& masterSequence, const string& slaveSequence, const vector<unsigned int>& blockStarts, const vector<unsigned int>& blockLengths, CRef<CSeq_align>& pairwiseSA); 

    //  Sets m_masterIndex as per the mastering method chosen.  Returns m_masterIndex.
    unsigned int DetermineMasterIndex(CCdCore& dummyCD, MasteringMethod masterMethod);
    

};

END_SCOPE(cd_utils)
END_NCBI_SCOPE

#endif // CU_SEQANNOT_FROM_FASTA__HPP
