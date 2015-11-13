#ifndef NETSCHEDULE_START_IDS__HPP
#define NETSCHEDULE_START_IDS__HPP

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
 *   NetSchedule queue job start identifiers
 *
 */


#include <corelib/ncbimtx.hpp>
#include <string>
#include <map>


BEGIN_NCBI_SCOPE

using namespace std;


// CNSStartIDs incapsulates loading/providing and serialization of job start
// ids on per queue basis
class CNSStartIDs
{
    typedef map< string, unsigned int, PNocase >    TStartIDs;

    public:
        CNSStartIDs(const string &  data_dir_name);
        ~CNSStartIDs();

        void Set(const string &  qname, unsigned int  value);
        unsigned int Get(const string &  qname);

        void Serialize(void) const;
        void Load(void);

    private:
        void x_SerializeNoLock(void) const;

    private:
        string                          m_FileName;
        mutable CFastMutex              m_Lock;
        TStartIDs                       m_IDs;
};


END_NCBI_SCOPE

#endif /* NETSCHEDULE_START_IDS__HPP */

