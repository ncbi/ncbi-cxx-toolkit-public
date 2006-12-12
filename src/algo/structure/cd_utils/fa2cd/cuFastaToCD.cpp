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
 *           application to generate a CD from a mFASTA text file
 *
 */

#include <ncbi_pch.hpp>
#include <algorithm>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbifile.hpp>
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

#include <objects/seq/Seq_annot.hpp>
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

#include <algo/structure/bma_refine/ColumnScorer.hpp>

#include <algo/structure/cd_utils/cuReadFastaWrapper.hpp>
#include <algo/structure/cd_utils/cuSeqAnnotFromFasta.hpp>
#include <algo/structure/cd_utils/cuCdFromFasta.hpp>
#include "cuFastaToCD.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(align_refine);
USING_SCOPE(cd_utils);

bool PurgeNonAlphaFromSequence(CCdFromFasta& cd, unsigned int index) {

    bool result = false;
    CSeq_inst::TLength newLength;

    string originalSequence;
    CRef< CBioseq > bioseq;
    if (cd.GetBioseqForIndex(index, bioseq)) {

        if (bioseq->GetInst().IsSetSeq_data()) {

            CSeq_data& seqData = bioseq->SetInst().SetSeq_data();

            if (seqData.IsNcbieaa()) {
                originalSequence = seqData.SetNcbieaa().Set();
            } else if (seqData.IsIupacaa()) {
                originalSequence = seqData.SetIupacaa().Set();
            } else if (seqData.IsNcbistdaa()) {
                std::vector < char >& vec = seqData.SetNcbistdaa().Set();
                NcbistdaaToNcbieaaString(vec, &originalSequence);
            }

            if (originalSequence.length() > 0 && find_if(originalSequence.begin(), originalSequence.end(), CSeqAnnotFromFasta::isNotAlpha) != originalSequence.end()) {

                originalSequence.erase(remove_if(originalSequence.begin(), originalSequence.end(), CSeqAnnotFromFasta::isNotAlpha), originalSequence.end());
            
                _TRACE("after remove non-alpha:  \n" << originalSequence);
                
                seqData.Select(CSeq_data::e_Ncbieaa);
                seqData.SetNcbieaa().Set(originalSequence);
                result = true;
            }
            newLength = originalSequence.length();
            bioseq->SetInst().SetLength(newLength);

        }
    }
    return result;
}

