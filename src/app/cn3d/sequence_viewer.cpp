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
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <memory>

#include <objects/seq/Bioseq.hpp>

#include "remove_header_conflicts.hpp"

#include "sequence_viewer.hpp"
#include "sequence_viewer_window.hpp"
#include "sequence_display.hpp"
#include "messenger.hpp"
#include "alignment_manager.hpp"
#include "structure_set.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_tools.hpp"
#include "sequence_set.hpp"
#include "cn3d_pssm.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


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

void SequenceViewer::CreateSequenceWindow(bool showNow)
{
    if (sequenceWindow) {
        sequenceWindow->Show(showNow);
        if (showNow)
            GlobalMessenger()->PostRedrawSequenceViewer(this);
    } else {
        SequenceDisplay *display = GetCurrentDisplay();
        if (display) {
            sequenceWindow = new SequenceViewerWindow(this);

#ifdef __WXMAC__
            //  Nudge down a bit to compensate for the menubar at the top of the screen
            wxPoint p = sequenceWindow->GetPosition();
            p.y += 20;
            sequenceWindow->Move(p);
#endif

            sequenceWindow->NewDisplay(display, true);
            sequenceWindow->ScrollToColumn(display->GetStartingColumn());
            sequenceWindow->Show(showNow);
            // ScrollTo causes immediate redraw, so don't need a second one
            GlobalMessenger()->UnPostRedrawSequenceViewer(this);
        }
    }
}

void SequenceViewer::SaveAlignment(void)
{
    KeepCurrent();

    // go back into the original pairwise alignment data and save according to the
    // current edited BlockMultipleAlignment and display row order
    vector < unsigned int > rowOrder;
    const SequenceDisplay *display = GetCurrentDisplay();
    for (unsigned int i=0; i<display->rows.size(); ++i) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(display->rows[i]);
        if (alnRow) rowOrder.push_back(alnRow->row);
    }
    alignmentManager->SavePairwiseFromMultiple(GetCurrentAlignments().back(), rowOrder);
}

void SequenceViewer::DisplayAlignment(BlockMultipleAlignment *alignment)
{
    SequenceDisplay *display = new SequenceDisplay(true, viewerWindow);
    for (unsigned int row=0; row<alignment->NRows(); ++row)
        display->AddRowFromAlignment(row, alignment);

    // set starting scroll to a few residues left of the first aligned block
    display->SetStartingColumn(alignment->GetFirstAlignedBlockPosition() - 5);

    AlignmentList alignments;
    alignments.push_back(alignment);
    InitData(&alignments, display);
    if (sequenceWindow)
        sequenceWindow->UpdateDisplay(display);
    else if (IsWindowedMode())
        CreateSequenceWindow(false);
}

bool SequenceViewer::ReplaceAlignment(const BlockMultipleAlignment *origAln, BlockMultipleAlignment *newAln)
{
    AlignmentList& alignments = GetCurrentAlignments();
    SequenceDisplay *display = GetCurrentDisplay();

    // sanity checks
    if (alignments.size() != 1 || alignments.front() != origAln || !sequenceWindow || !sequenceWindow->EditorIsOn()) {
        ERRORMSG("SequenceViewer::ReplaceAlignment() - bad parameters");
        return false;
    }

    // empty out and then recreate the current alignment list and display (but not the undo stacks!)
    DELETE_ALL_AND_CLEAR(alignments, AlignmentList);
    alignments.push_back(newAln);
    display->Empty();
    display->AddBlockBoundaryRow(newAln);
    for (unsigned int row=0; row<newAln->NRows(); ++row)
        display->AddRowFromAlignment(row, newAln);

    // set starting scroll to a few residues left of the first aligned block
    display->SetStartingColumn(newAln->GetFirstAlignedBlockPosition() - 5);

    Save();    // make this an undoable operation
    if (sequenceWindow)
        sequenceWindow->UpdateDisplay(display);
    return true;
}

void SequenceViewer::DisplaySequences(const SequenceList *sequenceList)
{
    SequenceDisplay *display = new SequenceDisplay(false, viewerWindow);

    // populate each line of the display with one sequence, with blank lines inbetween
    SequenceList::const_iterator s, se = sequenceList->end();
    for (s=sequenceList->begin(); s!=se; ++s) {

        // only do sequences from structure if this is a single-structure data file
        if (!(*s)->parentSet->IsMultiStructure() &&
            (*s)->parentSet->objects.front()->mmdbID != (*s)->identifier->mmdbID) continue;

        if (display->NRows() > 0) display->AddRowFromString("");

        // whole sequence on one row
        display->AddRowFromSequence(*s, 0, (*s)->Length() - 1);
    }

    InitData(NULL, display);
    if (sequenceWindow)
        sequenceWindow->UpdateDisplay(display);
    else if (IsWindowedMode())
        CreateSequenceWindow(false);
}

