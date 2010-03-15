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
#include <objtools/readers/gff3_sofa.hpp>

BEGIN_NCBI_SCOPE

BEGIN_objects_SCOPE // namespace ncbi::objects::

//  --------------------------------------------------------------------------
TLookupSofaToGenbank CGff3SofaTypes::m_MapSofaTermToGenbankType;
//  --------------------------------------------------------------------------

//  --------------------------------------------------------------------------
CGff3SofaTypes& SofaTypes()
//  --------------------------------------------------------------------------
{
    static CGff3SofaTypes sofaTypes;    
    return sofaTypes;
}

//  --------------------------------------------------------------------------
CGff3SofaTypes::CGff3SofaTypes()
//  --------------------------------------------------------------------------
{
    m_MapSofaTermToGenbankType[ "CDS" ] = CSeqFeatData::eSubtype_cdregion;
    m_MapSofaTermToGenbankType[ "exon" ] = CSeqFeatData::eSubtype_exon;
    m_MapSofaTermToGenbankType[ "gene" ] = CSeqFeatData::eSubtype_gene;
    m_MapSofaTermToGenbankType[ "mRNA" ] = CSeqFeatData::eSubtype_mRNA;
};

//  --------------------------------------------------------------------------
CGff3SofaTypes::~CGff3SofaTypes()
//  --------------------------------------------------------------------------
{
};

//  --------------------------------------------------------------------------
CSeqFeatData::ESubtype CGff3SofaTypes::MapSofaTermToGenbankType(
    const string& strSofa )
//  --------------------------------------------------------------------------
{
    TLookupSofaToGenbankCit cit = m_MapSofaTermToGenbankType.find( strSofa );
    if ( cit == m_MapSofaTermToGenbankType.end() ) {
        return CSeqFeatData::eSubtype_misc_feature;
    }
    return cit->second;
}

END_objects_SCOPE
END_NCBI_SCOPE