bool  BuildSeqAnnotForCD(CCdFromFasta& cd, const vector<unsigned int>& blockStarts, const vector<unsigned int>& blockLengths, bool purgeGaps) 
{
    typedef CSeq_align::C_Segs::TDendiag TDD;

    unsigned int i, j;
    unsigned int masterStart, slaveStart;
    unsigned int nBlocks = blockStarts.size();
    unsigned int nSeq = (unsigned int) cd.GetNumSequences();
    map<unsigned int, unsigned int> nGapsPriorToBlockStart, nGapsPriorToBlockStartMaster;
    string sequence;

    if (nBlocks != blockLengths.size()) return false;

    CRef< CSeq_id > master, slave;
//    CCdd::TSeqannot& seqAnnotList = cd.SetSeqannot();
    CRef< CSeq_annot> seqAnnot(new CSeq_annot());
    seqAnnot->SetData().Select(CSeq_annot::C_Data::e_Align);
    CSeq_annot::C_Data::TAlign& aligns = seqAnnot->SetData().SetAlign();

    if (! cd.GetSeqIDForIndex(0, master)) return false;

    sequence = cd.GetSequenceStringByIndex(0);
    CSeqAnnotFromFasta::CountNonAlphaToPositions(blockStarts, sequence, nGapsPriorToBlockStartMaster);
    if (purgeGaps) PurgeNonAlphaFromSequence(cd, 0);

    for (i = 1; i < nSeq; ++i) {

        sequence = cd.GetSequenceStringByIndex(i);
        CSeqAnnotFromFasta::CountNonAlphaToPositions(blockStarts, sequence, nGapsPriorToBlockStart);
        if (purgeGaps) PurgeNonAlphaFromSequence(cd, i);

        CRef<CSeq_align> sa(new CSeq_align());
        if (sa.Empty()) return false;

        sa->SetType(CSeq_align::eType_partial);
        sa->SetDim(2);
        
        TDD& ddl = sa->SetSegs().SetDendiag();
        for (j = 0; j < nBlocks; ++j) {
            CRef< CDense_diag > dd(new CDense_diag());
            dd->SetDim(2);

            CDense_diag::TIds& ids = dd->SetIds();
            if (! cd.GetSeqIDForIndex(i, slave)) return false;

            CRef< CSeq_id > masterCopy = CopySeqId(master);
            CRef< CSeq_id > slaveCopy = CopySeqId(slave);
            ids.push_back(masterCopy);
            ids.push_back(slaveCopy);

            masterStart = blockStarts[j] - nGapsPriorToBlockStartMaster[blockStarts[j]];
            slaveStart  = blockStarts[j] - nGapsPriorToBlockStart[blockStarts[j]];
            //cerr << "seq " << i+1 << " block " << j+1 << endl;
//            cerr << "    blockStarts[j] = " << blockStarts[j] << "; nGaps-master = " << nGapsPriorToBlockStartMaster[blockStarts[j]] << ", nGaps-slave = " << nGapsPriorToBlockStart[blockStarts[j]] << endl;
            //cerr << "    master Start " << masterStart << "; slave Start " << slaveStart << endl;

            dd->SetStarts().push_back(masterStart);
            dd->SetStarts().push_back(slaveStart);
            dd->SetLen(blockLengths[j]);

            ddl.push_back(dd);
        }

        aligns.push_back(sa);
        _TRACE("    Made seq-align " << i);
        
    }

    cd.SetSeqannot().push_back(seqAnnot);

    _TRACE("Seq-annot installed in CD\n");
    return true;
}

