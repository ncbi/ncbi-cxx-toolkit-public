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
#include <sra/readers/sra/sdk.hpp>

// SRA SDK structures
struct KTable;
struct KMetadata;
struct KMDataNode;
struct KNamelist;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_entry;

DECLARE_SRA_REF_TRAITS(KTable, const);
DECLARE_SRA_REF_TRAITS(KMetadata, const);
DECLARE_SRA_REF_TRAITS(KMDataNode, const);
DECLARE_SRA_REF_TRAITS(KNamelist, );


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
    explicit CKMetadata(const CKTable& table);
    CKMetadata(const CVDB& db, const char* table_name);

protected:
    void x_Init(const CKTable& table);
};


class NCBI_SRAREAD_EXPORT CKMDataNode
    : public CSraRef<const KMDataNode>
{
public:
    enum EMissingAction {
        eMissingThrow,
        eMissingIgnore
    };
    CKMDataNode(const CKMetadata& meta,
                const char* node_name,
                EMissingAction missing_action = eMissingThrow);
    CKMDataNode(const CKMDataNode& parent,
                const char* node_name,
                EMissingAction missing_action = eMissingThrow);

    // 'buffer' must be at least 'size' big, may be null if 'size' is 0
    // 'size' is the requested number of bytes
    // 'offset' is offset of returned data in the node
    // returns pair of (actually read bytes, remaining bytes)
    pair<size_t, size_t>
    TryToGetData(char* buffer, size_t size, size_t offset = 0) const;
    // the same as TryToGetData, but verifies that all requested data is read
    void GetData(char* buffer, size_t size, size_t offset = 0) const;

    size_t GetSize(void) const {
        return TryToGetData(0, 0, 0).second;
    }

    Uint8 GetUint8(void) const;
};


class NCBI_SRAREAD_EXPORT CKNameList
    : public CSraRef<KNamelist>
{
public:
    explicit CKNameList(const CKMDataNode& parent);

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