void SequenceViewer::TurnOnEditor(void)
{
    if (!sequenceWindow) CreateSequenceWindow(false);
    sequenceWindow->TurnOnEditor();
}

static void DumpFASTA(bool isA2M, const BlockMultipleAlignment *alignment,
    const vector < int >& rowOrder,
    BlockMultipleAlignment::eUnalignedJustification justification, CNcbiOstream& os)
{
    // do whole alignment for now
    unsigned int firstCol = 0, lastCol = alignment->AlignmentWidth() - 1, nColumns = 70;
    if (lastCol >= alignment->AlignmentWidth() || firstCol > lastCol || nColumns < 1) {
        ERRORMSG("DumpFASTA() - nonsensical display region parameters");
        return;
    }

    // first fill out ids
    typedef map < const MoleculeIdentifier * , list < string > > IDMap;
    IDMap idMap;
    unsigned int row;
    bool anyRepeat = false;

    for (row=0; row<alignment->NRows(); ++row) {
        const Sequence *seq = alignment->GetSequenceOfRow(row);
        list < string >& titleList = idMap[seq->identifier];
        CNcbiOstrstream oss;
        oss << '>';

        if (titleList.size() == 0) {

            // create full title line for first instance of this sequence
            CBioseq::TId::const_iterator i, ie = seq->bioseqASN->GetId().end();
            for (i=seq->bioseqASN->GetId().begin(); i!=ie; ++i) {
                if (i != seq->bioseqASN->GetId().begin())
                    oss << '|';
                oss << (*i)->AsFastaString();
            }
            string descr = seq->GetDescription();
            if (descr.size() > 0)
                oss << ' ' << descr;

        } else {
            // add repeat id
            oss << "lcl|instance #" << (titleList.size() + 1) << " of " << seq->identifier->ToString();
            anyRepeat = true;
        }

        oss << '\n';
        titleList.resize(titleList.size() + 1);
        titleList.back() = (string) CNcbiOstrstreamToString(oss);
    }

    static const int eAllRepeats=0, eFakeRepeatIDs=1, eNoRepeats=2;
    int choice = eAllRepeats;

    if (anyRepeat) {
        wxArrayString choices;
        choices.Add("Include all repeats with normal IDs");
        choices.Add("Include all repeats, but use unique IDs");
        choices.Add("Include no repeated sequences");
        choice = wxGetSingleChoiceIndex("How do you want to handle repeated sequences?",
            "Choose repeat type", choices);
        if (choice < 0) return; // cancelled
    }

    // output each alignment row (in order of the display)
    unsigned int paragraphStart, nParags = 0, i;
    char ch;
    Vector color, bgColor;
    bool highlighted, drawBG;
    for (row=0; row<alignment->NRows(); ++row) {
        const Sequence *seq = alignment->GetSequenceOfRow(rowOrder[row]);

        // output title
        list < string >& titleList = idMap[seq->identifier];
        if (choice == eAllRepeats) {
            os << titleList.front();    // use full id
        } else if (choice == eFakeRepeatIDs) {
            os << titleList.front();
            titleList.pop_front();      // move to next (fake) id
        } else if (choice == eNoRepeats) {
            if (titleList.size() > 0) {
                os << titleList.front();
                titleList.clear();      // remove all ids after first instance
            } else {
                continue;
            }
        }

        // split alignment up into "paragraphs", each with nColumns
        for (paragraphStart=0; (firstCol+paragraphStart)<=lastCol; paragraphStart+=nColumns, ++nParags) {
            for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; ++i) {
                if (alignment->GetCharacterTraitsAt(firstCol+paragraphStart+i, rowOrder[row], justification,
                        &ch, &color, &highlighted, &drawBG, &bgColor)) {
                    if (ch == '~')
                        os << (isA2M ? '.' : '-');
                    else
                        os << (isA2M ? ch : (char) toupper((unsigned char) ch));
                } else
                    ERRORMSG("GetCharacterTraitsAt failed!");
            }
            os << '\n';
        }
    }
}

static string ShortAndEscapedString(const string& s)
{
    string n;
    for (unsigned int i=0; i<s.size(); ++i) {
        if (s[i] == '\'')
            n += '\\';
        n += s[i];
        if (i > 500)
            break;
    }
    return n;
}

