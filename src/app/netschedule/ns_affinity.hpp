#ifndef NETSCHEDULE_BDB_AFF__HPP
#define NETSCHEDULE_BDB_AFF__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Net schedule job status database.
 *
 */

/// @file bdb_affinity.hpp
/// NetSchedule job affinity.
///
/// @internal

#include <corelib/ncbimtx.hpp>
#include <corelib/ncbicntr.hpp>

#include "ns_types.hpp"
#include "worker_node.hpp"

#include <db/bdb/bdb_file.hpp>
#include <db/bdb/bdb_env.hpp>
#include <db/bdb/bdb_cursor.hpp>

#include <map>
#include <vector>


BEGIN_NCBI_SCOPE

struct SAffinityDictDB;
struct SAffinityDictTokenIdx;
class CAffinityDictGuard;

/// Affinity dictionary database class
///
/// @internal
class CAffinityDict
{
public:
    CAffinityDict();
    ~CAffinityDict();

    /// Open the association files
//    void Open(CBDB_Env& env, const string& queue_name);

//    void Close();

    void Attach(SAffinityDictDB* aff_dict_db,
                SAffinityDictTokenIdx* aff_dict_token_idx);
    void Detach();

    /// Get affinity token id
    /// Returns 0 if token does not exist
    unsigned GetTokenId(const string& aff_token);

    void GetTokensIds(const list<string>& tokens, TNSBitVector& ids);

    /// Get affinity string by id
    string GetAffToken(unsigned aff_id);

private:
    CAffinityDict(const CAffinityDict& );
    CAffinityDict& operator=(const CAffinityDict& );

private:
    friend class CAffinityDictGuard;

    /// Add a new token or return token's id if this affinity
    /// index is already in the database
    unsigned x_CheckToken(const string&     aff_token,
                          CBDB_Transaction& trans);
    /// Remove affinity token
    void x_RemoveToken(unsigned          aff_id,
                       CBDB_Transaction& trans);

    CAtomicCounter           m_IdCounter;
    CFastMutex               m_DbLock;
    SAffinityDictDB*         m_AffDictDB;
    CBDB_FileCursor*         m_CurAffDB;
    SAffinityDictTokenIdx*   m_AffDict_TokenIdx;
    CBDB_FileCursor*         m_CurTokenIdx;
};


class CAffinityDictGuard
{
public:
    CAffinityDictGuard(CAffinityDict& aff_dict, CBDB_Transaction& trans)
      : m_AffDict(aff_dict), m_DbGuard(aff_dict.m_DbLock), m_Trans(trans)
    {}

    /// Add a new token or return token's id if this affinity
    /// index is already in the database
    unsigned CheckToken(const string& aff_token)
    {
        return m_AffDict.x_CheckToken(aff_token, m_Trans);
    }

    /// Remove affinity token
    void RemoveToken(unsigned aff_id)
    {
        m_AffDict.x_RemoveToken(aff_id, m_Trans);
    }

private:
    CAffinityDict&    m_AffDict;
    CFastMutexGuard   m_DbGuard;
    CBDB_Transaction& m_Trans;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_BDB_AFF__HPP */

