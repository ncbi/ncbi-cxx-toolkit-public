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
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

// Objects includes
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objtools/readers/ucscid.hpp>
#include <objtools/readers/idmapper.hpp>
#include "idmap.hpp"
#include "idmapper_builtin.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
CIdMapperBuiltin::CIdMapperBuiltin()
//  ============================================================================
{
    UcscID key( UcscID::Label( "hg18", "chr1" ) );
    CRef<CSeq_id> value( new CSeq_id( "gi|89161185" ) );
    m_Map.AddMapping( UcscID::Label( "hg18", "chr1" ), value );
};

//  ============================================================================
void
CIdMapperBuiltin::Setup(
    const CArgs& args )
//  ============================================================================
{
};

//  ============================================================================
CSeq_id_Handle
CIdMapperBuiltin::MapID(
    const string& key,
    unsigned int& uLength )
//  ============================================================================
{
    CSeq_id_Handle idh = m_Map.GetMapping( key, uLength );
    if ( idh ) {
        return idh;
    }
    return CIdMapper::MapID( key, uLength );
};

//  ============================================================================
void
CIdMapperBuiltin::Dump(
    CNcbiOstream& out,
    const string& strPrefix )
//  ============================================================================
{
    out << strPrefix << "[CIdMapperBuiltIn:" << endl;
    m_Map.Dump( out, strPrefix + "\t" );
    out << strPrefix << "]" << endl;
};

END_NCBI_SCOPE