bool  TrimSeqAnnotForCD(CCdFromFasta& cd, const set<unsigned int>& masterSeqIndicesToTrim, const set<unsigned int>& forcedBreaks)
{
    typedef CSeq_align::C_Segs::TDendiag TDD;

    unsigned int i, j;
    unsigned int nBlocks, masterStart, len;
    unsigned int nToTrim = masterSeqIndicesToTrim.size();
    unsigned int nBlocksOrig = cd.GetNumBlocks();
    unsigned int nSeq = (unsigned int) cd.GetNumSequences();
    unsigned int mappedPos, first = (unsigned int) cd.GetLowerBound(0), last = (unsigned int) cd.GetUpperBound(0);

    vector<int> originalMasterStarts, originalLengths;
    vector<unsigned int> residueCounts, newMasterStarts, newLengths;
    set<unsigned int>::const_iterator trimIt = masterSeqIndicesToTrim.begin(), trimEnd = masterSeqIndicesToTrim.end();
    string sequence;

    if (nToTrim == 0) return true;

    CRef< CSeq_id > master, slave;
    CRef< CSeq_annot> seqAnnot(new CSeq_annot());
    seqAnnot->SetData().Select(CSeq_annot::C_Data::e_Align);
    CSeq_annot::C_Data::TAlign& aligns = seqAnnot->SetData().SetAlign();

    if (first < 0 || last < 0) return false;
    if (! cd.GetSeqIDForIndex(0, master)) return false;
    if (! cd.GetCDBlockLengths(originalLengths) || ! cd.GetBlockStartsForRow(0, originalMasterStarts)) return false;
    if (originalLengths.size() != originalMasterStarts.size()) return false;


    //  'last' is the last aligned residue in zero-based counting.
    residueCounts.resize(last+1);

    //  No blocks before the first aligned residue.
    for (i = 0; i < first; ++i) residueCounts[i] = 0;

    //  Mark each currently aligned master residue.
    for (i = 0; i < nBlocksOrig; ++i) {
        masterStart = (unsigned int) originalMasterStarts[i];
        len = (unsigned int) originalLengths[i];
        for (j = 0; j < len; ++j) {
            residueCounts[masterStart + j] = 1;
        }
    }

    //  Unmark anything to be trimmed.
    _TRACE("Number of columns to trim:  " << nToTrim);
    for (; trimIt != trimEnd; ++trimIt) {
        if (*trimIt <= last) {
            residueCounts[*trimIt] = 0;
        }
    }

    nBlocks = CSeqAnnotFromFasta::GetBlocksFromCounts(1, residueCounts, forcedBreaks, newMasterStarts, newLengths);

    //  If everything is trimmed, put in a dummy alignment of one residue.
    if (nBlocks == 0) {
        newMasterStarts.push_back(first);
        newLengths.push_back(1);
        ++nBlocks;
    }

    for (i = 1; i < nSeq; ++i) {

        CRef<CSeq_align> sa(new CSeq_align());
        if (sa.Empty()) return false;

        sa->SetType(CSeq_align::eType_partial);
        sa->SetDim(2);
        
        TDD& ddl = sa->SetSegs().SetDendiag();
        for (j = 0; j < nBlocks; ++j) {
            CRef< CDense_diag > dd(new CDense_diag());
            dd->SetDim(2);

            CDense_diag::TIds& ids = dd->SetIds();
            if (! cd.GetSeqIDForIndex(i, slave)) return false;

            CRef< CSeq_id > masterCopy = CopySeqId(master);
            CRef< CSeq_id > slaveCopy = CopySeqId(slave);
            ids.push_back(masterCopy);
            ids.push_back(slaveCopy);

            masterStart = newMasterStarts[j];
            mappedPos = cd.MapPositionToOtherRow(0, masterStart, i);
            if (mappedPos < 0) {
                cerr << "Error (TrimSeqAnnotForCd):\nCould not map master start " << masterStart+1 << " of new block " << j+1 << " to row " << i+1 << "\nStopping.\n"; 
                return false;
            }

            //cerr << "seq " << i+1 << " new block " << j+1 << endl;
            //cerr << "    new master Start " << masterStart << "; new slave Start " << mappedPos << endl;
            dd->SetStarts().push_back(masterStart);
            dd->SetStarts().push_back((unsigned int) mappedPos);
            dd->SetLen(newLengths[j]);

            ddl.push_back(dd);
        }

        aligns.push_back(sa);
        _TRACE("    Made seq-align " << i);
        
    }

    cd.ResetSeqannot();
    cd.SetSeqannot().push_back(seqAnnot);

    _TRACE("New trimmed Seq-annot installed in CD\n");
    return true;
}

