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
 *   Government have not placed any restriction on its use or reproduction.
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
 * Authors:  Dmitry Kazimirov
 *
 * File Description: Automation processor - Worker node declarations.
 *
 */

#ifndef WN_AUTOMATION__HPP
#define WN_AUTOMATION__HPP

#include "automation.hpp"

BEGIN_NCBI_SCOPE

struct SWorkerNodeAutomationObject : public SNetServiceAutomationObject
{
    SWorkerNodeAutomationObject(CAutomationProc* automation_proc,
            const string& wn_address, const string& client_name);

    virtual const string& GetType() const;

    virtual const void* GetImplPtr() const;

    virtual bool Call(const string& method,
            CArgArray& arg_array, CJsonNode& reply);

    SWnCompatibleNsAPI m_NetScheduleAPI;
    CNetServer m_WorkerNode;
};

END_NCBI_SCOPE

#endif // WN_AUTOMATION__HPP
