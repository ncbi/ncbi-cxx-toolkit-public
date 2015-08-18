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
 *   NetSchedule database dumping support.
 *
 */

#include <ncbi_pch.hpp>

#include <stdlib.h>
#include <string.h>
#include "ns_db_dump.hpp"
#include "ns_types.hpp"


BEGIN_NCBI_SCOPE


SJobDump::SJobDump()
{
    memset(this, 0, sizeof(SJobDump));
    magic = kDumpMagic;
}

void SJobDump::Write(FILE *  f, const char *  progress_msg)
{
    errno = 0;
    if (fwrite(this, sizeof(SJobDump), 1, f) != 1)
        throw runtime_error(strerror(errno));

    if (progress_msg_size > 0) {
        errno = 0;
        if (fwrite(progress_msg, progress_msg_size, 1, f) != 1)
            throw runtime_error(strerror(errno));
    }
}

int SJobDump::Read(FILE *  f, char *  progress_msg)
{
    errno = 0;
    size_t      bytes = fread(this, 1, sizeof(SJobDump), f);
    if (bytes != sizeof(SJobDump)) {
        if (bytes > 0)
            throw runtime_error("Incomplete job record reading");
        if (feof(f))
            return 1;
        if (errno != 0)
            throw runtime_error(strerror(errno));
        throw runtime_error("Unknown job record reading error");
    }

    if (magic != kDumpMagic)
        throw runtime_error("Job structure magic does not match.");

    if (client_ip_size > kMaxClientIpSize)
        throw runtime_error("Job client ip size is more "
                            "than max allowed");
    if (client_sid_size > kMaxSessionIdSize)
        throw runtime_error("Job client session id size is more "
                            "than max allowed");
    if (ncbi_phid_size > kMaxHitIdSize)
        throw runtime_error("Job page hit id size is more "
                            "than max allowed");
    if (progress_msg_size > kNetScheduleMaxDBDataSize)
        throw runtime_error("Job progress message size is more "
                            "than max allowed");
    if (progress_msg_size > 0) {
        errno = 0;
        bytes = fread(progress_msg, 1, progress_msg_size, f);
        if (bytes != progress_msg_size) {
            if (bytes > 0)
                throw runtime_error("Incomplete job progress message reading");
            if (feof(f))
                throw runtime_error("Unexpected end of job info file. "
                                    "Expected a progress message, got EOF");
            if (errno != 0)
                throw runtime_error(strerror(errno));
            throw runtime_error("Unknown job progress message reading error");
        }
    }
    return 0;
}


// It is supposed that the input and/or output follow each instance of the
// structure below. The limit of the input/output is too large to have
// it as a member of this structure
void SJobIODump::Write(FILE *  f, const char *  input,
                                  const char *  output)
{
    errno = 0;
    if (fwrite(this, sizeof(SJobIODump), 1, f) != 1)
        throw runtime_error(strerror(errno));

    if (input_size > 0) {
        errno = 0;
        if (fwrite(input, input_size, 1, f) != 1)
            throw runtime_error(strerror(errno));
    }

    if (output_size > 0) {
        errno = 0;
        if (fwrite(output, output_size, 1, f) != 1)
            throw runtime_error(strerror(errno));
    }
}


int SJobIODump::Read(FILE *  f, char *  input,
                                char *  output)
{
    errno = 0;
    size_t  bytes = fread(this, 1, sizeof(SJobIODump), f);
    if (bytes != sizeof(SJobIODump)) {
        if (bytes > 0)
            throw runtime_error("Incomplete job i/o record reading");
        if (feof(f))
            throw runtime_error("Unexpected end of job info file. "
                                "Expected a job i/o, got EOF");
        if (errno != 0)
            throw runtime_error(strerror(errno));
        throw runtime_error("Unknown job i/o reading error");
    }

    if (input_size > kNetScheduleMaxOverflowSize)
        throw runtime_error("Job input size is more "
                            "than max allowed");
    if (input_size > 0) {
        errno = 0;
        bytes = fread(input, 1, input_size, f);
        if (bytes != input_size) {
            if (bytes > 0)
                throw runtime_error("Incomplete job input reading");
            if (feof(f))
                throw runtime_error("Unexpected end of job info file. "
                                    "Expected an input, got EOF");
            if (errno != 0)
                throw runtime_error(strerror(errno));
            throw runtime_error("Unknown job input reading error");
        }
    }

    if (output_size > kNetScheduleMaxOverflowSize)
        throw runtime_error("Job output size is more "
                            "than max allowed");
    if (output_size > 0) {
        errno = 0;
        bytes = fread(output, 1, output_size, f);
        if (bytes != output_size) {
            if (bytes > 0)
                throw runtime_error("Incomplete job output reading");
            if (feof(f))
                throw runtime_error("Unexpected end of job info file. "
                                    "Expected an output, got EOF");
            if (errno != 0)
                throw runtime_error(strerror(errno));
            throw runtime_error("Unknown job output reading error");
        }
    }
    return 0;
}


