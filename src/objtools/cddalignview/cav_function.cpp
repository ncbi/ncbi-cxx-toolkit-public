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
*      C interfaced function body for cddalignview as function call
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbi_limits.h>
#include <corelib/ncbidiag.hpp>

#include <list>
#include <memory>

#include <objects/cdd/Cdd.hpp>
#include <objects/ncbimime/Ncbi_mime_asn1.hpp>
#include <objects/ncbimime/Biostruc_seqs.hpp>
#include <objects/ncbimime/Biostruc_align.hpp>
#include <objects/ncbimime/Biostruc_align_seq.hpp>
#include <objects/ncbimime/Biostruc_seqs_aligns_cdd.hpp>
#include <objects/ncbimime/Bundle_seqs_aligns.hpp>

#include <objtools/cddalignview/cddalignview.h>
#include <objtools/cddalignview/cav_seqset.hpp>
#include <objtools/cddalignview/cav_alignset.hpp>
#include <objtools/cddalignview/cav_asnio.hpp>
#include <objtools/cddalignview/cav_alndisplay.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

static EDiagSev defaultDiagPostLevel;

static int LoadASNFromIstream(CNcbiIstream& asnIstream,
    const SeqEntryList* *sequences, const SeqAnnotList* *alignments)
{
	*sequences = NULL;
	*alignments = NULL;

    // try to decide what ASN type this is, and if it's binary or ascii
    static const string
        asciiMimeFirstWord = "Ncbi-mime-asn1",
        asciiCDDFirstWord = "Cdd";
    bool isMime = false, isCDD = false, isBinary = true;

    string firstWord;
    asnIstream >> firstWord;
    if (firstWord == asciiMimeFirstWord) {
        isMime = true;
        isBinary = false;
    } else if (firstWord == asciiCDDFirstWord) {
        isCDD = true;
        isBinary = false;
    }

    // try to read the file as various ASN types (if it's not clear from the first ascii word).
    auto_ptr<SeqEntryList> newSequences(new SeqEntryList());
    auto_ptr<SeqAnnotList> newAlignments(new SeqAnnotList());
    bool readOK = false;
    string err;

    if (!isCDD) {
        ERR_POST(Info << "trying to read input as " <<
            ((isBinary) ? "binary" : "ascii") << " mime");
        CRef < CNcbi_mime_asn1 > mime(new CNcbi_mime_asn1);
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        asnIstream.seekg(0);
        readOK = ReadASNFromIstream(asnIstream, *mime, isBinary, err);
        SetDiagPostLevel(defaultDiagPostLevel);
        if (readOK) {
            // copy lists
            if (mime->IsStrucseqs()) {
                *newSequences = mime->GetStrucseqs().GetSequences();
                *newAlignments = mime->GetStrucseqs().GetSeqalign();
            } else if (mime->IsAlignstruc()) {
                *newSequences = mime->GetAlignstruc().GetSequences();
                *newAlignments = mime->GetAlignstruc().GetSeqalign();
            } else if (mime->IsAlignseq()) {
                *newSequences = mime->GetAlignseq().GetSequences();
                *newAlignments = mime->GetAlignseq().GetSeqalign();
            } else if (mime->IsGeneral()) {
                if (mime->GetGeneral().GetSeq_align_data().IsBundle()) {
                    *newSequences = mime->GetGeneral().GetSeq_align_data().GetBundle().GetSequences();
                    *newAlignments = mime->GetGeneral().GetSeq_align_data().GetBundle().GetSeqaligns();
                } else if (mime->GetGeneral().GetSeq_align_data().IsCdd()) {
                    newSequences->resize(1);
                    newSequences->front().Reset(&(mime->SetGeneral().SetSeq_align_data().SetCdd().SetSequences()));
                    *newAlignments = mime->GetGeneral().GetSeq_align_data().GetCdd().GetSeqannot();
                }
            }
        } else {
            ERR_POST(Warning << "error: " << err);
        }
    }

    if (!readOK) {
        ERR_POST(Info << "trying to read input as " <<
            ((isBinary) ? "binary" : "ascii") << " cdd");
        CRef < CCdd > cdd(new CCdd);
        SetDiagPostLevel(eDiag_Fatal); // ignore all but Fatal errors while reading data
        asnIstream.seekg(0);
        readOK = ReadASNFromIstream(asnIstream, *cdd, isBinary, err);
        SetDiagPostLevel(defaultDiagPostLevel);
        if (readOK) {
            newSequences->resize(1);
            newSequences->front().Reset(&(cdd->SetSequences()));
            *newAlignments = cdd->GetSeqannot();   // copy the list
        } else {
            ERR_POST(Warning << "error: " << err);
        }
    }

    if (!readOK) {
        ERR_POST(Error << "Input is not a recognized data type (Ncbi-mime-asn1 or Cdd)");
        return CAV_ERROR_BAD_ASN;
    }
    if (newSequences->size() == 0 || newAlignments->size() == 0) {
        ERR_POST(Error << "Cannot find sequences and alignments in the input data!");
        return CAV_ERROR_BAD_ASN;
    }

    *sequences = newSequences.release();
	*alignments = newAlignments.release();
    return CAV_SUCCESS;
}

