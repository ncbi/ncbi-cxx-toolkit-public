#ifndef ACC_VER_CACHE_STORAGE__HPP
#define ACC_VER_CACHE_STORAGE__HPP

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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 */

#include <string>

#include <util/lmdbxx/lmdb++.h>

#include <objtools/pubseq_gateway/rpc/UtilException.hpp>

class CAccVerCacheStorage {
private:
    const lmdb::env& m_Env;
    lmdb::dbi m_Dbi;
    bool m_IsReadOnly;
public:
    CAccVerCacheStorage(const lmdb::env& Env);
    static void InitializeDb(const lmdb::env& Env);

    bool Update(const std::string& Key, const std::string& Data, bool CheckIfExists);
    bool Get(const std::string& Key, std::string& Data) const;

    class iterator: public std::iterator<std::forward_iterator_tag, std::pair<std::string, std::string>> {
        const lmdb::dbi& m_Dbi;
        lmdb::txn m_Txn;
        lmdb::cursor m_Cursor;
        bool m_BOF;
        bool m_EOF;
        std::pair<std::string, std::string> m_Current;
    public:
        iterator(const lmdb::env& Env, const lmdb::dbi& Dbi) :
            m_Dbi(Dbi),
            m_Txn(0),
            m_Cursor(0),
            m_BOF(true),
            m_EOF(false)
        {
            m_Txn = lmdb::txn::begin(Env, nullptr, MDB_RDONLY);
            m_Cursor = lmdb::cursor::open(m_Txn, Dbi);
            operator++();
        }
        iterator(const lmdb::dbi& Dbi) :
            m_Dbi(Dbi),
            m_Txn(0),
            m_Cursor(0),
            m_BOF(false),
            m_EOF(true)
        {}
        iterator() = delete;
        iterator(iterator&&) = default;
        iterator(const iterator& from) = delete;
        ~iterator() {
            if (m_Cursor.handle()) {
                m_Cursor.close();
            }
            if (m_Txn.handle()) {
                m_Txn.commit();
            }
        };
        iterator& operator= (const iterator&) = delete;
        iterator& operator= (iterator&&) = default;
        iterator& operator++() {
            if (!m_Txn.handle())
                EAccVerException::raise("Sentinel iterator can't move forward");
            m_BOF = false;
            if (m_EOF)
                EAccVerException::raise("EOF already reached");
            if (!m_Cursor.get(m_Current.first, m_Current.second, MDB_NEXT))
                m_EOF = true;
            return *this;
        }
        iterator operator++(int) {
            EAccVerException::raise("Post-increment is banned");
        }
        bool operator==(const iterator& rhs) const {
            return (m_Dbi == rhs.m_Dbi) && (m_EOF == rhs.m_EOF) && (m_BOF == rhs.m_BOF) && (m_EOF || m_BOF || m_Current.first == rhs.m_Current.first);
        }
        bool operator!=(const iterator& rhs) const {
            return ! operator==(rhs);
        }
        const std::pair<std::string, std::string>& operator*() const {
            return m_Current;
        }
        const std::pair<std::string, std::string>* operator->() const {
            return &m_Current;
        }
    };

    iterator begin() const {
        iterator tmp(m_Env, m_Dbi);
        return tmp;
    }

    iterator end() const {
        iterator tmp(m_Dbi);
        return tmp;
    }
    static constexpr const char* const DATA_DB        = "@DB";
};

#endif
