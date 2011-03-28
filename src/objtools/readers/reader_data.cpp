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
 * Author:  Frank Ludwig
 *
 * File Description:
 *   WIGGLE transient data structures
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include "reader_data.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects) // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
bool CBrowserData::IsBrowserData(
    const LineData& linedata )
//  ----------------------------------------------------------------------------
{
    return ( !linedata.empty() && linedata[0] == "browser" );
}

//  ----------------------------------------------------------------------------
bool CBrowserData::ParseLine(
    const LineData& linedata )
//  ----------------------------------------------------------------------------
{
    if ( !IsBrowserData(linedata) ) {
        return false;
    }
    m_Data.clear();

    LineData::const_iterator cit = linedata.begin();
    for ( cit++; cit != linedata.end(); ++cit ) {
        string key, value;
        m_Data[ key ] = value;
    }
    return true;
}

//  ----------------------------------------------------------------------------
const CBrowserData::BrowserData&
CBrowserData::Values() const
//  ----------------------------------------------------------------------------
{
    return m_Data;
}

//  ----------------------------------------------------------------------------
bool CTrackData::IsTrackData(
    const LineData& linedata )
//  ----------------------------------------------------------------------------
{
    return ( !linedata.empty() && linedata[0] == "track" );
}

//  ----------------------------------------------------------------------------
bool CTrackData::ParseLine(
    const LineData& linedata )
//  ----------------------------------------------------------------------------
{
    if ( !IsTrackData(linedata) ) {
        return false;
    }
    m_strType = m_strName = m_strDescription = "";
    m_Data.clear();

    LineData::const_iterator cit = linedata.begin();
    for ( cit++; cit != linedata.end(); ++cit ) {
        string key, value;
        NStr::SplitInTwo( *cit, "=", key, value );
        if ( key == "type" ) {
            m_strType = value;
            continue;
        }
        if ( key == "name" ) {
            m_strName = value;
            continue;
        }
        if ( key == "description" ) {
            m_strDescription = value;
            continue;
        }
        m_Data[ key ] = value;
    }
    return true;
}

//  ----------------------------------------------------------------------------
const CTrackData::TrackData&
CTrackData::Values() const
//  ----------------------------------------------------------------------------
{
    return m_Data;
}

END_SCOPE(objects)
END_NCBI_SCOPE
