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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1999/01/21 16:18:06  sandomir
* minor changes due to NStr namespace to contain string utility functions
*
* Revision 1.1  1999/01/20 17:39:46  vasilche
* Selection as separate class CSelection.
*
* ===========================================================================
*/

#include <html/selection.hpp>
#include <html/html.hpp>
#include <corelib/ncbicgi.hpp>

BEGIN_NCBI_SCOPE

string CSelection::sm_DefaultCheckboxName = "uid";
string CSelection::sm_DefaultSaveName = "other_uids";

CSelection::CSelection(CCgiRequest& request, 
                       const string& checkboxName, const string& saveName)
    : m_SaveName(saveName)
{
    TCgiEntries& values = const_cast<TCgiEntries&>(request.GetEntries()); // SW_01

    // decode saved ids
    TCgiEntriesCI found = values.find(saveName);
    if ( found != values.end() )
        Decode( found->second );

    // load checkboxes
    for ( TCgiEntriesCI i = values.lower_bound(checkboxName),
              end = values.upper_bound(checkboxName); i != end; ++i ) {
        AddID(NStr::StringToInt(i->second));
    }
}

void CSelection::CreateSubNodes(void)
{
    // save other selections
    string selection = Encode();
    if ( !selection.empty() )
        AppendChild(new CHTML_hidden(m_SaveName, selection));
}

END_NCBI_SCOPE
