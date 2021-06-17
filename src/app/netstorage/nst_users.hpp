#ifndef NETSTORAGE_USERS__HPP
#define NETSTORAGE_USERS__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   NetStorage users facilities
 *
 */

#include <string>
#include <corelib/ncbimtx.hpp>

BEGIN_NCBI_SCOPE

// Shared with the database wrapper code
const Int8 k_UndefinedUserID = -1;


class CNSTUserID
{
    public:
        CNSTUserID()
        {}

        CNSTUserID(const string &  name_space_,
                   const string &  name_) :
            name_space(name_space_), name(name_)
        {}

        void Validate(void);

        bool operator<(const CNSTUserID &  other) const
        { if (name != other.name)
            return name < other.name;
          return name_space < other.name_space;
        }

    public:
        string  GetNamespace(void) const
        { return name_space; }
        string  GetName(void) const
        { return name; }
        void  SetNamespace(const string &  name_space_)
        { name_space = name_space_; }
        void  SetName(const string &  name_)
        { name = name_; }

    private:
        string      name_space;
        string      name;
};


class CNSTUserData
{
    public:
        CNSTUserData() :
            m_DBUserID(k_UndefinedUserID)
        {}

    public:
        Int8  GetDBUserID(void) const
        { return m_DBUserID; }
        void  SetDBUserID(Int8  id)
        { m_DBUserID = id; }

    private:
        Int8        m_DBUserID;
};



class CNSTUserCache
{
    public:
        CNSTUserCache();
        Int8  GetDBUserID(const CNSTUserID &  user) const;
        void  SetDBUserID(const CNSTUserID &  user, Int8  id);
        size_t  Size(void) const;

    private:
        typedef map< CNSTUserID, CNSTUserData >   TUsers;

        TUsers          m_Users;    // All the server users
                                    // netstorage knows about
        mutable CMutex  m_Lock;     // Lock for the map
};


END_NCBI_SCOPE

#endif /* NETSTORAGE_USERS__HPP */

