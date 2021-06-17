#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__MESSAGES_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__MESSAGES_HPP

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
 * Authors: Dmitrii Saprykin
 *
 * File Description:
 *
 *  PSG storage message texts
 *
 */

#include <corelib/ncbistd.hpp>

#include <string>
#include <map>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CPSGMessages
{
    using TCollection = map<string, string>;
 public:
    CPSGMessages() = default;
    CPSGMessages(CPSGMessages const&) = default;
    CPSGMessages(CPSGMessages &&) = default;
    CPSGMessages& operator=(CPSGMessages const&) = default;
    CPSGMessages& operator=(CPSGMessages &&) = default;

    CPSGMessages& Set(string name, string value)
    {
        m_Messages[name] = value;
        return *this;
    }

    string Get(string const& name) const
    {
        auto itr = m_Messages.find(name);
        if (itr != m_Messages.cend()) {
            return itr->second;
        }
        return "";
    }

 private:
    TCollection m_Messages;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__MESSAGES_HPP

