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

#include "remove_header_conflicts.hpp"

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