//  If the 'scorer' is provided, a column is included in the resulting alignment only if the 
//  score of that column meets or exceeds the 'scorerThreshold';   'scorerThreshold' is 
//  ignored if 'scorer' is null.
bool FastaSeqEntry_To_SeqAnnot(CCdFromFasta& cd, ColumnScorer* scorer, double scorerThreshold) {

    bool builtIt = false;
    bool isInPssm, isAligned;
    bool useScorer = (scorer != NULL);
    unsigned int i, nCols, nRows, pssmCol, nAligned;
    unsigned int nSeq = (unsigned int) cd.GetNumSequences();
    double score, score_1, score_2;
    string sequence;
    vector<string> sequences;
    vector<string> columnStrings;
    vector<unsigned int> residueCount;  
    set<unsigned int> masterSeqIndices, forcedBreaks;  

    typedef map<unsigned int, vector<unsigned int> > SeqStartMap;
    typedef SeqStartMap::iterator SeqStartIt;
    typedef SeqStartMap::value_type SeqStartVT;

//    bm::bvector<> bs;

    vector<unsigned int> blockStarts;
    vector<unsigned int> blockLengths;

    if (nSeq == 0) return builtIt;

    //  Trim the alignment based on whether a column's score meets the 
    //  threshold for inclusion.
    if (useScorer) {
        char residue;
        unsigned int masterSeqIndex, prevMasterSeqIndex;
        vector<char> residues;

        AlignmentUtility au(cd.GetSequences(), cd.GetSeqannot());
        BlockMultipleAlignment* bma = (au.Okay()) ? const_cast<BlockMultipleAlignment*>(au.GetBlockMultipleAlignment()) : NULL;

        if (!bma) {
//            ERROR_MESSAGE_CL("Fatal error making block multiple alignment object for CD");
//            return -2;
            return builtIt;
        }

        nRows = bma->NRows();
        nCols = bma->AlignmentWidth();

        _TRACE("Nrows = " << nRows << ";  Ncols = " << nCols);

        nAligned = 0;
        prevMasterSeqIndex = BMA::eUndefined;
        score_1 = score_2 = ColumnScorer::SCORE_INVALID_OR_NOT_COMPUTED;
        for (i = 0, pssmCol = 0; i < nCols; ++i) {

            isAligned = BMAUtils::IsColumnOfType(*bma, i, align_refine::eAlignedResidues, isInPssm);
            residues.clear();
            BMAUtils::GetResiduesForColumn(*bma, i, residues);

            string s;
            BMAUtils::GetCharacterAndIndexForColumn(*bma, i, 0, &residue, &masterSeqIndex);
            for (unsigned int k = 0; k < residues.size(); ++k) s += residues[k];

            //cerr << "Column " << i+1 << " residues:  '" << s << "' (masterSeqIndex = " << masterSeqIndex << "):  isAligned " << isAligned << "; isInPssm " << isInPssm << "\n";


            //  There's no residue for this alignment index; force a break at
            //  the previous sequence index (if defined) to avoid shifting slave
            //  sequences during mapping of coordinates.
            if (prevMasterSeqIndex != BMA::eUndefined && masterSeqIndex == BMA::eUndefined) {
                forcedBreaks.insert(prevMasterSeqIndex);
            }

            if (isAligned) {
                ++nAligned;
                score = scorer->ColumnScore(*bma, i, NULL, 0);

                if (score < scorerThreshold) {
                    _TRACE("Score " << score << " < " << scorerThreshold << ":  master index " << masterSeqIndex << " (residue " << residue << ") excluded from CD; PSSM column " << i+1 << ".");
                    masterSeqIndices.insert(masterSeqIndex);
                } else {
                    //  Try and avoid fragmenting blocks too much.
                    //  If previous column had too low a score but neither neighbor did,
                    //  restore the previous column.
                    if (score_1 < scorerThreshold && score_2 >= scorerThreshold && 
                        score_1 != ColumnScorer::SCORE_INVALID_OR_NOT_COMPUTED && 
                        score_2 != ColumnScorer::SCORE_INVALID_OR_NOT_COMPUTED) {
                        score_1 = score_2;  // fake that score_1 was OK
                        masterSeqIndices.erase(prevMasterSeqIndex);
                        //cerr << "    ***  restored column " << i << endl;
                    }
                }
            } else {
                score = ColumnScorer::SCORE_INVALID_OR_NOT_COMPUTED;
                if (masterSeqIndex != BMA::eUndefined) {
                    masterSeqIndices.insert(masterSeqIndex);
                }
            }
            prevMasterSeqIndex = masterSeqIndex;
            score_2 = score_1;
            score_1 = score;
        }

        //  Marked everything as bad!
        if (nAligned == masterSeqIndices.size()) return builtIt;

        builtIt = TrimSeqAnnotForCD(cd, masterSeqIndices, forcedBreaks);
    }
    

	return builtIt;

}

/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments

void CFasta2CD::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> argDescr(new CArgDescriptions);

    // Specify USAGE context
    argDescr->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Basic MFasta --> CD converter");

    argDescr->AddKey("i", "MFastaIn", "filename of file containing an MFasta alignment (ascii)", argDescr->eString);
    argDescr->AddOptionalKey("n", "CdName", "name of the CD created\n(if not given, uses lowercase of the base of the input mFasta file's name)\nAn accession of the form 'acc_<name>' will also be assigned.\n", argDescr->eString);

