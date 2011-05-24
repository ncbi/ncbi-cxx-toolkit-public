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

#include <set>
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

    // When there's a problem, this should return eResult_disallowed and return
    // the severity of the issue via out_sev.
    virtual EResult IsFeatureNameOkay( 
        const string &feature_name, 
        EDiagSev *out_sev = NULL, 
        std::string *out_err_msg = NULL ) = 0;
};

//  ============================================================================
class CSimpleTableFilter
    : public ITableFilter
//  ============================================================================
{
public:
    CSimpleTableFilter( EDiagSev sevIfFeatureNameNotAllowed,
        const string &err_prefix, const string &err_suffix) :
      m_SevIfFeatureNameNotAllowed(sevIfFeatureNameNotAllowed),
          m_ErrPrefix(err_prefix), m_ErrSuffix(err_suffix)
      { }

    void AddDisallowedFeatureName( const string &feature_name )
    {
        m_DisallowedFeatureNames.insert( feature_name );
    }

    EResult IsFeatureNameOkay( const string &feature_name, EDiagSev *out_sev, 
        std::string *out_err_msg )
    {
        if( m_DisallowedFeatureNames.find(feature_name) != 
            m_DisallowedFeatureNames.end() )
        {
            if( NULL != out_sev ) {
                *out_sev = m_SevIfFeatureNameNotAllowed;
            }
            if( NULL != out_err_msg ) {
                *out_err_msg = m_ErrPrefix + feature_name + m_ErrSuffix;
            }
            return eResult_Disallowed;
        } else {
            return eResult_Okay;
        }
    }

private:
    std::set<std::string, PNocase_Conditional> m_DisallowedFeatureNames;
    const std::string m_ErrPrefix;
    const std::string m_ErrSuffix;
    const EDiagSev m_SevIfFeatureNameNotAllowed;
};
             
END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___TABLEFILTER__HPP
