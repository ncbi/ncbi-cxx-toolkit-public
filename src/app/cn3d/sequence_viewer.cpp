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
*      implementation of non-GUI part of main sequence/alignment viewer
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.50  2002/04/22 18:01:55  thiessen
* add default extension for alignment export
*
* Revision 1.49  2002/04/22 14:27:28  thiessen
* add alignment export
*
* Revision 1.48  2002/03/07 19:16:04  thiessen
* don't auto-show sequence windows
*
* Revision 1.47  2002/03/04 15:52:14  thiessen
* hide sequence windows instead of destroying ; add perspective/orthographic projection choice
*
* Revision 1.46  2002/03/01 15:48:05  thiessen
* revert to single line per sequence
*
* Revision 1.45  2002/02/28 19:11:52  thiessen
* wrap sequences in single-structure mode
*
* Revision 1.44  2001/12/06 23:13:45  thiessen
* finish import/align new sequences into single-structure data; many small tweaks
*
* Revision 1.43  2001/11/30 14:02:05  thiessen
* progress on sequence imports to single structures
*
* Revision 1.42  2001/10/01 16:04:24  thiessen
* make CDD annotation window non-modal; add SetWindowTitle to viewers
*
* Revision 1.41  2001/04/05 22:55:36  thiessen
* change bg color handling ; show geometry violations
*
* Revision 1.40  2001/04/04 00:27:14  thiessen
* major update - add merging, threader GUI controls
*
* Revision 1.39  2001/03/30 03:07:34  thiessen
* add threader score calculation & sorting
*
* Revision 1.38  2001/03/13 01:25:06  thiessen
* working undo system for >1 alignment (e.g., update window)
*
* Revision 1.37  2001/03/09 15:49:04  thiessen
* major changes to add initial update viewer
*
* Revision 1.36  2001/03/02 15:32:52  thiessen
* minor fixes to save & show/hide dialogs, wx string headers
*
* Revision 1.35  2001/03/01 20:15:51  thiessen
* major rearrangement of sequence viewer code into base and derived classes
*
* Revision 1.34  2001/02/13 01:03:57  thiessen
* backward-compatible domain ID's in output; add ability to delete rows
*
* Revision 1.33  2001/02/08 23:01:51  thiessen
* hook up C-toolkit stuff for threading; working PSSM calculation
*
* Revision 1.32  2001/01/26 19:29:59  thiessen
* limit undo stack size ; fix memory leak
*
* Revision 1.31  2000/12/29 19:23:39  thiessen
* save row order
*
* Revision 1.30  2000/12/21 23:42:16  thiessen
* load structures from cdd's
*
* Revision 1.29  2000/12/15 15:51:47  thiessen
* show/hide system installed
*
* Revision 1.28  2000/11/30 15:49:39  thiessen
* add show/hide rows; unpack sec. struc. and domain features
*
* Revision 1.27  2000/11/17 19:48:14  thiessen
* working show/hide alignment row
*
* Revision 1.26  2000/11/12 04:02:59  thiessen
* working file save including alignment edits
*
* Revision 1.25  2000/11/11 21:15:54  thiessen
* create Seq-annot from BlockMultipleAlignment
*
* Revision 1.24  2000/11/03 01:12:44  thiessen
* fix memory problem with alignment cloning
*
* Revision 1.23  2000/11/02 16:56:02  thiessen
* working editor undo; dynamic slave transforms
*
* Revision 1.22  2000/10/19 12:40:54  thiessen
* avoid multiple sequence redraws with scroll set
*
* Revision 1.21  2000/10/17 14:35:06  thiessen
* added row shift - editor basically complete
*
* Revision 1.20  2000/10/16 20:03:07  thiessen
* working block creation
*
* Revision 1.19  2000/10/12 19:20:45  thiessen
* working block deletion
*
* Revision 1.18  2000/10/12 16:22:45  thiessen
* working block split
*
* Revision 1.17  2000/10/12 02:14:56  thiessen
* working block boundary editing
*
* Revision 1.16  2000/10/05 18:34:43  thiessen
* first working editing operation
*
* Revision 1.15  2000/10/04 17:41:30  thiessen
* change highlight color (cell background) handling
*
* Revision 1.14  2000/10/03 18:59:23  thiessen
* added row/column selection
*
* Revision 1.13  2000/10/02 23:25:22  thiessen
* working sequence identifier window in sequence viewer
*
* Revision 1.12  2000/09/20 22:22:27  thiessen
* working conservation coloring; split and center unaligned justification
*
* Revision 1.11  2000/09/15 19:24:22  thiessen
* allow repeated structures w/o different local id
*
* Revision 1.10  2000/09/14 14:55:34  thiessen
* add row reordering; misc fixes
*
* Revision 1.9  2000/09/12 01:47:38  thiessen
* fix minor but obscure bug
*
* Revision 1.8  2000/09/11 22:57:33  thiessen
* working highlighting
*
* Revision 1.7  2000/09/11 14:06:29  thiessen
* working alignment coloring
*
* Revision 1.6  2000/09/11 01:46:16  thiessen
* working messenger for sequence<->structure window communication
*
* Revision 1.5  2000/09/08 20:16:55  thiessen
* working dynamic alignment views
*
* Revision 1.4  2000/09/07 17:37:35  thiessen
* minor fixes
*
* Revision 1.3  2000/09/03 18:46:49  thiessen
* working generalized sequence viewer
*
* Revision 1.2  2000/08/30 23:46:28  thiessen
* working alignment display
*
* Revision 1.1  2000/08/30 19:48:42  thiessen
* working sequence window
*
* ===========================================================================
*/

