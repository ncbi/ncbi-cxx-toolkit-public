#ifndef OBJTOOLS_EDIT__PUBLICATION_EDIT__HPP
#define OBJTOOLS_EDIT__PUBLICATION_EDIT__HPP
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
*  and reliability of the software and data,  the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties,  express or implied,  including
*  warranties of performance,  merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
* Authors:  Igor Filippov, Andrea Asztalos
*
* File Description:
*   Functions that provides fixes for author names  
*/

#include <corelib/ncbistd.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
    class CName_std;
BEGIN_SCOPE(edit)

// returns one or more (skip_rest = true) initials from the input string
NCBI_XOBJEDIT_EXPORT string GetFirstInitial(string input, bool skip_rest);

// generates the contents of the 'initials' member based on the first name and 
// existing value
NCBI_XOBJEDIT_EXPORT bool GenerateInitials(CName_std& name);

// fixes the 'initials' member only if it has been already set
NCBI_XOBJEDIT_EXPORT bool FixInitials(CName_std& name);

// moves the middle name (initials member) to the first name
NCBI_XOBJEDIT_EXPORT bool MoveMiddleToFirst(CName_std& name);

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif    
    // OBJTOOLS_EDIT__PUBLICATION_EDIT__HPP