SJobEventsDump::SJobEventsDump()
{
    memset(this, 0, sizeof(SJobEventsDump));
}

void SJobEventsDump::Write(FILE *  f, const char *  client_node,
                                      const char *  client_session,
                                      const char *  err_msg)
{
    errno = 0;
    if (fwrite(this, sizeof(SJobEventsDump), 1, f) != 1)
        throw runtime_error(strerror(errno));

    if (client_node_size > 0) {
        errno = 0;
        if (fwrite(client_node, client_node_size, 1, f) != 1)
            throw runtime_error(strerror(errno));
    }
    if (client_session_size > 0) {
        errno = 0;
        if (fwrite(client_session, client_session_size, 1, f) != 1)
            throw runtime_error(strerror(errno));
    }
    if (err_msg_size > 0) {
        errno = 0;
        if (fwrite(err_msg, err_msg_size, 1, f) != 1)
            throw runtime_error(strerror(errno));
    }
}

int SJobEventsDump::Read(FILE *  f, char *  client_node,
                                    char *  client_session,
                                    char *  err_msg)
{
    errno = 0;
    size_t      bytes = fread(this, 1, sizeof(SJobEventsDump), f);
    if (bytes != sizeof(SJobEventsDump)) {
        if (bytes > 0)
            throw runtime_error("Incomplete job event reading");
        if (feof(f))
            throw runtime_error("Unexpected end of job info file. "
                                "Expected a job event, got EOF");
        if (errno != 0)
            throw runtime_error(strerror(errno));
        throw runtime_error("Unknown job event reading error");
    }

    if (client_node_size > kMaxWorkerNodeIdSize)
        throw runtime_error("Job event client node size is more "
                            "than max allowed");
    if (client_node_size > 0) {
        errno = 0;
        bytes = fread(client_node, 1, client_node_size, f);
        if (bytes != client_node_size) {
            if (bytes > 0)
                throw runtime_error("Incomplete job event client node reading");
            if (feof(f))
                throw runtime_error("Unexpected end of job info file. "
                                    "Expected a client node, got EOF");
            if (errno != 0)
                throw runtime_error(strerror(errno));
            throw runtime_error("Unknown job event client node reading error");
        }
    }

    if (client_session_size > kMaxWorkerNodeIdSize)
        throw runtime_error("Job event client session size is more "
                            "than max allowed");
    if (client_session_size > 0) {
        errno = 0;
        bytes = fread(client_session, 1, client_session_size, f);
        if (bytes != client_session_size) {
            if (bytes > 0)
                throw runtime_error("Incomplete job event "
                                    "client session reading");
            if (feof(f))
                throw runtime_error("Unexpected end of job info file. "
                                    "Expected a client session, got EOF");
            if (errno != 0)
                throw runtime_error(strerror(errno));
            throw runtime_error("Unknown job event "
                                "client session reading error");
        }
    }

    if (err_msg_size > kNetScheduleMaxDBErrSize)
        throw runtime_error("Job event error message size is more "
                            "than max allowed");
    if (err_msg_size > 0) {
        errno = 0;
        bytes = fread(err_msg, 1, err_msg_size, f);
        if (bytes != err_msg_size) {
            if (bytes > 0)
                throw runtime_error("Incomplete job event "
                                    "error message reading");
            if (feof(f))
                throw runtime_error("Unexpected end of job info file. "
                                    "Expected an error message, got EOF");
            if (errno != 0)
                throw runtime_error(strerror(errno));
            throw runtime_error("Unknown job event "
                                "error message reading error");
        }
    }

    return 0;
}


SAffinityDictDump::SAffinityDictDump()
{
    memset(this, 0, sizeof(SAffinityDictDump));
    magic = kDumpMagic;
}


void SAffinityDictDump::Write(FILE *  f)
{
    errno = 0;
    if (fwrite(this, sizeof(SAffinityDictDump), 1, f) != 1)
        throw runtime_error(strerror(errno));
}

int SAffinityDictDump::Read(FILE *  f)
{
    errno = 0;
    size_t      bytes = fread(this, 1, sizeof(SAffinityDictDump), f);
    if (bytes != sizeof(SAffinityDictDump)) {
        if (bytes > 0)
            throw runtime_error("Incomplete affinity reading");
        if (feof(f))
            return 1;
        if (errno != 0)
            throw runtime_error(strerror(errno));
        throw runtime_error("Unknown affinity reading error");
    }

    if (magic != kDumpMagic)
        throw runtime_error("Affinity structure magic does not match.");
    if (token_size > kNetScheduleMaxDBDataSize)
        throw runtime_error("Affinity token size is more "
                            "than max allowed");
    return 0;
}