//    argDescr->AddOptionalKey("n", "CdName", "name of the CD created\n(if not given, uses lowercase of the base of the input mFasta file's name)\nIf -a is not also given, an accession of the form 'acc_<name>' will also be assigned.\n", argDescr->eString);
//    argDescr->AddOptionalKey("a", "CdAcc", "accession of the CD created\nAn accession of the form 'acc_<name>' will be assigned if the value conflicts with any standard domain accession format at NCBI.\n", argDescr->eString);

    argDescr->AddOptionalKey("o", "OutputCDFileName", "filename for output CD, adding '.cn3' if not present\n(if not given, uses same basename as the input mFasta filename, adding the '.cn3' extension)\n\n", argDescr->eString);//CArgDescriptions::eOutputFile, CArgDescriptions::fPreOpen);

    // Fasta input options

    argDescr->AddFlag("parseIds", "try to parse identifier in the defline as GI, PDB, etc./nNOTE: validation the ID itself, or of sequence data w/ the parsed ID, is attempted.");

    // Output alignment options

    argDescr->AddFlag("ibm", "run the Intersection-By-Master algorithm to keep only those alignment columns without gap characters;\n(without this flag, the input alignment is left 'as-is')");

    argDescr->AddFlag("keepMaster", "use first sequence in the input as master sequence;\nskips attempt to identify an 'optimal' master\n(Ignored if -mr is also specified.)");

    argDescr->AddOptionalKey("mr", "masterRow", "force use of a particular row from the input as the master\n(rows numbered from 1; overrides the -keepMaster flag)", argDescr->eInteger);
    argDescr->SetConstraint("mr", new CArgAllow_Integers(1, 100000));

    argDescr->AddOptionalKey("t", "threshold", "exclude columns w/ median PSSM score less than this value from CD block model\n(no columns excluded by default; a threshold of 3 is good if trimming bad columns is desired)", argDescr->eDouble);

    argDescr->AddFlag("v", "report program version and exit; any other arguments present are ignored.");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(argDescr.release());
}


/////////////////////////////////////////////////////////////////////////////
//  Main Run method; invoke requested tests (printout arguments obtained from command-line)

