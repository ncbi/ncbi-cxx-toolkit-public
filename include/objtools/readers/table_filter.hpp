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
/// Use to give a feature filter to CFeature_table_reader
class ITableFilter
//  ============================================================================
{
public:
    ITableFilter(void) { }
    virtual ~ITableFilter() {}

    /// How a given feature name should be handled
    enum EAction {
        eAction_Okay = 1, ///< Just accept the feat
        eAction_AllowedButWarn, ///< Accept the feat but give message eProblem_FeatureNameNotAllowed
        eAction_Disallowed, ///< Do not accept the feat and give message eProblem_FeatureNameNotAllowed
    };

    /// Returns how we should treat the given feature name
    virtual EAction GetFeatAction( 
        const string &feature_name ) const = 0;
private:
    /// forbid copy
    ITableFilter(const ITableFilter &);
    /// forbid assignment
    ITableFilter & operator=(const ITableFilter &);
};

//  ============================================================================
/// Example implementation of ITableFilter with the simplest, most common
/// functionality.
class CSimpleTableFilter
    : public ITableFilter
//  ============================================================================
{
public:
    /// @param default_action
    ///     The action to return if no special action has been set for
    ///     a feature.
    explicit CSimpleTableFilter(EAction default_action)
        : m_default_action(default_action) { }

    /// Special EAction for this feat to override the default action.
    /// Calling again will override the previous setting
    void SetActionForFeat(const string &feature_name, EAction action)
    {
        m_FeatActionMap[feature_name] = action;
    }

    /// Indicate how a given feature should be handled.
    EAction GetFeatAction( const string &feature_name ) const
    {
        TFeatActionMap::const_iterator find_feature_action = 
            m_FeatActionMap.find(feature_name);

        if( find_feature_action != m_FeatActionMap.end() )
        {
            return find_feature_action->second;
        } else {
            return m_default_action;
        }
    }

private:
    const EAction m_default_action;

    typedef std::map<std::string, EAction, PNocase_Conditional> TFeatActionMap;
    // maps feature names to how they should be handled
    TFeatActionMap m_FeatActionMap;
};
             
END_objects_SCOPE

END_NCBI_SCOPE

#endif // OBJTOOLS_READERS___TABLEFILTER__HPP