// checks two things for each slave sequence: that all the residues of the sequence
// are present in the display, and that the aligned residues are in the right place
// wrt the master
static bool VerifyAlignmentData(const AlignmentSet *alignmentSet, const AlignmentDisplay *display)
{
    int alnLoc, masterLoc, slaveLoc, currentMasterLoc, currentSlaveLoc;
    char masterChar, slaveChar;
    const MasterSlaveAlignment *alignment;

    for (int i=0; i<alignmentSet->alignments.size(); ++i) {
        masterLoc = slaveLoc = -1;
        alignment = alignmentSet->alignments[i];

        for (alnLoc=0; alnLoc<display->GetWidth(); ++alnLoc) {

            // get and check characters
            masterChar = display->GetCharAt(alnLoc, 0);
            if (masterChar == '?') {
                ERR_POST(Error << "bad alignment coordinate: loc " << alnLoc << " row " << 0);
                return false;
            }
            slaveChar = display->GetCharAt(alnLoc, 1 + i);
            if (slaveChar == '?') {
                ERR_POST(Error << "bad alignment coordinate: loc " << alnLoc << " row " << (1+i));
                return false;
            }

            // advance seqLocs, check sequence string length and composition
            if (!IsGap(masterChar)) {
                ++masterLoc;
                if (i == 0) {   // only need to check master once
                    if (masterLoc >= alignment->master->sequenceString.size()) {
                        ERR_POST(Error << "master sequence too long at alnLoc " << alnLoc
                            << " row " << (i+1) << "masterLoc" << masterLoc);
                        return false;
                    } else if (toupper(masterChar) != toupper(alignment->master->sequenceString[masterLoc])) {
                        ERR_POST(Error << "master sequence mismatch at alnLoc " << alnLoc
                            << " row " << (i+1) << "masterLoc" << masterLoc);
                        return false;
                    }
                }
            }
            if (!IsGap(slaveChar)) {
                ++slaveLoc;
                if (slaveLoc >= alignment->slave->sequenceString.size()) {
                    ERR_POST(Error << "slave sequence too long at alnLoc " << alnLoc
                        << " row " << (i+1) << "slaveLoc" << slaveLoc);
                    return false;
                } else if (toupper(slaveChar) != toupper(alignment->slave->sequenceString[slaveLoc])) {
                    ERR_POST(Error << "slave sequence mismatch at alnLoc " << alnLoc
                        << " row " << (i+1) << "slaveLoc" << slaveLoc);
                    return false;
                }
            }
            currentMasterLoc = IsGap(masterChar) ? -1 : masterLoc;
            currentSlaveLoc = IsGap(slaveChar) ? -1 : slaveLoc;

            // check display characters, to see if they match alignment data
            if (IsGap(slaveChar) || IsUnaligned(slaveChar)) {
                if (currentMasterLoc >= 0 && alignment->masterToSlave[currentMasterLoc] != -1) {
                    ERR_POST(Error << "slave should be marked aligned at alnLoc " << alnLoc
                        << " row " << (i+1));
                    return false;
                }
            }
            if (IsAligned(slaveChar)) {
                if (!IsAligned(masterChar)) {
                    ERR_POST(Error <<" slave marked aligned but master unaligned at alnLoc " << alnLoc
                        << " row " << (i+1));
                    return false;
                }
                if (alignment->masterToSlave[currentMasterLoc] == -1) {
                    ERR_POST(Error << "slave incorrectly marked aligned at alnLoc " << alnLoc
                        << " row " << (i+1));
                    return false;
                }
                if (alignment->masterToSlave[currentMasterLoc] != currentSlaveLoc) {
                    ERR_POST(Error << "wrong slave residue aligned at alnLoc " << alnLoc
                        << " row " << (i+1));
                    return false;
                }
            }

            // converse: make sure alignment data is correctly reflected in display
            if (!IsGap(masterChar)) {
                if (alignment->masterToSlave[currentMasterLoc] == -1) {
                    if (IsAligned(slaveChar)) {
                        ERR_POST(Error << "slave should be unaligned at alnLoc " << alnLoc
                            << " row " << (i+1));
                        return false;
                    }
                } else {    // aligned master
                    if (!IsAligned(slaveChar)) {
                        ERR_POST(Error << "slave should be aligned at alnLoc " << alnLoc
                            << " row " << (i+1));
                        return false;
                    }
                    if (currentSlaveLoc != alignment->masterToSlave[currentMasterLoc]) {
                        ERR_POST(Error << "wrong slave residue aligned to master at alnLoc " << alnLoc
                            << " row " << (i+1));
                        return false;
                    }
                }
            }
        }

        // check seequence lengths
        if (masterLoc != alignment->master->sequenceString.size() - 1 ||
            slaveLoc != alignment->slave->sequenceString.size() - 1) {
            ERR_POST(Error << "bad sequence lengths at row " << (i+1));
            return false;
        }
    }
    return true;
}

