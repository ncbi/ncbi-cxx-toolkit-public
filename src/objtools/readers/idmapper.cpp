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
#include <serial/iterator.hpp>

// Objects includes
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqset/Seq_entry.hpp>

#include <objtools/readers/reader_exception.hpp>
#include <objtools/readers/line_error.hpp>
#include <objtools/readers/error_container.hpp>
#include <objtools/readers/idmapper.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

//  ============================================================================
bool
s_HandlesAreEqual(
    CSeq_id_Handle handle1,
    CSeq_id_Handle handle2 )
//  ============================================================================
{
    if ( handle1.operator==( handle2 ) ) {
        return true;
    }
    
    CConstRef<CSeq_id> id1 = handle1.GetSeqId();
    CConstRef<CSeq_id> id2 = handle2.GetSeqId();
    if ( (id1->Which() != CSeq_id::e_Local) || (id2->Which() != CSeq_id::e_Local) ) {
        return false;
    }
    
    string str1 = id1->GetLocal().GetStr();
    NStr::ToLower( str1 );
    string str2 = id2->GetLocal().GetStr();
    NStr::ToLower( str2 );
    
    if ( str1 == str2 ) {
        return true;
    }
    if ( string( "chr" ) + str1 == str2 ) {
        return true;
    }
    if ( string( "chr" ) + str2 == str1 ) {
        return true;
    }
    return false;
};


//  ============================================================================
CSeq_id_Handle
CIdMapper::Map(
    const CSeq_id_Handle& from )
//  ============================================================================
{
    for ( CACHE::iterator it = m_Cache.begin(); it != m_Cache.end(); ++it ) {
        if ( s_HandlesAreEqual( from, it->first ) ) {
            return it->second;
        }
    }

    //
    //  Cannot map this ID. We will treat this as an error.
    //
    CObjReaderLineException MapError( eDiag_Error, 0, 
        MapErrorString( from ) );

    if ( !m_pErrors || !m_pErrors->PutError( MapError ) ) {
        throw MapError;
    }
    return CSeq_id_Handle();  
};

/*
//  ============================================================================
CRef<CSeq_loc>
CIdMapper::MapLocation(
    const CSeq_id_Handle& idhFrom,
    const CSeq_loc& locFrom )
//  ============================================================================
{
    CRef<CObjectManager> pOm = CObjectManager::GetInstance();

    CScope scope(*pOm);
    scope.AddDefaults();

    CBioseq_Handle hbsFrom = scope.GetBioseqHandle( idhFrom );

    CRef<CSeq_loc_Mapper> mapper;
    mapper.Reset( new CSeq_loc_Mapper( hbsFrom, CSeq_loc_Mapper::eSeqMap_Down ) );
    return mapper->Map( locFrom );
};
*/

//  ============================================================================
string
CIdMapper::MapErrorString(
    const CSeq_id_Handle& idh )
//  ============================================================================
{
    string strId = idh.AsString();
    string strMsg( 
        string("IdMapper: Unable to resolve local ID \"") + strId + string("\"") );
    return strMsg;
};

//  ============================================================================
void
CIdMapper::MapObject(
    CSerialObject& object )
//  ============================================================================
{
    CTypeIterator< CSeq_id > idit( object );
    for ( /*0*/; idit; ++idit ) {
        CSeq_id& id = *idit;
        if ( id.Which() != CSeq_id::e_Local ) {
            continue;
        }
        const CSeq_id::TLocal& locid = id.GetLocal();
        if ( ! locid.IsStr() ) {
            continue;
        }
        CSeq_id_Handle hMappedHandle = Map( CSeq_id_Handle::GetHandle(id) );
        if ( !hMappedHandle ) {
            continue;
        }
        id.Set( hMappedHandle.Which(), 
            hMappedHandle.GetSeqId()->GetSeqIdString() );
    }
};

END_NCBI_SCOPE

