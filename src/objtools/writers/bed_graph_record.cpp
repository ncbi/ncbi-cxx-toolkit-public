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
 * Authors:  Frank Ludwig
 *
 * File Description:  Write bed file
 *
 */

#include <ncbi_pch.hpp>

#include <objects/seq/Seq_annot.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqloc/Packed_seqint.hpp>

#include <objmgr/mapped_feat.hpp>

#include <objtools/writers/bed_graph_record.hpp>
#include <objtools/writers/write_util.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ----------------------------------------------------------------------------
CBedGraphRecord::CBedGraphRecord():
//  ----------------------------------------------------------------------------
    m_chromId( "." ),
    m_chromStart( "." ),
    m_chromEnd( "." ),
    m_chromValue( "." )
{
}

//  ----------------------------------------------------------------------------
CBedGraphRecord::~CBedGraphRecord()
//  ----------------------------------------------------------------------------
{
}

//  ----------------------------------------------------------------------------
bool CBedGraphRecord::Write(
    CNcbiOstream& ostr)
//  ----------------------------------------------------------------------------
{
    ostr << m_chromId;
    ostr << "\t" << m_chromStart;
    ostr << "\t" << m_chromEnd;
    ostr << "\t" << m_chromValue;
    ostr << "\n";
    return true;
}

//  -----------------------------------------------------------------------------
void CBedGraphRecord::SetChromId(
    const string& chromId)
//  -----------------------------------------------------------------------------
{
    m_chromId = chromId;
}

//  -----------------------------------------------------------------------------
void CBedGraphRecord::SetChromStart(
    size_t chromStart)
//  -----------------------------------------------------------------------------
{
    m_chromStart = NStr::NumericToString(chromStart);
}

//  -----------------------------------------------------------------------------
void CBedGraphRecord::SetChromEnd(
    size_t chromEnd)
//  -----------------------------------------------------------------------------
{
    m_chromEnd = NStr::NumericToString(chromEnd);
}

//  -----------------------------------------------------------------------------
void CBedGraphRecord::SetChromValue(
    double value)
//  -----------------------------------------------------------------------------
{
    m_chromValue = NStr::NumericToString(value);
}

END_NCBI_SCOPE
