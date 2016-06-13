#ifndef GPIPE_ASNCACHE_EXTRACT_SATKEY_HPP__
#define GPIPE_ASNCACHE_EXTRACT_SATKEY_HPP__
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
 * Authors:  Cheinan Marks
 *
 * File Description:
 * A class that extracts the sat key from the ID dump update directory name.
 * The directory has a format xx.0_0 where xx is a one or two digit integer
 * that corresponds to the sat ID.
 *
 */

#include <utility>
#include <string>

#include <util/xregexp/regexp.hpp>

BEGIN_NCBI_SCOPE

class CExtractUpdateSatKey
{
public:
    CExtractUpdateSatKey() : m_SatkeyRegex( "(\\d+)\\.0_0$" ) {}
    
    std::pair<bool,Uint2>   operator() ( const std::string & dump_subdir_path )
    {
        std::pair<bool, Uint2>  match;
        std::string regex_match = m_SatkeyRegex.GetMatch( dump_subdir_path, 0, 1 );
        match.first = !regex_match.empty();
        if (match.first) {
            match.second = NStr::StringToUInt( regex_match );
        }
        
       return   match;
    }
    
private:
    CRegexp m_SatkeyRegex;
};

END_NCBI_SCOPE

#endif  //  GPIPE_ASNCACHE_EXTRACT_SATKEY_HPP__