#include <wx/string.h> // kludge for now to fix weird namespace conflict
#include <corelib/ncbistd.hpp>

#include "cn3d/sequence_viewer.hpp"
#include "cn3d/sequence_viewer_window.hpp"
#include "cn3d/sequence_display.hpp"
#include "cn3d/messenger.hpp"
#include "cn3d/alignment_manager.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/molecule_identifier.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/sequence_set.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

SequenceViewer::SequenceViewer(AlignmentManager *alnMgr) :
    // not sure why this cast is necessary, but MSVC requires it...
    ViewerBase(reinterpret_cast<ViewerWindowBase**>(&sequenceWindow), alnMgr),
    sequenceWindow(NULL)
{
}

SequenceViewer::~SequenceViewer(void)
{
}

void SequenceViewer::CreateSequenceWindow(void)
{
    if (sequenceWindow) {
        sequenceWindow->Show(true);
        GlobalMessenger()->PostRedrawSequenceViewer(this);
    } else {
        SequenceDisplay *display = GetCurrentDisplay();
        if (display) {
            sequenceWindow = new SequenceViewerWindow(this);
            sequenceWindow->NewDisplay(display, true);
            sequenceWindow->ScrollToColumn(display->GetStartingColumn());
            sequenceWindow->Show(true);
            // ScrollTo causes immediate redraw, so don't need a second one
            GlobalMessenger()->UnPostRedrawSequenceViewer(this);
        }
    }
}

void SequenceViewer::Refresh(void)
{
    if (sequenceWindow) sequenceWindow->Refresh();
}

void SequenceViewer::SaveDialog(void)
{
    if (sequenceWindow) sequenceWindow->SaveDialog(false);
}

void SequenceViewer::SaveAlignment(void)
{
    KeepOnlyStackTop();

    // go back into the original pairwise alignment data and save according to the
    // current edited BlockMultipleAlignment and display row order
    std::vector < int > rowOrder;
    const SequenceDisplay *display = GetCurrentDisplay();
    for (int i=0; i<display->rows.size(); i++) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(display->rows[i]);
        if (alnRow) rowOrder.push_back(alnRow->row);
    }
    alignmentManager->SavePairwiseFromMultiple(alignmentStack.back().back(), rowOrder);
}

