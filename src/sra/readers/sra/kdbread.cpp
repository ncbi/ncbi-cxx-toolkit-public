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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to SRA files
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/sra/kdbread.hpp>

#include <sra/readers/sra/vdbread.hpp>
#include <vdb/vdb-priv.h>
#include <klib/rc.h>
#include <klib/namelist.h>
#include <kdb/table.h>
#include <kdb/meta.h>
#include <kdb/namelist.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_entry;


DEFINE_SRA_REF_TRAITS(KTable, const);
DEFINE_SRA_REF_TRAITS(KMetadata, const);
DEFINE_SRA_REF_TRAITS(KMDataNode, const);
DEFINE_SRA_REF_TRAITS(KNamelist, );


CKTable::CKTable(const CVDBTable& table)
{
    if ( rc_t rc = VTableOpenKTableRead(table, x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot open KTable", rc);
    }
}


CKMetadata::CKMetadata(const CKTable& table)
{
    x_Init(table);
}


CKMetadata::CKMetadata(const CVDB& db, const char* table_name)
{
    x_Init(CVDBTable(db, table_name));
}


void CKMetadata::x_Init(const CKTable& table)
{
    if ( rc_t rc = KTableOpenMetadataRead(table, x_InitPtr()) ) {
        *x_InitPtr() = 0;
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot open KMetadata", rc);
    }
}


CKMDataNode::CKMDataNode(const CKMetadata& meta,
                         const char* node_name,
                         EMissingAction missing_action)
{
    if ( rc_t rc = KMetadataOpenNodeRead(meta, x_InitPtr(), node_name) ) {
        *x_InitPtr() = 0;
        if ( GetRCObject(rc) == RCObject(rcPath) &&
             GetRCState(rc) == rcNotFound ) {
            if ( missing_action == eMissingThrow ) {
                NCBI_THROW3(CSraException, eNotFound,
                            "Cannot open KMDataNode", rc, node_name);
            }
        }
        else {
            NCBI_THROW3(CSraException, eInitFailed,
                        "Cannot open KMDataNode", rc, node_name);
        }
    }
}


CKMDataNode::CKMDataNode(const CKMDataNode& parent,
                         const char* node_name,
                         EMissingAction missing_action)
{
    if ( rc_t rc = KMDataNodeOpenNodeRead(parent, x_InitPtr(), node_name) ) {
        *x_InitPtr() = 0;
        if ( GetRCObject(rc) == RCObject(rcPath) &&
             GetRCState(rc) == rcNotFound ) {
            if ( missing_action == eMissingThrow ) {
                NCBI_THROW3(CSraException, eNotFound,
                            "Cannot open child KMDataNode", rc, node_name);
            }
        }
        else {
            NCBI_THROW3(CSraException, eInitFailed,
                        "Cannot open child KMDataNode", rc, node_name);
        }
    }
}


Uint8 CKMDataNode::GetUint8(void) const
{
    uint64_t value;
    if ( rc_t rc = KMDataNodeReadAsU64(*this, &value) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot read metadata node value", rc);
    }
    return (Uint8)value;
}


pair<size_t, size_t>
CKMDataNode::TryToGetData(char* buffer, size_t size, size_t offset) const
{
    pair<size_t, size_t> ret;
    if ( rc_t rc = KMDataNodeRead(*this, offset, buffer, size,
                                  &ret.first, &ret.second) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot read metadata node data", rc);
    }
    return ret;
}


void CKMDataNode::GetData(char* buffer, size_t size, size_t offset) const
{
    pair<size_t, size_t> ret = TryToGetData(buffer, size, offset);
    if ( ret.first != size ) {
        NCBI_THROW2(CSraException, eDataError,
                    "Cannot read all requested data from metadata node",
                    RC(rcApp, rcData, rcRetrieving, rcSize, rcTooBig));
    }
}


CKNameList::CKNameList(const CKMDataNode& parent)
{
    if ( rc_t rc = KMDataNodeListChild(parent, x_InitPtr()) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot get metadata node child list", rc);
    }
    if ( rc_t rc = KNamelistCount(*this, &m_Size) ) {
        NCBI_THROW2(CSraException, eInitFailed,
                    "Cannot get size of metadata node child list", rc);
    }
}


const char* CKNameList::operator[](size_t index) const
{
    if ( index >= size() ) {
        NCBI_THROW3(CSraException, eInvalidIndex,
                    "Invalid index for a namelist",
                    RC(rcApp, rcData, rcRetrieving, rcOffset, rcTooBig),
                    index);
    }
    const char* ret = 0;
    if ( rc_t rc = KNamelistGet(*this, uint32_t(index), &ret) ) {
        NCBI_THROW3(CSraException, eInitFailed,
                    "Cannot get name from a namelist", rc, index);
    }
    return ret;
}


END_SCOPE(objects)
END_NCBI_SCOPE
