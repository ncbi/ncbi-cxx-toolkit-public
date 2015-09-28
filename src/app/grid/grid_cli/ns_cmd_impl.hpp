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
 * File Description: NetSchedule-specific commands - implementation
 *                   helpers - declarations.
 *
 */

#ifndef NS_CMD_IMPL__HPP
#define NS_CMD_IMPL__HPP

#include "grid_cli.hpp"

BEGIN_NCBI_SCOPE

struct SWnCompatibleNsAPI : public CNetScheduleAPI
{
    SWnCompatibleNsAPI(const string& service, const string& name)
        : CNetScheduleAPI(service, name)
    {}
};

class CPrintJobInfo : public IJobInfoProcessor
{
public:
    virtual void ProcessJobMeta(const CNetScheduleKey& key);

    virtual void BeginJobEvent(const CTempString& event_header);
    virtual void ProcessJobEventField(const CTempString& attr_name,
            const string& attr_value);
    virtual void ProcessJobEventField(const CTempString& attr_name);
    virtual void ProcessInput(const string& data) { PrintXput(data, "input"); }
    virtual void ProcessOutput(const string& data) { PrintXput(data, "output"); }

    virtual void ProcessJobInfoField(const CTempString& field_name,
        const CTempString& field_value);

    virtual void ProcessRawLine(const string& line);

private:
    void PrintXput(const string& data, const char* prefix);
};

END_NCBI_SCOPE

#endif // NS_CMD_IMPL__HPP