void SequenceViewer::DisplayAlignment(BlockMultipleAlignment *alignment)
{
    ClearStacks();

    SequenceDisplay *display = new SequenceDisplay(true, viewerWindow);
    for (int row=0; row<alignment->NRows(); row++)
        display->AddRowFromAlignment(row, alignment);

    // set starting scroll to a few residues left of the first aligned block
    display->SetStartingColumn(alignment->GetFirstAlignedBlockPosition() - 5);

    AlignmentList alignments;
    alignments.push_back(alignment);
    InitStacks(&alignments, display);
    if (sequenceWindow)
        sequenceWindow->UpdateDisplay(display);
    else
        CreateSequenceWindow();

    // allow alignment export
    sequenceWindow->EnableExport(true);
}

void SequenceViewer::DisplaySequences(const SequenceList *sequenceList)
{
//    int from, to, width = 50;

    SequenceDisplay *display = new SequenceDisplay(false, viewerWindow);

    // populate each line of the display with one sequence, with blank lines inbetween
    SequenceList::const_iterator s, se = sequenceList->end();
    for (s=sequenceList->begin(); s!=se; s++) {

        // only do sequences from structure if this is a single-structure data file
        if (!(*s)->parentSet->IsMultiStructure() &&
            (*s)->parentSet->objects.front()->mmdbID != (*s)->identifier->mmdbID) continue;

        if (display->NRows() > 0) display->AddRowFromString("");

        // wrap long sequences
//        from = 0;
//        to = width - 1;
//        do {
//            display->AddRowFromSequence(*s, from, ((to >= (*s)->Length()) ? (*s)->Length() - 1 : to));
//            from += width;
//            to += width;
//        } while (from < (*s)->Length());

        // whole sequence on one row
        display->AddRowFromSequence(*s, 0, (*s)->Length() - 1);
    }

    ClearStacks();
    InitStacks(NULL, display);
    if (sequenceWindow)
        sequenceWindow->UpdateDisplay(display);
    else
        CreateSequenceWindow();

    // forbid alignment export
    sequenceWindow->EnableExport(false);
}

void SequenceViewer::SetWindowTitle(const std::string& title) const
{
    if (*viewerWindow)
        (*viewerWindow)->SetTitle(wxString(title.c_str()) + " - Sequence/Alignment Viewer");
}

void SequenceViewer::TurnOnEditor(void)
{
    if (!sequenceWindow) CreateSequenceWindow();
    sequenceWindow->TurnOnEditor();
}

static void DumpFASTA(const BlockMultipleAlignment *alignment,
    BlockMultipleAlignment::eUnalignedJustification justification, CNcbiOstream& os)
{
    // do whole alignment for now
    int firstCol = 0, lastCol = alignment->AlignmentWidth() - 1, nColumns = 70;

    if (firstCol < 0 || lastCol >= alignment->AlignmentWidth() || firstCol > lastCol || nColumns < 1) {
        ERR_POST(Error << "DumpFASTA() - nonsensical display region parameters");
        return;
    }

    // output each alignment row
    int paragraphStart, nParags = 0, i;
    char ch;
    Vector color, bgColor;
    bool highlighted, drawBG;
    for (int row=0; row<alignment->NRows(); row++) {

        // create title line
        os << '>';
        const Sequence *seq = alignment->GetSequenceOfRow(row);
        bool prevID = false;
        if (seq->identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
            os << "gi|" << seq->identifier->gi;
            prevID = true;
        }
        if (seq->identifier->pdbID.size() > 0) {
            if (prevID) os << '|';
            if (seq->identifier->pdbID == "query" || seq->identifier->pdbID == "consensus") {
                os << "lcl|" << seq->identifier->pdbID;
            } else {
                os << "pdb|" << seq->identifier->pdbID;
                if (seq->identifier->pdbChain != ' ')
                    os << '|' << (char) seq->identifier->pdbChain << " Chain "
                       << (char) seq->identifier->pdbChain << ',';
            }
            prevID = true;
        }
        if (seq->identifier->accession.size() > 0) {
            if (prevID) os << '|';
            os << seq->identifier->accession;
            prevID = true;
        }
        if (seq->description.size() > 0)
            os << ' ' << seq->description;
        os << '\n';

        // split alignment up into "paragraphs", each with nColumns
        for (paragraphStart=0; (firstCol+paragraphStart)<=lastCol; paragraphStart+=nColumns, nParags++) {
            for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++) {
                if (alignment->GetCharacterTraitsAt(firstCol+paragraphStart+i, row, justification,
                        &ch, &color, &highlighted, &drawBG, &bgColor)) {
                    if (ch == '~') ch = '-';
                    os << (char) toupper(ch);
                } else
                    os << '?';
            }
            os << '\n';
        }
    }
}

