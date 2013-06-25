#ifndef SRA__READER__SRA__KDBREAD__HPP
#define SRA__READER__SRA__KDBREAD__HPP
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
 *   Access to KDB files
 *
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbiobj.hpp>

#include <sra/readers/sra/sraread.hpp>

#include <kdb/table.h>
#include <kdb/meta.h>
#include <klib/namelist.h>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

SPECIALIZE_SRA_REF_TRAITS(KTable, const);
SPECIALIZE_SRA_REF_TRAITS(KMetadata, const);
SPECIALIZE_SRA_REF_TRAITS(KMDataNode, const);
SPECIALIZE_SRA_REF_TRAITS(KNamelist, );


class CVDB;
class CVDBTable;

class NCBI_SRAREAD_EXPORT CKTable
    : public CSraRef<const KTable>
{
public:
    CKTable(const CVDBTable& table);
};


class NCBI_SRAREAD_EXPORT CKMetadata
    : public CSraRef<const KMetadata>
{
public:
    CKMetadata(const CKTable& table);
    CKMetadata(const CVDBTable& table);
    CKMetadata(const CVDB& db, const char* table_name);

protected:
    void x_Init(const CKTable& table);
};


class NCBI_SRAREAD_EXPORT CKMDataNode
    : public CSraRef<const KMDataNode>
{
public:
    CKMDataNode(const CKMetadata& meta, const char* node_name);
    CKMDataNode(const CKMDataNode& parent, const char* node_name);

    Uint8 GetUint8(void) const;
};


class NCBI_SRAREAD_EXPORT CKNameList
    : public CSraRef<KNamelist>
{
public:
    CKNameList(const CKMDataNode& parent);

    size_t size(void) const
        {
            return m_Size;
        }

    const char* operator[](size_t index) const;

protected:
    uint32_t m_Size;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__SRA__KDBREAD__HPP