static void DumpText(bool doHTML, const BlockMultipleAlignment *alignment,
    const vector < int >& rowOrder,
    BlockMultipleAlignment::eUnalignedJustification justification, CNcbiOstream& os)
{

#define LEFT_JUSTIFY resetiosflags(IOS_BASE::right) << setiosflags(IOS_BASE::left)
#define RIGHT_JUSTIFY resetiosflags(IOS_BASE::left) << setiosflags(IOS_BASE::right)

    // do whole alignment for now
    unsigned int firstCol = 0, lastCol = alignment->AlignmentWidth() - 1, nColumns = 60;

    if (lastCol >= alignment->AlignmentWidth() || firstCol > lastCol || nColumns < 1) {
        ERRORMSG("DumpText() - nonsensical display region parameters");
        return;
    }

    // HTML colors
    static const string
        bgColor("#FFFFFF"), rulerColor("#700777"), numColor("#229922");

    // set up the titles and uids, figure out how much space any seqLoc string will take
    vector < string > titles(alignment->NRows()), uids(doHTML ? alignment->NRows() : 0);
    unsigned int alnRow, row, maxTitleLength = 0, maxSeqLocStrLength = 0, leftMargin, decimalLength;
    for (alnRow=0; alnRow<alignment->NRows(); ++alnRow) {
        row = rowOrder[alnRow]; // translate display row -> data row
        const Sequence *sequence = alignment->GetSequenceOfRow(row);

        titles[row] = sequence->identifier->ToString();
        if (titles[row].size() > maxTitleLength) maxTitleLength = titles[row].size();
        decimalLength = ((int) log10((double) sequence->Length())) + 1;
        if (decimalLength > maxSeqLocStrLength) maxSeqLocStrLength = decimalLength;

        // uid for link to entrez
        if (doHTML) {
            // prefer gi's, since accessions can be outdated
            if (sequence->identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
                uids[row] = NStr::IntToString(sequence->identifier->gi);
            } else if (sequence->identifier->pdbID.size() > 0) {
                if (sequence->identifier->pdbID != "query" &&
                    sequence->identifier->pdbID != "consensus") {
                    uids[row] = sequence->identifier->pdbID;
                    if (sequence->identifier->pdbChain != ' ')
                        uids[row] += (char) sequence->identifier->pdbChain;
                }
            } else {
                uids[row] = sequence->identifier->GetLabel();
            }
        }
    }
    leftMargin = maxTitleLength + maxSeqLocStrLength + 2;

    // need to keep track of first, last seqLocs for each row in each paragraph;
    // find seqLoc of first residue >= firstCol
    vector < int > lastShownSeqLocs(alignment->NRows());
    unsigned int alnLoc, i;
    char ch;
    Vector color, bgCol;
    bool highlighted, drawBG;
    for (alnRow=0; alnRow<alignment->NRows(); ++alnRow) {
        row = rowOrder[alnRow]; // translate display row -> data row
        lastShownSeqLocs[row] = -1;
        for (alnLoc=0; alnLoc<firstCol; ++alnLoc) {
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
    for (paragraphStart=0; (firstCol+paragraphStart)<=lastCol; paragraphStart+=nColumns, ++nParags) {

        // start table row
        if (doHTML)
            os << "<tr><td><pre>\n";
        else
            if (paragraphStart > 0) os << '\n';

        // do ruler
        unsigned int nMarkers = 0, width;
        if (doHTML) os << "<font color=" << rulerColor << '>';
        for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; ++i) {
            if ((paragraphStart+i+1)%10 == 0) {
                if (nMarkers == 0)
                    width = leftMargin + i + 1;
                else
                    width = 10;
                os << RIGHT_JUSTIFY << setw(width) << (paragraphStart+i+1);
                ++nMarkers;
            }
        }
        if (doHTML) os << "</font>";
        os << '\n';
        if (doHTML) os << "<font color=" << rulerColor << '>';
        for (i=0; i<leftMargin; ++i) os << ' ';
        for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; ++i) {
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
        for (alnRow=0; alnRow<alignment->NRows(); ++alnRow) {
            row = rowOrder[alnRow]; // translate display row -> data row
            const Sequence *sequence = alignment->GetSequenceOfRow(row);

            // actual sequence characters and colors; count how many non-gaps in each row
            nDisplayedResidues = 0;
            string rowChars;
            vector < string > rowColors;
            for (i=0; i<nColumns && (firstCol+paragraphStart+i)<=lastCol; ++i) {
                if (!alignment->GetCharacterTraitsAt(firstCol+paragraphStart+i, row, justification,
                        &ch, &color, &highlighted, &drawBG, &bgCol))
                    ch = '?';
                rowChars += ch;
                wxString colorStr;
                colorStr.Printf("#%02x%02x%02x",
                    (int) (color[0]*255), (int) (color[1]*255), (int) (color[2]*255));
                rowColors.push_back(colorStr.c_str());
                if (ch != '~') ++nDisplayedResidues;
            }

            // title
            if (doHTML && uids[row].size() > 0) {
				string descr = sequence->GetDescription();
                os << "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi"
                    << "?cmd=Search&doptcmdl=GenPept&db=Protein&term="
                    << uids[row] << "\" onMouseOut=\"window.status=''\"\n"
                    << "onMouseOver=\"window.status='"
                    << ShortAndEscapedString((descr.size() > 0) ? descr : titles[row])
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
                string prevColor;
                for (i=0; i<rowChars.size(); ++i) {
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
        for (alnRow=0; alnRow<alignment->NRows(); ++alnRow) {
            row = rowOrder[alnRow]; // translate display row -> data row
            if (lastShownSeqLocs[row] != (int)alignment->GetSequenceOfRow(row)->Length() - 1) {
                ERRORMSG("DumpText: full display - seqloc markers don't add up for row " << row);
                break;
            }
        }
        if (alnRow != alignment->NRows())
            ERRORMSG("DumpText: full display - seqloc markers don't add up correctly");
    }
}

void SequenceViewer::ExportAlignment(eExportType type)
{
    // get filename
    wxString extension, wildcard;
    if (type == asFASTA) { extension = ".fsa"; wildcard = "FASTA Files (*.fsa)|*.fsa"; }
    else if (type == asFASTAa2m) { extension = ".a2m"; wildcard = "A2M FASTA (*.a2m)|*.a2m"; }
    else if (type == asText) { extension = ".txt"; wildcard = "Text Files (*.txt)|*.txt"; }
    else if (type == asHTML) { extension = ".html"; wildcard = "HTML Files (*.html)|*.html"; }
    else if (type == asPSSM) { extension = ".pssm"; wildcard = "PSSM Files (*.pssm)|*.pssm"; }

    wxString outputFolder = wxString(GetUserDir().c_str(), GetUserDir().size() - 1); // remove trailing /
    wxString baseName, outputFile;
    wxSplitPath(GetWorkingFilename().c_str(), NULL, &baseName, NULL);
    wxFileDialog dialog(sequenceWindow, "Choose a file for alignment export:", outputFolder,
#ifdef __WXGTK__
        baseName + extension,
#else
        baseName,
#endif
        wildcard, wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    dialog.SetFilterIndex(0);
    if (dialog.ShowModal() == wxID_OK)
        outputFile = dialog.GetPath();

    if (outputFile.size() > 0) {

        // create output stream
        auto_ptr<CNcbiOfstream> ofs(new CNcbiOfstream(outputFile.c_str(), IOS_BASE::out));
        if (!(*ofs)) {
            ERRORMSG("Unable to open output file " << outputFile.c_str());
            return;
        }

        // map display row order to rows in BlockMultipleAlignment
        vector < int > rowOrder;
        const SequenceDisplay *display = GetCurrentDisplay();
        for (unsigned int i=0; i<display->rows.size(); ++i) {
            DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(display->rows[i]);
            if (alnRow) rowOrder.push_back(alnRow->row);
        }

        // actually write the alignment
        if (type == asFASTA || type == asFASTAa2m) {
            INFOMSG("exporting" << (type == asFASTAa2m ? " A2M " : " ") << "FASTA to " << outputFile.c_str());
            DumpFASTA((type == asFASTAa2m), alignmentManager->GetCurrentMultipleAlignment(),
                rowOrder, sequenceWindow->GetCurrentJustification(), *ofs);
        } else if (type == asText || type == asHTML) {
            INFOMSG("exporting " << (type == asText ? "text" : "HTML") << " to " << outputFile.c_str());
            DumpText((type == asHTML), alignmentManager->GetCurrentMultipleAlignment(),
                rowOrder, sequenceWindow->GetCurrentJustification(), *ofs);
        } else if (type == asPSSM) {
            static string prevTitle;
            if (prevTitle.size() == 0)
                prevTitle = GetWorkingTitle();
            string title = wxGetTextFromUser(
                "Enter a name for this PSSM (to be used by other applications like PSI-BLAST or RPS-BLAST):",
                "PSSM Title?", prevTitle.c_str(), *viewerWindow).Strip(wxString::both).c_str();
            if (title.size() > 0) {
                INFOMSG("exporting PSSM (" << title << ") to " << outputFile.c_str());
                alignmentManager->GetCurrentMultipleAlignment()->GetPSSM().OutputPSSM(*ofs, title);
                prevTitle = title;
            }
        }
    }
}

END_SCOPE(Cn3D)
