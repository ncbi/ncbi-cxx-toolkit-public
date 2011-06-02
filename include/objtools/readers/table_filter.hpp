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
 * Author: Michael Kornbluh
 *
 * File Description:
 *   Allows users to optionally filter when reading a table file.  For example,
 *   a user might specify that the "source" feature should cause a warning.
 */

#ifndef OBJTOOLS_READERS___TABLEFILTER__HPP
#define OBJTOOLS_READERS___TABLEFILTER__HPP

#include <map>
#include <string>

#include <corelib/ncbidiag.hpp>
#include <corelib/ncbistr.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class ITableFilter
//  ============================================================================
{
public:
    virtual ~ITableFilter() {}

    enum EResult {
        eResult_Okay = 1,
        eResult_AllowedButWarn,
        eResult_Disallowed
    };

    // Returns how we should treat the given feature name
    virtual EResult IsFeatureNameOkay( 
        const string &feature_name ) = 0;
};

//  ============================================================================
class CSimpleTableFilter
    : public ITableFilter
//  ============================================================================
{
public:
    void AddDisallowedFeatureName( const string &feature_name, EResult result )
    {
        // set how to handle the given feature_name
        m_DisallowedFeatureNames[feature_name] = result;
    }

    EResult IsFeatureNameOkay( const string &feature_name )
    {
        TDisallowedMap::const_iterator find_feature_result = 
            m_DisallowedFeatureNames.find(feature_name);

        if( find_feature_result != m_DisallowedFeatureNames.end() )
        {
            return find_feature_result->second;
        } else {
            return eResult_Okay;
        }
    }

private:
    typedef std::map<std::string, EResult, PNocase_Conditional> TDisallowedMap;
    // maps feature names to how they should be handled
    TDisallowedMap m_DisallowedFeatureNames;
};
             
END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___TABLEFILTER__HPP