END_NCBI_SCOPE


// leave the main function outside the NCBI namespace, just in case that might
// cause any problems when linking it to C code...
USING_NCBI_SCOPE;

int CAV_DisplayMultiple(
    const SeqEntryList& sequences,
    const SeqAnnotList& alignments,
    unsigned int options,
    unsigned int paragraphWidth,
    double conservationThreshhold,
    const char *title,
    int nFeatures,
    const AlignmentFeature *alnFeatures,
    CNcbiOstream *outputStream,
    CNcbiOstream *diagnosticStream)
{
    // make sure C++ output streams are sync'ed with C's stdio
    IOS_BASE::sync_with_stdio(true);

    // set up output streams (send all diagnostic messages to a different stream)
    CNcbiOstream *outStream;
    if (outputStream)
        outStream = outputStream;
    else
        outStream = &NcbiCout;
    if (diagnosticStream)
        SetDiagStream(diagnosticStream);
    else
        SetDiagStream(&NcbiCerr);
    if (options & CAV_DEBUG)
        SetDiagPostLevel(defaultDiagPostLevel = eDiag_Info);   // show all messages
    else
        SetDiagPostLevel(defaultDiagPostLevel = eDiag_Error);  // show only errors

    // check option consistency
    if (options & CAV_CONDENSED && !(options & CAV_TEXT || options & CAV_HTML)) {
        ERR_POST(Error << "Cannot do condensed display except with text/HTML output");
        return CAV_ERROR_BAD_PARAMS;
    }
    if (options & CAV_FASTA_LOWERCASE && !(options & CAV_FASTA)) {
        ERR_POST(Error << "Cannot do fasta_lc option except with FASTA output");
        return CAV_ERROR_BAD_PARAMS;
    }
    if (options & CAV_HTML_HEADER && !(options & CAV_HTML)) {
        ERR_POST(Error << "Cannot do HTML header without HTML output");
        return CAV_ERROR_BAD_PARAMS;
    }

    // process asn data
    auto_ptr<SequenceSet> sequenceSet(new SequenceSet(sequences));
    if (!sequenceSet.get() || sequenceSet->Status() != CAV_SUCCESS) {
        ERR_POST(Critical << "Error processing sequence data");
        return sequenceSet->Status();
    }
    auto_ptr<AlignmentSet> alignmentSet(new AlignmentSet(sequenceSet.get(), alignments));
    if (!alignmentSet.get() || alignmentSet->Status() != CAV_SUCCESS) {
        ERR_POST(Critical << "Error processing alignment data");
        return alignmentSet->Status();
    }

    // create the alignment display structure
    auto_ptr<AlignmentDisplay> display(new AlignmentDisplay(sequenceSet.get(), alignmentSet.get()));
    if (!display.get() || display->Status() != CAV_SUCCESS) {
        ERR_POST(Critical << "Error creating alignment display");
        return display->Status();
    }

    // do verification
    if (options & CAV_DEBUG) {
        if (!VerifyAlignmentData(alignmentSet.get(), display.get())) {
            ERR_POST(Critical << "AlignmentDisplay failed verification");
            return CAV_ERROR_DISPLAY;
        } else {
            ERR_POST(Info << "AlignmentDisplay passed verification");
        }
    }

    // display alignment with given parameters
    ERR_POST(Info << "writing output...");
    int
        from = (options & CAV_LEFTTAILS) ? 0 : display->GetFirstAlignedLoc(),
        to = (options & CAV_RIGHTTAILS) ? display->GetWidth()-1 : display->GetLastAlignedLoc();
    if (options & CAV_SHOW_IDENTITY) conservationThreshhold = AlignmentDisplay::SHOW_IDENTITY;
    int retval;
    if (options & CAV_TEXT || options & CAV_HTML) {
        if (options & CAV_CONDENSED)
            retval = display->DumpCondensed(*outStream, options,
                from, to, paragraphWidth, conservationThreshhold, title, nFeatures, alnFeatures);
        else
            retval = display->DumpText(*outStream, options,
                from, to, paragraphWidth, conservationThreshhold, title, nFeatures, alnFeatures);
    } else if (options & CAV_FASTA) {
        retval = display->DumpFASTA(from, to, paragraphWidth,
            ((options & CAV_FASTA_LOWERCASE) > 0), *outStream);
    }
//    if (outStream != &NcbiCout) delete outStream;
    if (retval != CAV_SUCCESS) {
        ERR_POST(Error << "Error dumping display to output");
        return retval;
    }

    return CAV_SUCCESS;
}

