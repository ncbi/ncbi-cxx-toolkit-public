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
 *   GFF file reader
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqfeat/Cdregion.hpp>

#include "gff3_data.hpp"

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ----------------------------------------------------------------------------
CGff3Record::CGff3Record():
    m_uSeqStart( 0 ),
    m_uSeqStop( 0 ),
    m_pdScore( 0 ),
    m_peStrand( 0 ),
    m_pePhase( 0 )
//  ----------------------------------------------------------------------------
{};

//  ----------------------------------------------------------------------------
CGff3Record::~CGff3Record()
//  ----------------------------------------------------------------------------
{
    delete m_pdScore;
    delete m_peStrand;
    delete m_pePhase; 
};

//  ----------------------------------------------------------------------------
bool CGff3Record::Parse(
    const string& strRawInput )
//  ----------------------------------------------------------------------------
{
    vector< string > columns;
    NStr::Tokenize( strRawInput, " \t", columns, NStr::eMergeDelims );

    //  to do: sanity checks

    m_strId = columns[0];
    m_strSource = columns[1];
    m_strType = columns[2];
    m_uSeqStart = NStr::StringToUInt( columns[3] ) - 1;
    m_uSeqStop = NStr::StringToUInt( columns[4] ) - 1;

    if ( columns[5] != "." ) {
        m_pdScore = new double( NStr::StringToDouble( columns[5] ) );
    }

    if ( columns[6] == "+" ) {
        m_peStrand = new ENa_strand( eNa_strand_plus );
    }
    if ( columns[6] == "-" ) {
        m_peStrand = new ENa_strand( eNa_strand_minus );
    }
    if ( columns[6] == "?" ) {
        m_peStrand = new ENa_strand( eNa_strand_unknown );
    }

    if ( columns[7] == "0" ) {
        m_pePhase = new TFrame( CCdregion::eFrame_one );
    }
    if ( columns[7] == "1" ) {
        m_pePhase = new TFrame( CCdregion::eFrame_two );
    }
    if ( columns[7] == "2" ) {
        m_pePhase = new TFrame( CCdregion::eFrame_three );
    }

    m_strAttributes = columns[8];
    
    return x_ParseAttributes( m_strAttributes );
}

//  ----------------------------------------------------------------------------
bool CGff3Record::GetAttribute(
    const string& strKey,
    string& strValue ) const
//  ----------------------------------------------------------------------------
{
    AttrCit it = m_Attributes.find( strKey );
    if ( it == m_Attributes.end() ) {
        return false;
    }
    strValue = it->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CGff3Record::x_ParseAttributes(
    const string& strRawAttributes )
//  ----------------------------------------------------------------------------
{
    vector< string > attributes;
    NStr::Tokenize( strRawAttributes, ";", attributes, NStr::eMergeDelims );
    for ( size_t u=0; u < attributes.size(); ++u ) {
        string strKey;
        string strValue;
        if ( ! NStr::SplitInTwo( attributes[u], "=", strKey, strValue ) ) {
            return false;
        }
        if ( NStr::StartsWith( strValue, "\"" ) ) {
            strValue = strValue.substr( 1, string::npos );
        }
        if ( NStr::EndsWith( strValue, "\"" ) ) {
            strValue = strValue.substr( 0, strValue.length() - 1 );
        }
        m_Attributes[ strKey ] = strValue;
    }
    return true;
}

END_objects_SCOPE
END_NCBI_SCOPE