static void DumpText(bool doHTML, const BlockMultipleAlignment *alignment,
    BlockMultipleAlignment::eUnalignedJustification justification, CNcbiOstream& os)
{

#define LEFT_JUSTIFY resetiosflags(IOS_BASE::right) << setiosflags(IOS_BASE::left)
#define RIGHT_JUSTIFY resetiosflags(IOS_BASE::left) << setiosflags(IOS_BASE::right)

    // do whole alignment for now
    int firstCol = 0, lastCol = alignment->AlignmentWidth() - 1, nColumns = 60;

    if (firstCol < 0 || lastCol >= alignment->AlignmentWidth() || firstCol > lastCol || nColumns < 1) {
        ERR_POST(Error << "DumpText() - nonsensical display region parameters");
        return;
    }

    // HTML colors
    static const std::string
        bgColor("#FFFFFF"), rulerColor("#700777"), numColor("#229922");

    // set up the titles and uids, figure out how much space any seqLoc string will take
    std::vector < std::string > titles(alignment->NRows()), uids(doHTML ? alignment->NRows() : 0);
    int row, maxTitleLength = 0, maxSeqLocStrLength = 0, leftMargin, decimalLength;
    for (row=0; row<alignment->NRows(); row++) {
        const Sequence *sequence = alignment->GetSequenceOfRow(row);

        titles[row] = sequence->identifier->ToString();
        if (titles[row].size() > maxTitleLength) maxTitleLength = titles[row].size();
        decimalLength = ((int) log10((double) sequence->Length())) + 1;
        if (decimalLength > maxSeqLocStrLength) maxSeqLocStrLength = decimalLength;

        // uid for link to entrez
        if (doHTML) {
            if (sequence->identifier->pdbID.size() > 0) {
                if (sequence->identifier->pdbID != "query" &&
                    sequence->identifier->pdbID != "consensus") {
                    uids[row] = sequence->identifier->pdbID;
                    if (sequence->identifier->pdbChain != ' ')
                        uids[row] += (char) sequence->identifier->pdbChain;
                }
            } else if (sequence->identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
                CNcbiOstrstream uidoss;
                uidoss << sequence->identifier->gi << '\0';
                uids[row] = uidoss.str();
                delete uidoss.str();
            } else if (sequence->identifier->accession.size() > 0) {
                uids[row] = sequence->identifier->accession;
            }
        }
    }
    leftMargin = maxTitleLength + maxSeqLocStrLength + 2;

    // need to keep track of first, last seqLocs for each row in each paragraph;
    // find seqLoc of first residue >= firstCol
    std::vector < int > lastShownSeqLocs(alignment->NRows());
    int alnLoc, i;
    char ch;
    Vector color, bgCol;
    bool highlighted, drawBG;
    for (row=0; row<alignment->NRows(); row++) {
        lastShownSeqLocs[row] = -1;
        for (alnLoc=0; alnLoc<firstCol; alnLoc++) {
            if (!alignment->GetCharacterTraitsAt(alnLoc, row, justification,
                    &ch, &color, &highlighted, &drawBG, &bgCol))
                ch = '~';
            if (ch != '~') lastShownSeqLocs[row]++;
        }
    }

    // header
    if (doHTML)
        os << "<HTML><TITLE>Alignment Exported From Cn3D</TITLE>\n" <<
            "<BODY BGCOLOR=" << bgColor << ">\n";

    // split alignment up into "paragraphs", each with nColumns
    if (doHTML) os << "<TABLE>\n";
    int paragraphStart, nParags = 0;
    for (paragraphStart=0; (firstCol+paragraphStart)<=lastCol; paragraphStart+=nColumns, nParags++) {

        // start table row
        if (doHTML)
            os << "<tr><td><pre>\n";
        else
            if (paragraphStart > 0) os << '\n';

        // do ruler
        int nMarkers = 0, width;
        if (doHTML) os << "<font color=" << rulerColor << '>';
        for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++) {
            if ((paragraphStart+i+1)%10 == 0) {
                if (nMarkers == 0)
                    width = leftMargin + i + 1;
                else
                    width = 10;
                os << RIGHT_JUSTIFY << setw(width) << (paragraphStart+i+1);
                nMarkers++;
            }
        }
        if (doHTML) os << "</font>";
        os << '\n';
        if (doHTML) os << "<font color=" << rulerColor << '>';
        for (i=0; i<leftMargin; i++) os << ' ';
        for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++) {
            if ((paragraphStart+i+1)%10 == 0)
                os << '|';
            else if ((paragraphStart+i+1)%5 == 0)
                os << '*';
            else
                os << '.';
        }
        if (doHTML) os << "</font>";
        os << '\n';

        int nDisplayedResidues;

        // output each alignment row
        for (row=0; row<alignment->NRows(); row++) {
            const Sequence *sequence = alignment->GetSequenceOfRow(row);

            // actual sequence characters and colors; count how many non-gaps in each row
            nDisplayedResidues = 0;
            std::string rowChars;
            std::vector < std::string > rowColors;
            for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; i++) {
                if (!alignment->GetCharacterTraitsAt(firstCol+paragraphStart+i, row, justification,
                        &ch, &color, &highlighted, &drawBG, &bgCol))
                    ch = '?';
                rowChars += ch;
                wxString colorStr;
                colorStr.Printf("#%02x%02x%02x",
                    (int) (color[0]*255), (int) (color[1]*255), (int) (color[2]*255));
                rowColors.push_back(colorStr.c_str());
                if (ch != '~') nDisplayedResidues++;
            }

            // title
            if (doHTML && uids[row].size() > 0) {
                os << "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/utils/qmap.cgi?uid="
                    << uids[row] << "&form=6&db=p&Dopt=g\" onMouseOut=\"window.status=''\"\n"
                    << "onMouseOver=\"window.status='"
                    << ((sequence->description.size() > 0) ? sequence->description : titles[row])
                    << "';return true\">"
                    << setw(0) << titles[row] << "</a>";
            } else {
                os << setw(0) << titles[row];
            }
            os << setw(maxTitleLength+1-titles[row].size()) << ' ';

            // left start pos (output 1-numbered for humans...)
            if (doHTML) os << "<font color=" << numColor << '>';
            if (nDisplayedResidues > 0)
                os << RIGHT_JUSTIFY << setw(maxSeqLocStrLength) << (lastShownSeqLocs[row]+2) << ' ';
            else
                os << RIGHT_JUSTIFY << setw(maxSeqLocStrLength) << ' ' << ' ';

            // dump sequence, applying color changes only when necessary
            if (doHTML) {
                std::string prevColor;
                for (i=0; i<rowChars.size(); i++) {
                    if (rowColors[i] != prevColor) {
                        os << "</font><font color=" << rowColors[i] << '>';
                        prevColor = rowColors[i];
                    }
                    os << rowChars[i];
                }
                os << "</font>";
            } else
                os << rowChars;

            // right end pos
            if (nDisplayedResidues > 0) {
                os << ' ';
                if (doHTML) os << "<font color=" << numColor << '>';
                os << LEFT_JUSTIFY << setw(0) << (lastShownSeqLocs[row]+nDisplayedResidues+1);
                if (doHTML) os << "</font>";
            }
            os << '\n';

            // setup to begin next parag
            lastShownSeqLocs[row] += nDisplayedResidues;
        }

        // end table row
        if (doHTML) os << "</pre></td></tr>\n";
    }

    if (doHTML)
        os << "</TABLE>\n"
            << "</BODY></HTML>\n";

    // additional sanity check on seqloc markers
    if (firstCol == 0 && lastCol == alignment->AlignmentWidth()-1) {
        for (row=0; row<alignment->NRows(); row++) {
            if (lastShownSeqLocs[row] !=
                    alignment->GetSequenceOfRow(row)->Length()-1) {
                ERR_POST(Error << "DumpText: full display - seqloc markers don't add up for row " << row);
                break;
            }
        }
        if (row != alignment->NRows())
            ERR_POST(Error << "DumpText: full display - seqloc markers don't add up correctly");
    }
}

