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

#include "json_over_uttp.hpp"
#include "grid_cli.hpp"

#include <connect/services/netschedule_api.hpp>

BEGIN_NCBI_SCOPE

#define TEMP_STRING_CTOR(str) CTempString(str, sizeof(str) - 1)

CJsonNode GenericStatToJson(CNetServer server,
        ENetScheduleStatTopic topic, bool verbose);

CJsonNode LegacyStatToJson(CNetServer server, bool verbose);

class CAttrListParser
{
public:
    enum ENextAttributeType {
        eAttributeWithValue,
        eStandAloneAttribute,
        eNoMoreAttributes
    };

    void Reset(const char* position, const char* eol)
    {
        m_Position = position;
        m_EOL = eol;
    }

    void Reset(const CTempString& line)
    {
        const char* line_buf = line.data();
        Reset(line_buf, line_buf + line.size());
    }

    ENextAttributeType NextAttribute(CTempString& attr_name,
        string& attr_value);

private:
    const char* m_Position;
    const char* m_EOL;
};

class IJobInfoProcessor
{
public:
    virtual void ProcessJobMeta(const CNetScheduleKey& key) = 0;

    virtual void BeginJobEvent(const CTempString& event_header) = 0;
    virtual void ProcessJobEventField(const CTempString& attr_name,
            const string& attr_value) = 0;
    virtual void ProcessJobEventField(const CTempString& attr_name) = 0;

    virtual void ProcessInputOutput(const string& data,
            const CTempString& input_or_output) = 0;

    virtual void ProcessJobInfoField(const CTempString& field_name,
            const CTempString& field_value) = 0;

    virtual void ProcessRawLine(const string& line) = 0;

    virtual ~IJobInfoProcessor() {}
};

class CPrintJobInfo : public IJobInfoProcessor
{
public:
    virtual void ProcessJobMeta(const CNetScheduleKey& key);

    virtual void BeginJobEvent(const CTempString& event_header);
    virtual void ProcessJobEventField(const CTempString& attr_name,
            const string& attr_value);
    virtual void ProcessJobEventField(const CTempString& attr_name);

    virtual void ProcessInputOutput(const string& data,
            const CTempString& input_or_output);

    virtual void ProcessJobInfoField(const CTempString& field_name,
        const CTempString& field_value);

    virtual void ProcessRawLine(const string& line);
};

class CJobInfoToJSON : public IJobInfoProcessor
{
public:
    CJobInfoToJSON() : m_JobInfo(CJsonNode::NewObjectNode()) {}

    virtual void ProcessJobMeta(const CNetScheduleKey& key);

    virtual void BeginJobEvent(const CTempString& event_header);
    virtual void ProcessJobEventField(const CTempString& attr_name,
            const string& attr_value);
    virtual void ProcessJobEventField(const CTempString& attr_name);

    virtual void ProcessInputOutput(const string& data,
            const CTempString& input_or_output);

    virtual void ProcessJobInfoField(const CTempString& field_name,
        const CTempString& field_value);

    virtual void ProcessRawLine(const string& line);

    CJsonNode GetRootNode() const {return m_JobInfo;}

private:
    CJsonNode m_JobInfo;
    CJsonNode m_JobEvents;
    CJsonNode m_CurrentEvent;
    CJsonNode m_UnparsableLines;
};

void ProcessJobInfo(CNetScheduleAPI ns_api, const string& job_key,
        IJobInfoProcessor* processor, bool verbose);

END_NCBI_SCOPE

#endif // NS_CMD_IMPL__HPP
