#ifndef CONN___NETSCHEDULE_KEY__HPP
#define CONN___NETSCHEDULE_KEY__HPP

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
 * Authors: Maxim Didenko
 *
 * File Description:
 *   NetSchedule client API.
 *
 */


#include <connect/connect_export.h>
#include <connect/ncbi_connutil.h>
#include <corelib/ncbistl.hpp>

#include <util/bitset/ncbi_bitset.hpp>
#include <util/bitset/bmserial.h>
#include <util/bitset/bmfwd.h>

#include <string>
#include <map>
#include <vector>

/// @file netschedule_client.hpp
/// NetSchedule client specs. 
///



BEGIN_NCBI_SCOPE


/** @addtogroup NetScheduleClient
 *
 * @{
 */

/// Meaningful information encoded in the NetSchedule key
///
struct NCBI_XCONNECT_EXPORT CNetScheduleKey
{
    CNetScheduleKey(unsigned _id, const string& _host, unsigned _port, unsigned ver = 1)
        : id(_id), host(_host), port(_port), version(ver) {}

    explicit CNetScheduleKey(const string& str_key);

    operator string() const;

    unsigned     id;        ///< Job id
    string       host;      ///< server name
    unsigned     port;      ///< TCP/IP port number
    unsigned     version;   ///< Key version
};

const string kNetScheduleKeyPrefix = "JSID";


/// @internal
#define NETSCHEDULE_JOBMASK "JSID_01_%u_%s_%u"


/////////////////////////////////////////////////////////////////////////////////////
////
class CNetScheduleAdmin;

template <typename TBVector = bm::bvector<> >
class CNetScheduleKeys
{
public:
    typedef map<pair<string,unsigned int>, TBVector> TKeysMap;

    CNetScheduleKeys() {}

    class const_iterator 
    {
    public:
        typedef typename CNetScheduleKeys<TBVector>::TKeysMap TKeysMap;
        typedef typename TBVector::enumerator TBVEnumerator;
        
        const_iterator(const TKeysMap& keys, bool end) : m_Keys(&keys) 
        {
            if (end) 
                m_SrvIter = m_Keys->end();
            else {
                for (m_SrvIter = m_Keys->begin(); m_SrvIter != m_Keys->end(); ++m_SrvIter) {
                    m_BVEnum = m_SrvIter->second.first();
                    if (m_BVEnum != m_SrvIter->second.end())
                        break;
                }
                if (m_SrvIter == m_Keys->end())
                    m_BVEnum = TBVEnumerator();
            }
        }
        bool operator == (const const_iterator& other) const
        {
            return m_Keys == other.m_Keys && m_SrvIter == other.m_SrvIter 
                && m_BVEnum == other.m_BVEnum;
        }
        bool operator != (const const_iterator& other) const
        {
            return ! operator==(other);
        }

        CNetScheduleKey operator*() const
        {
            return CNetScheduleKey((unsigned)*m_BVEnum, m_SrvIter->first.first,
                                   m_SrvIter->first.second);
        }

        const_iterator& operator++()
        {
            if (m_SrvIter != m_Keys->end()) {
                if(++m_BVEnum == m_SrvIter->second.end()) {
                    if (++m_SrvIter == m_Keys->end())
                        m_BVEnum = TBVEnumerator();
                    else
                        m_BVEnum = m_SrvIter->second.first();
                }
            }
            return *this;
        }
        const_iterator operator++(int)
        {
            const_iterator tmp(*this);
            ++*this;
            return tmp;
        }
    private:
        const TKeysMap* m_Keys;
        typename TKeysMap::const_iterator m_SrvIter;
        TBVEnumerator m_BVEnum;
    };

    const_iterator begin() const { return const_iterator(m_Keys, false); }
    const_iterator end() const { return const_iterator(m_Keys, true); }

private:
    friend class CNetScheduleAdmin;

    TKeysMap m_Keys;

    void x_Clear()
    {
        m_Keys.clear();
    }
    void x_Add(const pair<string,unsigned int>& host, const string& base64_enc_bv)
    {
        size_t src_read, dst_written;
        size_t dst_size =  base64_enc_bv.size();
        vector<unsigned char> dst_buf(dst_size, 0);
        BASE64_Decode(base64_enc_bv.data(),  base64_enc_bv.size(), &src_read,
                      &dst_buf[0], dst_size, &dst_written);
        TBVector& bv = m_Keys[host];
        bv.clear();
        bm::deserialize(bv, &dst_buf[0]);
    }

    CNetScheduleKeys(const CNetScheduleKeys&);
    CNetScheduleKeys& operator=(const CNetScheduleKeys&);
};


/* @} */


END_NCBI_SCOPE


#endif  /* CONN___NETSCHEDULE_KEY__HPP */