void SequenceViewer::ExportAlignment(bool asFASTA, bool asTEXT, bool asHTML)
{
    int nOpt = 0;
    if (asFASTA) nOpt++;
    if (asTEXT) nOpt++;
    if (asHTML) nOpt++;
    if (nOpt != 1) {
        ERR_POST(Error << "SequenceViewer::ExportAlignment() - exactly one export type must be chosen");
        return;
    }

    // get filename
    wxString extension, wildcard;
    if (asFASTA) { extension = ".fasta"; wildcard = "FASTA Files (*.fasta)|*.fasta"; }
    else if (asTEXT) { extension = ".txt"; wildcard = "Text Files (*.txt)|*.txt"; }
    else if (asHTML) { extension = ".html"; wildcard = "HTML Files (*.html)|*.html"; }
    wxString filename = wxFileSelector("Choose a file for alignment export:",
        "", "alignment", extension, wildcard, wxSAVE /*| wxOVERWRITE_PROMPT*/, sequenceWindow);

    if (filename.size() > 0) {

        // add extension, check for existence
        if (asFASTA && filename.Right(6) != ".fasta") filename += ".fasta";
        else if (asTEXT && filename.Right(4) != ".txt") filename += ".txt";
        else if (asHTML && filename.Right(5) != ".html") filename += ".html";
        if (wxFileExists(filename)) {
            wxString message;
            message.Printf("The file %s already exists.\nDo you want to overwrite it?", filename.c_str());
            if (wxMessageBox(message, "Overwrite?", wxYES_NO | wxICON_QUESTION) == wxNO)
                return;
        }

        // create output stream
        auto_ptr<CNcbiOfstream> ofs(new CNcbiOfstream(filename.c_str(), IOS_BASE::out));
        if (!(*ofs)) {
            ERR_POST(Error << "Unable to open output file " << filename.c_str());
            return;
        }

        // actually write the alignment
        if (asFASTA) {
            TESTMSG("exporting FASTA to " << filename.c_str());
            DumpFASTA(alignmentManager->GetCurrentMultipleAlignment(),
                sequenceWindow->GetCurrentJustification(), *ofs);
        } else {
            TESTMSG("exporting " << (asTEXT ? "text" : "HTML") << " to " << filename.c_str());
            DumpText(asHTML, alignmentManager->GetCurrentMultipleAlignment(),
                sequenceWindow->GetCurrentJustification(), *ofs);
        }
    }
}

END_SCOPE(Cn3D)

