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

#ifdef _MSC_VER
#pragma warning(disable:4018)   // disable signed/unsigned mismatch warning in MSVC
#endif

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <memory>

#include "sequence_viewer.hpp"
#include "sequence_viewer_window.hpp"
#include "sequence_display.hpp"
#include "messenger.hpp"
#include "alignment_manager.hpp"
#include "structure_set.hpp"
#include "molecule_identifier.hpp"
#include "cn3d_tools.hpp"
#include "sequence_set.hpp"

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
    vector < int > rowOrder;
    const SequenceDisplay *display = GetCurrentDisplay();
    for (int i=0; i<display->rows.size(); ++i) {
        DisplayRowFromAlignment *alnRow = dynamic_cast<DisplayRowFromAlignment*>(display->rows[i]);
        if (alnRow) rowOrder.push_back(alnRow->row);
    }
    alignmentManager->SavePairwiseFromMultiple(GetCurrentAlignments().back(), rowOrder);
}

void SequenceViewer::DisplayAlignment(BlockMultipleAlignment *alignment)
{
    SequenceDisplay *display = new SequenceDisplay(true, viewerWindow);
    for (int row=0; row<alignment->NRows(); ++row)
        display->AddRowFromAlignment(row, alignment);

    // set starting scroll to a few residues left of the first aligned block
    display->SetStartingColumn(alignment->GetFirstAlignedBlockPosition() - 5);

    AlignmentList alignments;
    alignments.push_back(alignment);
    InitData(&alignments, display);
    if (sequenceWindow)
        sequenceWindow->UpdateDisplay(display);
    else
        CreateSequenceWindow(false);

    // allow alignment export
    sequenceWindow->EnableExport(true);
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
    else
        CreateSequenceWindow(false);

    // forbid alignment export
    sequenceWindow->EnableExport(false);
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
    int firstCol = 0, lastCol = alignment->AlignmentWidth() - 1, nColumns = 70;
    if (firstCol < 0 || lastCol >= alignment->AlignmentWidth() || firstCol > lastCol || nColumns < 1) {
        ERRORMSG("DumpFASTA() - nonsensical display region parameters");
        return;
    }

    // first fill out ids
    typedef map < const MoleculeIdentifier * , list < string > > IDMap;
    IDMap idMap;
    int row;
    bool anyRepeat = false;

    for (row=0; row<alignment->NRows(); ++row) {
        const Sequence *seq = alignment->GetSequenceOfRow(row);
        list < string >& titleList = idMap[seq->identifier];
        CNcbiOstrstream oss;
        oss << '>';

        if (titleList.size() == 0) {

            // create full title line for first instance of this sequence
            bool prevID = false;
            if (seq->identifier->gi != MoleculeIdentifier::VALUE_NOT_SET) {
                oss << "gi|" << seq->identifier->gi;
                prevID = true;
            }
            if (seq->identifier->pdbID.size() > 0) {
                if (prevID) oss << '|';
                if (seq->identifier->pdbID == "query" || seq->identifier->pdbID == "consensus") {
                    oss << "lcl|" << seq->identifier->pdbID;
                } else {
                    oss << "pdb|" << seq->identifier->pdbID;
                    if (seq->identifier->pdbChain != ' ')
                        oss << '|' << (char) seq->identifier->pdbChain << " Chain "
                           << (char) seq->identifier->pdbChain << ',';
                }
                prevID = true;
            }
            if (seq->identifier->accession.size() > 0) {
                if (prevID) oss << '|';
                oss << seq->identifier->accession;
                prevID = true;
            }
            if (seq->description.size() > 0)
                oss << ' ' << seq->description;

        } else {
            // add repeat id
            oss << "lcl|instance #" << (titleList.size() + 1) << " of " << seq->identifier->ToString();
            anyRepeat = true;
        }

        titleList.resize(titleList.size() + 1);
        oss << '\n' << '\0';
        auto_ptr<char> data(oss.str());
        titleList.back() = data.get();
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
    int paragraphStart, nParags = 0, i;
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
                        os << (isA2M ? ch : (char) toupper(ch));
                } else
                    ERRORMSG("GetCharacterTraitsAt failed!");
            }
            os << '\n';
        }
    }
}

