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
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>

#include "cn3d/command_processor.hpp"
#include "cn3d/structure_window.hpp"
#include "cn3d/cn3d_glcanvas.hpp"
#include "cn3d/structure_set.hpp"
#include "cn3d/cn3d_tools.hpp"
#include "cn3d/messenger.hpp"

USING_NCBI_SCOPE;


BEGIN_SCOPE(Cn3D)

void CommandProcessor::ProcessCommand(DECLARE_PARAMS)
{
    TRACEMSG("processing " << command << ", data:\n" << dataIn);

    // process known commands
    PROCESS_IF_COMMAND_IS(Highlight);

    // will only get here if command isn't recognized
    *status = MessageResponder::REPLY_ERROR;
    *dataOut = "Unrecognized command";
}

IMPLEMENT_COMMAND_FUNCTION(Highlight)
{
    *status = MessageResponder::REPLY_OKAY;
    *dataOut = "did Highlight";
}

END_SCOPE(Cn3D)

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2003/03/14 19:23:06  thiessen
* add CommandProcessor to handle file-message commands; fixes for GCC 2.9
*
*/