int CAV_DisplayMultiple(
    const void *asnDataBlock,
    unsigned int options,
    unsigned int paragraphWidth,
    double conservationThreshhold,
    const char *title,
    int nFeatures,
    const AlignmentFeature *alnFeatures,
    CNcbiOstream *outputStream,
    CNcbiOstream *diagnosticStream)
{
    // load input data into an input stream
    if (!asnDataBlock) {
        ERR_POST(Critical << "NULL asnDataBlock parameter");
        return CAV_ERROR_BAD_ASN;
    }
    CNcbiIstrstream asnIstrstream(static_cast<const char*>(asnDataBlock), kMax_Int);

    // load asn data block
    const SeqEntryList *seqs;
    const SeqAnnotList *alns;
    int retval = LoadASNFromIstream(asnIstrstream, &seqs, &alns);
    if (retval != CAV_SUCCESS) {
        ERR_POST(Critical << "Couldn't get sequence and alignment ASN data");
        return retval;
    }

    // make sure these get freed
    auto_ptr<const SeqEntryList> sequences(seqs);
    auto_ptr<const SeqAnnotList> alignments(alns);

    return CAV_DisplayMultiple(*seqs, *alns, options, paragraphWidth, conservationThreshhold,
        title, nFeatures, alnFeatures, outputStream, diagnosticStream);
}

int CAV_DisplayMultiple(
    const void *asnDataBlock,
    unsigned int options,
    unsigned int paragraphWidth,
    double conservationThreshhold,
    const char *title,
    int nFeatures,
    const AlignmentFeature *features)
{
    return CAV_DisplayMultiple(asnDataBlock, options, paragraphWidth,
        conservationThreshhold, title, nFeatures, features, NULL, NULL);
}