SGroupDictDump::SGroupDictDump()
{
    memset(this, 0, sizeof(SGroupDictDump));
    magic = kDumpMagic;
}


void SGroupDictDump::Write(FILE *  f)
{
    errno = 0;
    if (fwrite(this, sizeof(SGroupDictDump), 1, f) != 1)
        throw runtime_error(strerror(errno));
}

int SGroupDictDump::Read(FILE *  f)
{
    errno = 0;
    size_t      bytes = fread(this, 1, sizeof(SGroupDictDump), f);
    if (bytes != sizeof(SGroupDictDump)) {
        if (bytes > 0)
            throw runtime_error("Incomplete job group reading");
        if (feof(f))
            return 1;
        if (errno != 0)
            throw runtime_error(strerror(errno));
        throw runtime_error("Unknown job group reading error");
    }

    if (magic != kDumpMagic)
        throw runtime_error("Group structure magic does not match.");
    if (token_size > kNetScheduleMaxDBDataSize)
        throw runtime_error("Group token size is more "
                            "than max allowed");
    return 0;
}


SQueueDescriptionDump::SQueueDescriptionDump()
{
    memset(this, 0, sizeof(SQueueDescriptionDump));
    magic = kDumpMagic;
}


void SQueueDescriptionDump::Write(FILE *  f)
{
    errno = 0;
    if (fwrite(this, sizeof(SQueueDescriptionDump), 1, f) != 1)
        throw runtime_error(strerror(errno));
}

int SQueueDescriptionDump::Read(FILE *  f)
{
    errno = 0;
    size_t      bytes = fread(this, 1, sizeof(SQueueDescriptionDump), f);
    if (bytes != sizeof(SQueueDescriptionDump)) {
        if (bytes > 0)
            throw runtime_error("Incomplete queue description reading");
        if (feof(f))
            return 1;
        if (errno != 0)
            throw runtime_error(strerror(errno));
        throw runtime_error("Unknown queue description reading error");
    }

    if (magic != kDumpMagic)
        throw runtime_error("Queue description structure magic "
                            "does not match.");
    if (qname_size > kMaxQueueNameSize)
        throw runtime_error("Queue name size is more "
                            "than max allowed");
    if (qclass_size > kMaxQueueNameSize)
        throw runtime_error("Queue class name size is more "
                            "than max allowed");
    if (program_name_size > kMaxQueueLimitsSize)
        throw runtime_error("Program name size is more "
                            "than max allowed");
    if (subm_hosts_size > kMaxQueueLimitsSize)
        throw runtime_error("Submitter hosts size is more "
                            "than max allowed");
    if (wnode_hosts_size > kMaxQueueLimitsSize)
        throw runtime_error("Worker node hosts size is more "
                            "than max allowed");
    if (reader_hosts_size > kMaxQueueLimitsSize)
        throw runtime_error("Reader hosts size is more "
                            "than max allowed");
    if (description_size > kMaxDescriptionSize)
        throw runtime_error("Description size is more "
                            "than max allowed");
    if (linked_section_prefixes_size > kLinkedSectionsList)
        throw runtime_error("Linked section prefixes size is more "
                            "than max allowed");
    if (linked_section_names_size > kLinkedSectionsList)
        throw runtime_error("Linked section names size is more "
                            "than max allowed");
    return 0;
}


SLinkedSectionDump::SLinkedSectionDump()
{
    memset(this, 0, sizeof(SLinkedSectionDump));
    magic = kDumpMagic;
}


void SLinkedSectionDump::Write(FILE *  f)
{
    if (fwrite(this, sizeof(SLinkedSectionDump), 1, f) != 1)
        throw runtime_error(strerror(errno));
}

int SLinkedSectionDump::Read(FILE *  f)
{
    errno = 0;
    size_t      bytes = fread(this, 1, sizeof(SLinkedSectionDump), f);
    if (bytes != sizeof(SLinkedSectionDump)) {
        if (bytes > 0)
            throw runtime_error("Incomplete linked section reading");
        if (feof(f))
            return 1;
        if (errno != 0)
            throw runtime_error(strerror(errno));
        throw runtime_error("Unknown linked section reading error");
    }

    if (magic != kDumpMagic)
        throw runtime_error("Linked section structure magic does not match");
    if (section_size > kLinkedSectionValueNameSize)
        throw runtime_error("Linked section name size is more "
                            "than max allowed");
    if (value_name_size > kLinkedSectionValueNameSize)
        throw runtime_error("Linked section value name size is more "
                            "than max allowed");
    if (value_size > kLinkedSectionValueSize)
        throw runtime_error("Linked section value size is more "
                            "than max allowed");
    return 0;
}


END_NCBI_SCOPE

