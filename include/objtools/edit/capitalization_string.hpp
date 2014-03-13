#ifndef _CAPITALIZATION_INSTRINGS_HPP_
#define _CAPITALIZATION_INSTRINGS_HPP_
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
 * Authors:  Andrea Asztalos
 */

#include <corelib/ncbistd.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/bioseq_handle.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

enum ECapChange {
    eCapChange_none = 0,                /// no change
    eCapChange_tolower,                 /// change each letter to lower case
    eCapChange_toupper,                 /// change each letter to upper case
    eCapChange_firstcap_restlower,      /// capitalize the first letter, the rest is lower case
    eCapChange_firstcap_restnochange,   /// capitalize the first letter, the rest is not changed
    eCapChange_firstlower_restnochange, /// first letter is lower case, the rest is not changed
    eCapChange_capword_afterspace,      /// capitalize the first letter and letters after spaces
    eCapChange_capword_afterspacepunc   /// capitalize first letter and letters after spaces or punctuations
};

/// Capitalize the string according to desired capitalization option
NCBI_XOBJEDIT_EXPORT void FixCapitalizationInString (objects::CSeq_entry_Handle seh, string& str, ECapChange capchange_opt);
NCBI_XOBJEDIT_EXPORT void FixAbbreviationsInElement(string& result);
NCBI_XOBJEDIT_EXPORT void FixOrgNames(objects::CSeq_entry_Handle seh, string& result);
NCBI_XOBJEDIT_EXPORT void FindOrgNames(objects::CSeq_entry_Handle seh, vector<string>& taxnames);

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE

#endif   // _CAPITALIZATION_INSTRINGS_HPP_