int CFasta2CD::Run(void)
{

    const string fa2cdVersion = "fa2cd -- Version 1.1.0  ($Date$) \n\nFile conversion utiltiy to generate a .cn3 file, usable as input to Cn3D and CDTree,\nfrom a file containing a mFASTA formatted multiple alignment.\n";

    //    bool useLocalIds, useAlignmentAsis;
    unsigned int nSeq = 1;
    string test, err;
    string fastaFile, cdOutFile, cdAcc, compareMethodStr;
    double threshold, dummyThreshold = -100.0;
    CCdFromFasta::Fasta2CdParams params;
//    TReadFastaFlags fastaFlags = fReadFasta_AssumeProt;
//    CBasicFastaWrapper fastaIO(fastaFlags, false);


    // Process arguments
    CArgs args = GetArgs();

    //  If requested, report version and exit.
    if (args["v"]) {
        NcbiCerr << fa2cdVersion << NcbiEndl;
        return 0;
    }

    PrintArgs(cerr);
    fastaFile = args["i"].AsString();

    threshold = (args["t"]) ? args["t"].AsDouble() : dummyThreshold;

/*
    if (args["o"]) {
        cdOutFile = args["o"].AsString();
    } else {
        cdOutFile = cdAcc + ".cn3";
    }
    if (cdOutFile.length() == 0) cdOutFile = "cd.cn3";
*/

    params.cdName     = BuildCDName(args);
    params.cdAcc = BuildCDAccession(args);
    if (params.cdAcc.length() == 0) {
        (*m_outStream) << "Error:  Invalid CD accession '" << params.cdAcc << "' .\nPlease choose a different accession.\n";
        return -1;
    }

    cdOutFile = BuildOutputFilename(args);

    params.useLocalIds = (! args["parseIds"]);
    params.useAsIs = (! args["ibm"]);

    //  Command-line arg starts counting rows at 1.  Set master as row 0 by default.
    params.masterIndex = (args["mr"]) ? (unsigned int) args["mr"].AsInteger() - 1 : 0;

//    m_outStream = (outFile.size() > 0) ? new ofstream(outFile.c_str(), ios::out) : &cout;
    m_outStream = &cerr;
    SetDiagStream(m_outStream);

//    CSeqAnnotFromFasta fastaSeqAnnot(!useAlignmentAsis, false, false);
    params.masterMethod = CSeqAnnotFromFasta::eMostAlignedAndFewestGaps; 
//    CNcbiIfstream ifs(fastaFile.c_str(), IOS_BASE::in);

    //  Override the mastering method based on command-line parameters.
    if (args["mr"]) {
        params.masterMethod = (params.masterIndex > 0) ? CSeqAnnotFromFasta::eSpecifiedSequence : CSeqAnnotFromFasta::eFirstSequence;
    } else if (args["keepMaster"]) {
        params.masterMethod = CSeqAnnotFromFasta::eFirstSequence;
    }

//    cerr << "First param print:\n" << params.Print() << endl;

//#ifdef _READFASTA_TEST
//    string s;
//    bool writeResult1 = WriteASNToFile("Seq-entry.txt", *se, false, &err);
//    cerr << args.Print(s) << endl;
//    return 0;
//#endif


    m_ccdFromFasta = new CCdFromFasta(fastaFile, params);
    if (!m_ccdFromFasta) return -1;

    //  Convert the sequence data into a block-aligned Seq-align packaged in a Seq-annot
    //  and install it in the CD.
    MedianColumnScorer medianScorer;
    bool converted = FastaSeqEntry_To_SeqAnnot(*m_ccdFromFasta, (threshold == dummyThreshold) ? NULL : &medianScorer, threshold);

    //  Write the CD object (unless column scorer was used and it failed).
    if (threshold != dummyThreshold && !converted) {
        (*m_outStream) << "Error during Fasta->CD conversion.  Try again without using the -t parameter, or providing a different value.\n";
        return -1;
    } else {
        m_ccdFromFasta->WriteToFile(cdOutFile);
        return 0;
    }
}

bool CFasta2CD::OutputCD(const string& filename, string& err) 
{
    return WriteASNToFile(filename.c_str(), *m_ccdFromFasta, false, &err);
}

string CFasta2CD::BuildCDName(const CArgs& args)
{
    string cdAcc;
    if (args["n"]) {
        cdAcc     = args["n"].AsString();
    } else {
        CFile cfile(args["i"].AsString());
        string base = cfile.GetBase();
        cdAcc = NStr::ToLower(base);
    }

    NStr::TruncateSpacesInPlace(cdAcc);

    if (cdAcc.size() == 0) cdAcc = "my_CD";
    return cdAcc;
}

string CFasta2CD::BuildCDAccession(const CArgs& args)
{
    string cdAcc = "acc_" + BuildCDName(args);

//    if (args["a"]) {
//        cdAcc  = args["a"].AsString();
//        NStr::TruncateSpacesInPlace(cdAcc);
//        if (cdAcc.size() == 0) cdAcc = "acc_my_CD";
//    } else if (args["n"]) {
//        cdAcc += BuildCDName(args);
//    }

    return cdAcc;
}


string CFasta2CD::BuildOutputFilename(const CArgs& args)
{
    static const string cdExt = ".cn3";
    string cdOutFile, cdOutExt;
    CFile cFastaFile(args["i"].AsString());
    string inputBase = cFastaFile.GetBase();  //  , cdAcc = args["n"].AsString();

    if (args["o"]) {
        cdOutFile = args["o"].AsString();

        //  Find the last file extension...
        CFile::SplitPath(cdOutFile, NULL, NULL, &cdOutExt);
        if (cdOutFile.length() > 0 && cdOutExt != cdExt) {
            cdOutFile += cdExt;
        }
    } else if (inputBase.length() > 0) {
        cdOutFile = inputBase + cdExt;
//    } else if (cdAcc.size() > 0) {
//        cdOutFile = cdAcc + cdExt;
    }

    //  Do here in case args["o"] might be empty.
    if (cdOutFile.length() == 0) cdOutFile = "fa2cd.cn3";

    return cdOutFile;
}

