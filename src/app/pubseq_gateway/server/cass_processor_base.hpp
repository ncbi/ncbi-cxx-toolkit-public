#ifndef PSGS_CASSPROCESSORBASE__HPP
#define PSGS_CASSPROCESSORBASE__HPP

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
 * Authors: Sergey Satskiy
 *
 * File Description: base class for processors which may generate cassandra
 *                   fetches
 *
 */

#include <corelib/request_status.hpp>
#include <corelib/ncbidiag.hpp>

#include "cass_fetch.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;


class CPSGS_CassProcessorBase
{
public:
    CPSGS_CassProcessorBase();
    virtual ~CPSGS_CassProcessorBase();

protected:
    bool IsFinished(void) const;
    bool AreAllFinishedRead(void) const;

protected:
    // Cassandra data loaders; there could be many of them
    list<unique_ptr<CCassFetch>>    m_FetchDetails;

    // Sometimes the processor finishes without reaching an async stage e.g.
    // when blob props must be searched only in cache and caches is not hit.
    // m_Completed signals such conditions
    bool                            m_Completed;
};

#endif  // PSGS_CASSPROCESSORBASE__HPP

