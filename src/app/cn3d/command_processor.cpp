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
*       module to process external commands (e.g. from file messenger)
*
*   Commands:
*
*       Highlight:
*           data is any number of the following, one per line separated by '\n':
*               [identifier] \t [ranges]
*           where [identifier] is an XML-format Seq-id
*           where a tab character separates the identifier from the ranges
*           where ranges is sequence of zero-numbered numerical positions or ranges,
*               e.g. "1-100, 102, 105-192"
*
*       LoadFile:
*           data is the name of the file to load
*
*       Exit:
*           quit the program, same behaviour as hitting File:Exit or 'X' icon
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include <list>

#include "command_processor.hpp"
#include "structure_window.hpp"
#include "cn3d_glcanvas.hpp"
#include "structure_set.hpp"
#include "cn3d_tools.hpp"
#include "messenger.hpp"
#include "sequence_set.hpp"
#include "molecule_identifier.hpp"

#include <wx/tokenzr.h>

USING_NCBI_SCOPE;
USING_SCOPE(objects);


BEGIN_SCOPE(Cn3D)

// splits single string into multiple; assumes '\n'-terminated lines
static void SplitString(const string& inStr, list<string> *outList)
{
    outList->clear();

    int firstPos = 0, size = 0;
    for (unsigned int i=0; i<inStr.size(); ++i) {
        if (inStr[i] == '\n') {
            outList->resize(outList->size() + 1);
            outList->back() = inStr.substr(firstPos, size);
            firstPos = i + 1;
            size = 0;
//            TRACEMSG("outList: '" << outList->back() << "'");
        } else if (i == inStr.size() - 1) {
            ERRORMSG("SplitString() - input multi-line string doesn't end with \n!");
        } else {
            ++size;
        }
    }
}

#define SPLIT_DATAIN_INTO_LINES \
    list<string> lines; \
    SplitString(dataIn, &lines); \
    list<string>::const_iterator l, le = lines.end()

void CommandProcessor::ProcessCommand(DECLARE_PARAMS)
{
    TRACEMSG("processing " << command);
    if (dataIn.size() > 0)
        TRACEMSG("data:\n" << dataIn.substr(0, dataIn.size() - 1));
    else
        TRACEMSG("data: (none)");

    // process known commands
    PROCESS_IF_COMMAND_IS(Highlight);
    PROCESS_IF_COMMAND_IS(LoadFile);
    PROCESS_IF_COMMAND_IS(Exit);

    // will only get here if command isn't recognized
    ADD_REPLY_ERROR("Unrecognized command");
}

IMPLEMENT_COMMAND_FUNCTION(Highlight)
{
    if (dataIn.size() == 0) {
        ADD_REPLY_ERROR("no identifiers given");
        return;
    }

    // default response
    *status = MessageResponder::REPLY_OKAY;
    dataOut->erase();

    GlobalMessenger()->RemoveAllHighlights(true);

    SPLIT_DATAIN_INTO_LINES;
    for (l=lines.begin(); l!=le; ++l) {

        vector < string > toks;
        NStr::Tokenize(*l, "\t", toks);
        if (toks.size() != 2) {
            ADD_REPLY_ERROR(string("invalid line, expected 'id' + tab + 'ranges': ") + *l);
            continue;
        }

        // sequence to highlight on
        const Sequence *seq;

        // get identifier
        list < CRef < CSeq_id > > idList;
        idList.resize(1);
        if (!IdentifierToSeqId(toks[0], idList.front())) {
            ADD_REPLY_ERROR(string("unparseable id: ") + toks[0]);
            continue;
        }
        seq = structureWindow->glCanvas->structureSet->sequenceSet->FindMatchingSequence(idList);
        if (!seq) {
            ADD_REPLY_ERROR(string("sequence not found: ") + toks[0]);
            continue;
        }

        // now parse ranges and highlight
        wxStringTokenizer tkz(toks[1].c_str(), ", ", wxTOKEN_STRTOK);
        while (tkz.HasMoreTokens()) {
            wxString range = tkz.GetNextToken();
            wxStringTokenizer rangeToks(range, "-", wxTOKEN_RET_EMPTY);
            if (rangeToks.CountTokens() < 1 || rangeToks.CountTokens() > 2) {
                ADD_REPLY_ERROR(string("bad range: ") + range.c_str());
                continue;
            }
            unsigned long from, to;
            bool okay;
            if (rangeToks.CountTokens() == 1) {
                okay = rangeToks.GetNextToken().ToULong(&from);
                to = from;
            } else {
                okay = (rangeToks.GetNextToken().ToULong(&from) && rangeToks.GetNextToken().ToULong(&to));
            }
            if (!okay || from >= seq->Length() || to >= seq->Length() || from > to) {
                ADD_REPLY_ERROR(string("bad range value(s): ") + range.c_str());
                continue;
            }

            // actually do the highlighting, finally!
            GlobalMessenger()->AddHighlights(seq, from, to);
        }
    }
}

IMPLEMENT_COMMAND_FUNCTION(LoadFile)
{
    if (dataIn.size() == 0) {
        ADD_REPLY_ERROR("no file name given");
        return;
    }

    // default response
    *status = MessageResponder::REPLY_OKAY;
    dataOut->erase();

    // give sequence and structure windows a chance to save data
    if (structureWindow->glCanvas->structureSet) {
        GlobalMessenger()->SequenceWindowsSave(true);
        structureWindow->SaveDialog(true, false);   // can't cancel
    }

    wxString stripped(wxString(dataIn.c_str()).Strip(wxString::both));
    if (!structureWindow->LoadData(stripped.c_str(), true, false))
        ADD_REPLY_ERROR(string("Error loading file '") + stripped.c_str() + "'");
}

IMPLEMENT_COMMAND_FUNCTION(Exit)
{
    // default response
    *status = MessageResponder::REPLY_OKAY;
    dataOut->erase();

    structureWindow->ProcessCommand(StructureWindow::MID_EXIT);
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.14  2005/11/17 22:25:43  thiessen
* remove more spurious uint-compared-to-zero
*
* Revision 1.13  2005/10/26 18:55:30  thiessen
* better handling of -n option
*
* Revision 1.12  2005/10/19 17:28:18  thiessen
* migrate to wxWidgets 2.6.2; handle signed/unsigned issue
*
* Revision 1.11  2004/05/21 21:41:39  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.10  2004/03/15 18:23:01  thiessen
* prefer prefix vs. postfix ++/-- operators
*
* Revision 1.9  2004/02/19 17:04:53  thiessen
* remove cn3d/ from include paths; add pragma to disable annoying msvc warning
*
* Revision 1.8  2004/01/17 01:47:26  thiessen
* add network load
*
* Revision 1.7  2004/01/17 00:17:31  thiessen
* add Biostruc and network structure load
*
* Revision 1.6  2003/09/02 19:34:52  thiessen
* implement Exit message
*
* Revision 1.5  2003/07/10 18:47:29  thiessen
* add CDTree->Select command
*
* Revision 1.4  2003/07/10 13:47:22  thiessen
* add LoadFile command
*
* Revision 1.3  2003/03/20 20:45:45  thiessen
* fix wxString overload ambiguity
*
* Revision 1.2  2003/03/20 20:33:51  thiessen
* implement Highlight command
*
* Revision 1.1  2003/03/14 19:23:06  thiessen
* add CommandProcessor to handle file-message commands; fixes for GCC 2.9
*
*/