void CFasta2CD::PrintArgs(ostream& os) 
{
    CArgs args = GetArgs();
    static const string yesStr = "yes", noStr = "no";
    string fastaFile = args["i"].AsString();
    string cdOutFile = BuildOutputFilename(args), cdName = BuildCDName(args);
    string cdAcc = "acc_" + cdName;
    bool useLocalIds = (! args["parseIds"]);
    bool useAlignmentAsIs = (! args["ibm"]);
    bool keepMaster = (args["keepMaster"]);
    unsigned int masterIndex = (args["mr"]) ? (unsigned int) args["mr"].AsInteger() : 1;

    os << "Fasta->CD Settings:        " << endl << "-------------------------------------\n";
    os << "Input mFASTA file:        " << fastaFile << endl;
    os << "CD Name:                  " << cdName << endl;
    os << "CD Accession:             " << cdAcc << endl;
    os << "Output CD file:           " << cdOutFile << endl;
    os << "Use alignment 'as-is':    " << (useAlignmentAsIs ? yesStr : noStr) << endl;
    os << "Parse Ids from Defline:   " << (useLocalIds ? noStr : yesStr) << endl;
    if (keepMaster || args["mr"]) os << "Forced master sequence index:  " << masterIndex << endl;
    if (args["t"]) os << "Trim bad columns from CD using threshold = " << args["t"].AsDouble() << endl;
    os << "-------------------------------------\n\n";

}

/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CFasta2CD::Exit(void)
{
    SetDiagStream(0);
}

/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[])
{

    //  Diagnostics code from Paul's struct_util demo.
    SetDiagStream(&NcbiCout); // send all diagnostic messages to cout
    SetDiagPostLevel(eDiag_Info);   
    //    SetupCToolkitErrPost(); // reroute C-toolkit err messages to C++ err streams

    SetDiagTrace(eDT_Default);      // trace messages only when DIAG_TRACE env. var. is set
    //SetDiagTrace(eDT_Enable);      // trace messages only when DIAG_TRACE env. var. is set
    SetDiagPostFlag(eDPF_Log);      // show only the provided message
    #ifdef _DEBUG
        SetDiagPostFlag(eDPF_File);
        SetDiagPostFlag(eDPF_Line);
    //#else
    //UnsetDiagTraceFlag(eDPF_File);
    //UnsetDiagTraceFlag(eDPF_Line);
    #endif

        _TRACE("test tracing...");

    // C++ object verification
    CSerialObject::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectIStream::SetVerifyDataGlobal(eSerialVerifyData_Always);
    CObjectOStream::SetVerifyDataGlobal(eSerialVerifyData_Always);

    // Execute main application function
    CFasta2CD fasta2Cd;
    fasta2Cd.AppMain(argc, argv, 0, eDS_Default, 0);
    return 0;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.6  2006/12/12 16:18:38  lanczyck
 * change version number to '1.1.0'
 *
 * Revision 1.5  2006/09/01 15:44:05  lanczyck
 * add -v (version) flag
 *
 * Revision 1.4  2006/08/23 19:42:18  lanczyck
 * change defaults and arguments for public release with CDTree
 *
 * Revision 1.3  2006/06/07 20:31:36  lanczyck
 * add <algorithm> to includes for PC; remove unused variables
 *
 * Revision 1.2  2006/05/22 17:15:44  lanczyck
 * use new CCdCore subclass for a Fasta-generated CDs
 *
 * Revision 1.1  2006/03/29 15:44:07  lanczyck
 * add files for fasta->cd converter; change Makefile accordingly
 *
 * ===========================================================================
 */
