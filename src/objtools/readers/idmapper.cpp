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

#include <objtools/readers/message_listener.hpp>
#include <objtools/readers/idmapper.hpp>

#include <objmgr/util/sequence.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


CIdMapper::CIdMapper(const std::string& strContext,
                     bool bInvert,
                     IMessageListener* pErrors)
    : m_strContext(strContext),
      m_bInvert(bInvert),
      m_pErrors( pErrors )
{
}


void CIdMapper::AddMapping(const CSeq_id_Handle& from,
                           const CSeq_id_Handle& to )
{
    CSeq_id_Handle key = from;
    CSeq_id_Handle val = to;
    if (m_bInvert) {
        std::swap(key, val);
    }

    TMapperCache::iterator it =
        m_Cache.insert(TMapperCache::value_type(key, SMapper())).first;
    it->second.dest_idh = to;
    it->second.dest_mapper.Reset();
}


void CIdMapper::AddMapping(const CSeq_loc& loc_from,
                           const CSeq_loc& loc_to)
{
    CConstRef<CSeq_id> id1(loc_from.GetId());
    CConstRef<CSeq_id> id2(loc_to.GetId());
    CSeq_id_Handle idh1;
    CSeq_id_Handle idh2;
    if (id1) {
        idh1 = CSeq_id_Handle::GetHandle(*id1);
    }
    if (id2) {
        idh2 = CSeq_id_Handle::GetHandle(*id2);
    }

    CSeq_id_Handle key = idh1;
    CRef<CSeq_loc_Mapper> mapper;
    if (m_bInvert) {
        key = idh2;
        mapper.Reset(new CSeq_loc_Mapper(loc_to, loc_from));
    } else {
        mapper.Reset(new CSeq_loc_Mapper(loc_from, loc_to));
    }

    TMapperCache::iterator it =
        m_Cache.insert(TMapperCache::value_type(key, SMapper())).first;
    it->second.dest_idh = CSeq_id_Handle();
    it->second.dest_mapper = mapper;
}


CSeq_id_Handle CIdMapper::Map(const CSeq_id_Handle& from )
{
    TMapperCache::iterator it = m_Cache.find(from);
    if (it != m_Cache.end()) {
        if (it->second.dest_idh) {
            return it->second.dest_idh;
        } else if (it->second.dest_mapper) {
            CRef<CSeq_loc> loc_from(new CSeq_loc);
            loc_from->SetWhole().Assign(*from.GetSeqId());
            CRef<CSeq_loc> loc_to = it->second.dest_mapper->Map(*loc_from);
            CConstRef<CSeq_id> id(loc_to->GetId());
            if (id) {
                CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
                return idh;
            }
        }
    }

    //
    //  Cannot map this ID. We will treat this as an error.
    //
    if ( m_pErrors ) {
        AutoPtr<CObjReaderLineException> pMapError( 
            CObjReaderLineException::Create( 
            eDiag_Error, 0, MapErrorString( from ) ) );
        if ( !m_pErrors->PutError( *pMapError ) ) {
            pMapError->Throw();
        }
    }
    return from;
};


CRef<CSeq_loc> CIdMapper::Map(const CSeq_loc& from )
{
    CConstRef<CSeq_id> id(from.GetId());
    CSeq_id_Handle idh = CSeq_id_Handle::GetHandle(*id);
    TMapperCache::iterator it = m_Cache.find(idh);
    if (it != m_Cache.end()) {
        CRef<CSeq_loc> loc_to;
        if (it->second.dest_idh) {
            loc_to.Reset(new CSeq_loc);
            loc_to->Assign(from);
            loc_to->SetId(*it->second.dest_idh.GetSeqId());
        } else if (it->second.dest_mapper) {
            loc_to = it->second.dest_mapper->Map(from);
            if (loc_to->IsNull()) {
                loc_to.Reset();
            }
        }
        if (loc_to) {
            return loc_to;
        }
    }

    //
    //  Cannot map this ID. We will treat this as an error.
    //
    //
    //  Cannot map this ID. We will treat this as an error.
    //
    if ( m_pErrors ) {
        AutoPtr<CObjReaderLineException> pMapError( 
            CObjReaderLineException::Create(
            eDiag_Error, 0, MapErrorString( from ) ) );
        if ( !m_pErrors->PutError( *pMapError ) ) {
            pMapError->Throw();
        }
    }

    CRef<CSeq_loc> loc(new CSeq_loc);
    loc->Assign(from);
    return loc;
};



//  ============================================================================
string CIdMapper::MapErrorString(const CSeq_id_Handle& idh )
{
    string strId = idh.AsString();
    string strMsg(
        string("IdMapper: Unable to resolve ID \"") + strId + string("\"") );
    return strMsg;
};


string CIdMapper::MapErrorString(const CSeq_loc& loc )
{
    string strId;
    loc.GetLabel(&strId);
    string strMsg(
        string("IdMapper: Unable to resolve ID \"") + strId + string("\"") );
    return strMsg;
};


void CIdMapper::MapObject(CSerialObject& object)
{
    set< CRef<CSeq_id> > ids;
    CTypeIterator< CSeq_id > idit( object );
    for ( ;  idit;  ++idit ) {
        CSeq_id& id = *idit;
        if (ids.insert(CRef<CSeq_id>(&id)).second == false) {
            continue;
        }
        CSeq_id_Handle idh = Map( CSeq_id_Handle::GetHandle(id) );
        if ( !idh ) {
            continue;
        }
        id.Assign(*idh.GetSeqId());
    }
};


const char* CIdMapperException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eBadSeqId:      return "eBadSeqId";
    case eOther:         return "eOther";
    default:             return CException::GetErrCodeString();
    }
}


END_NCBI_SCOPE

