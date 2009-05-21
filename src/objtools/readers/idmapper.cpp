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
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqfeat/Seq_feat.hpp>

#include <objtools/readers/ucscid.hpp>
#include <objtools/readers/idmapper.hpp>
#include "idmap.hpp"
#include "idmapper_builtin.hpp"
#include "idmapper_user.hpp"
#include "idmapper_site.hpp"
#include "idmapper_database.hpp"

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
CIdMapper*
CIdMapper::GetIdMapper(
    const string& strType )
//  ============================================================================
{
    if ( strType == "builtin" ) {
        return new CIdMapperBuiltin;
    }
    if ( strType == "user" ) {
        return new CIdMapperUser;
    }
    if ( strType == "site" ) {
        return new CIdMapperSite;
    }
    if ( strType == "database" ) {
        return new CIdMapperDatabase;
    }
    return new CIdMapper;
}

//  ============================================================================
CIdMapper*
CIdMapper::GetIdMapper(
    const CArgs& args )
//  ============================================================================
{
    CIdMapper* pIdMapper = GetIdMapper( args[ "t" ].AsString() );
    pIdMapper->Setup( args );
    return pIdMapper;
};

//  ============================================================================
void CIdMapper::Setup(
    const CArgs& args )
//  ============================================================================
{
};

//  ============================================================================
CSeq_id_Handle
CIdMapper::MapID(
    const string& strKey,
    unsigned int& uLength )
//  ============================================================================
{
    uLength = 0;    
    CSeq_id id( CSeq_id::e_Local, strKey );
    return CSeq_id_Handle::GetHandle( id );
};

//  ============================================================================
void
CIdMapper::MapLocation(
    const string& strBuild,
    CSeq_loc& location )
//  ============================================================================
{
    const CSeq_id* pId = location.GetId();
    if ( ! pId || pId->Which() != CSeq_id::e_Local ) {
        return;
    }
    const CSeq_id::TLocal& locid = pId->GetLocal();
    if ( ! locid.IsStr() ) {
        return;
    }
    string strChrom = locid.GetStr();
    
    unsigned int iDummy( 0 );
    CSeq_id_Handle mapped_handle = MapID( 
        UcscID::Label( strBuild, strChrom ), iDummy );            
    location.SetId( *(mapped_handle.GetSeqId()) );
};

//  ============================================================================
void
CIdMapper::MapSeqAnnot(
    const string& strBuild,
    CRef<CSeq_annot>& annot )
//  ============================================================================
{
    if ( ! annot->CanGetData() ) {
        return;
    }
    CSeq_annot::TData& data = annot->SetData();
    switch( data.Which() ) {
    
    case CSeq_annot::C_Data::e_Graph: {
            list< CRef< CSeq_graph > >& graphs = data.SetGraph();
            NON_CONST_ITERATE( list< CRef< CSeq_graph > >, it, graphs ) {
                MapLocation( strBuild, (*it)->SetLoc() );
            }
        }
        break;
    
    case CSeq_annot::C_Data::e_Ftable: {
            list< CRef< CSeq_feat > >& features = data.SetFtable();
            NON_CONST_ITERATE( list< CRef< CSeq_feat > >, it, features ) {
                MapLocation( strBuild, (*it)->SetLocation() );
            }
        }
        break;
            
    default: 
        break;
    }
};

//  ============================================================================
void
CIdMapper::Dump(
    CNcbiOstream& out,
    const string& strPrefix )
//  ============================================================================
{
    out << strPrefix << "[CIdMapper:" << endl;
    out << strPrefix << "]" << endl;
};

END_NCBI_SCOPE