static void DumpText(bool doHTML, const BlockMultipleAlignment *alignment,
    const vector < int >& rowOrder,
    BlockMultipleAlignment::eUnalignedJustification justification, CNcbiOstream& os)
{

#define LEFT_JUSTIFY resetiosflags(IOS_BASE::right) << setiosflags(IOS_BASE::left)
#define RIGHT_JUSTIFY resetiosflags(IOS_BASE::left) << setiosflags(IOS_BASE::right)

    // do whole alignment for now
    int firstCol = 0, lastCol = alignment->AlignmentWidth() - 1, nColumns = 60;

    if (firstCol < 0 || lastCol >= alignment->AlignmentWidth() || firstCol > lastCol || nColumns < 1) {
        ERRORMSG("DumpText() - nonsensical display region parameters");
        return;
    }

    // HTML colors
    static const string
        bgColor("#FFFFFF"), rulerColor("#700777"), numColor("#229922");

    // set up the titles and uids, figure out how much space any seqLoc string will take
    vector < string > titles(alignment->NRows()), uids(doHTML ? alignment->NRows() : 0);
    int alnRow, row, maxTitleLength = 0, maxSeqLocStrLength = 0, leftMargin, decimalLength;
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
                CNcbiOstrstream uidoss;
                uidoss << sequence->identifier->gi << '\0';
                uids[row] = uidoss.str();
                delete uidoss.str();
            } else if (sequence->identifier->pdbID.size() > 0) {
                if (sequence->identifier->pdbID != "query" &&
                    sequence->identifier->pdbID != "consensus") {
                    uids[row] = sequence->identifier->pdbID;
                    if (sequence->identifier->pdbChain != ' ')
                        uids[row] += (char) sequence->identifier->pdbChain;
                }
            } else if (sequence->identifier->accession.size() > 0) {
                uids[row] = sequence->identifier->accession;
            }
        }
    }
    leftMargin = maxTitleLength + maxSeqLocStrLength + 2;

    // need to keep track of first, last seqLocs for each row in each paragraph;
    // find seqLoc of first residue >= firstCol
    vector < int > lastShownSeqLocs(alignment->NRows());
    int alnLoc, i;
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
        int nMarkers = 0, width;
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
                os << "<a href=\"http://www.ncbi.nlm.nih.gov/entrez/query.fcgi"
                    << "?cmd=Search&doptcmdl=GenPept&db=Protein&term="
                    << uids[row] << "\" onMouseOut=\"window.status=''\"\n"
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
            if (lastShownSeqLocs[row] !=
                    alignment->GetSequenceOfRow(row)->Length()-1) {
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

    wxString outputFolder = wxString(GetUserDir().c_str(), GetUserDir().size() - 1); // remove trailing /
    wxString baseName, outputFile;
    wxSplitPath(GetWorkingFilename().c_str(), NULL, &baseName, NULL);
    wxFileDialog dialog(sequenceWindow, "Choose a file for alignment export:", outputFolder,
#ifdef __WXGTK__
        baseName + extension,
#else
        baseName,
#endif
        wildcard, wxSAVE | wxOVERWRITE_PROMPT);
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
        for (int i=0; i<display->rows.size(); ++i) {
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
        }
    }
}

END_SCOPE(Cn3D)


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.67  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.66  2004/03/15 18:32:03  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.65  2004/02/19 17:05:07  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.64  2003/11/26 20:37:54  thiessen
* prefer gi for URLs
*
* Revision 1.63  2003/11/20 22:08:50  thiessen
* update Entrez url
*
* Revision 1.62  2003/10/13 14:16:31  thiessen
* add -n option to not show alignment window
*
* Revision 1.61  2003/02/05 14:55:22  thiessen
* always load single structure even if structureLimit == 0
*
* Revision 1.60  2003/02/03 19:20:05  thiessen
* format changes: move CVS Log to bottom of file, remove std:: from .cpp files, and use new diagnostic macros
*
* Revision 1.59  2002/12/09 16:25:04  thiessen
* allow negative score threshholds
*
* Revision 1.58  2002/12/06 17:07:15  thiessen
* remove seqrow export format; add choice of repeat handling for FASTA export; export rows in display order
*
* Revision 1.57  2002/12/02 13:37:08  thiessen
* add seqrow format export
*
* Revision 1.56  2002/10/13 22:58:08  thiessen
* add redo ability to editor
*
* Revision 1.55  2002/09/09 13:38:23  thiessen
* separate save and save-as
*
* Revision 1.54  2002/09/03 13:15:58  thiessen
* add A2M export
*
* Revision 1.53  2002/08/15 22:13:16  thiessen
* update for wx2.3.2+ only; add structure pick dialog; fix MultitextDialog bug
*
* Revision 1.52  2002/06/06 01:30:02  thiessen
* fixes for linux/gcc
*
* Revision 1.51  2002/06/05 14:28:40  thiessen
* reorganize handling of window titles
*
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
*/
