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
 * Author:  Vladimir Ivanov
 *
 */

#include <ncbi_pch.hpp>
#include <util/xregexp/mask_regexp.hpp>


BEGIN_NCBI_SCOPE


bool CMaskRegexp::Match(CTempString str, NStr::ECase use_case) const
{
    CRegexp::TCompile compile_flags = CRegexp::fCompile_default;
    if ( use_case == NStr::eNocase ) {
        compile_flags |= CRegexp::fCompile_ignore_case;
    }
    bool found = m_Inclusions.empty();

    ITERATE(list<string>, it, m_Inclusions) {
        CRegexp re(*it, compile_flags);
        if ( re.IsMatch(str) ) {
            found = true;
            break;
        }                
    }
    if ( found ) {
        ITERATE(list<string>, it, m_Exclusions) {
            CRegexp re(*it, compile_flags);
            if ( re.IsMatch(str) ) {
                found = false;
                break;
            }                
        }
    }
    return found;
}


END_NCBI_SCOPE