int CAV_DisplayMultiple(
    const ncbi::objects::CNcbi_mime_asn1& mime,
    unsigned int options,
    unsigned int paragraphWidth,
    double conservationThreshhold,
    const char *title,
    int nFeatures,
    const AlignmentFeature *features,
    ncbi::CNcbiOstream *outputStream,
    ncbi::CNcbiOstream *diagnosticStream)
{
    const SeqEntryList *sequences = NULL;
    SeqEntryList localSeqList;
    const SeqAnnotList *alignments = NULL;

    if (mime.IsStrucseqs()) {
        sequences = &(mime.GetStrucseqs().GetSequences());
        alignments = &(mime.GetStrucseqs().GetSeqalign());
    } else if (mime.IsAlignstruc()) {
        sequences = &(mime.GetAlignstruc().GetSequences());
        alignments = &(mime.GetAlignstruc().GetSeqalign());
    } else if (mime.IsAlignseq()) {
        sequences = &(mime.GetAlignseq().GetSequences());
        alignments = &(mime.GetAlignseq().GetSeqalign());
    } else if (mime.IsGeneral()) {
        if (mime.GetGeneral().GetSeq_align_data().IsBundle()) {
            sequences = &(mime.GetGeneral().GetSeq_align_data().GetBundle().GetSequences());
            alignments = &(mime.GetGeneral().GetSeq_align_data().GetBundle().GetSeqaligns());
        } else if (mime.GetGeneral().GetSeq_align_data().IsCdd()) {
            localSeqList.resize(1);
            localSeqList.front().Reset(const_cast<CSeq_entry*>(&(mime.GetGeneral().GetSeq_align_data().GetCdd().GetSequences())));
            sequences = &localSeqList;
            alignments = &(mime.GetGeneral().GetSeq_align_data().GetCdd().GetSeqannot());
        }
    }

    if (!sequences || !alignments) {
        ERR_POST(Error << "Ncbi-mime-asn1 object is not of recognized type");
        return CAV_ERROR_BAD_ASN;
    }

    return CAV_DisplayMultiple(*sequences, *alignments, options, paragraphWidth, conservationThreshhold,
        title, nFeatures, features, outputStream, diagnosticStream);
}

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2005/03/02 18:04:50  thiessen
* add variant taking mime C++ object
*
* Revision 1.5  2004/10/04 22:45:58  thiessen
* deal with new general mime type
*
* Revision 1.4  2004/05/21 21:42:51  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.3  2004/03/15 18:51:27  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.2  2003/06/02 16:06:41  dicuccio
* Rearranged src/objects/ subtree.  This includes the following shifts:
*     - src/objects/asn2asn --> arc/app/asn2asn
*     - src/objects/testmedline --> src/objects/ncbimime/test
*     - src/objects/objmgr --> src/objmgr
*     - src/objects/util --> src/objmgr/util
*     - src/objects/alnmgr --> src/objtools/alnmgr
*     - src/objects/flat --> src/objtools/flat
*     - src/objects/validator --> src/objtools/validator
*     - src/objects/cddalignview --> src/objtools/cddalignview
* In addition, libseq now includes six of the objects/seq... libs, and libmmdb
* replaces the three libmmdb? libs.
*
* Revision 1.1  2003/03/19 19:04:12  thiessen
* move again
*
* Revision 1.2  2003/03/19 16:06:28  thiessen
* add <memory>
*
* Revision 1.1  2003/03/19 05:33:43  thiessen
* move to src/app/cddalignview
*
* Revision 1.11  2003/03/18 22:35:06  thiessen
* add C++ version of function that takes streams for output
*
* Revision 1.10  2003/02/03 17:52:03  thiessen
* move CVS Log to end of file
*
* Revision 1.9  2003/01/21 18:01:07  thiessen
* add condensed alignment display
*
* Revision 1.8  2002/11/08 19:38:11  thiessen
* add option for lowercase unaligned in FASTA
*
* Revision 1.7  2002/02/08 19:53:17  thiessen
* add annotation to text/HTML displays
*
* Revision 1.6  2001/05/17 15:01:41  lavr
* Typos corrected
*
* Revision 1.5  2001/03/02 01:19:24  thiessen
* add FASTA output
*
* Revision 1.4  2001/02/15 19:23:44  thiessen
* add identity coloring
*
* Revision 1.3  2001/02/14 16:06:10  thiessen
* add block and conservation coloring to HTML display
*
* Revision 1.2  2001/01/29 23:55:10  thiessen
* add AlignmentDisplay verification
*
* Revision 1.1  2001/01/29 18:13:34  thiessen
* split into C-callable library + main
*
*/
