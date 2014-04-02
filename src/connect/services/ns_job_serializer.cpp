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
 * File Description: NetSchedule job serialization - implementation.
 *
 */

#include <ncbi_pch.hpp>

#include <connect/services/ns_job_serializer.hpp>

#include <connect/services/ns_output_parser.hpp>
#include <connect/services/grid_rw_impl.hpp>

#include <corelib/rwstream.hpp>


BEGIN_NCBI_SCOPE

string CNetScheduleJobSerializer::SaveJobInput(const string& target_dir,
        CNetCacheAPI& nc_api)
{
    string target_file = CDirEntry::ConcatPath(target_dir,
            m_Job.job_id + ".in");

    CNcbiOfstream output_stream(target_file, CNcbiOfstream::binary);

    bool need_space = false;

    if (!m_Job.affinity.empty()) {
        output_stream << "affinity=\"" <<
                NStr::PrintableString(m_Job.affinity) << '"';
        need_space = true;
    }

    if (!m_Job.group.empty()) {
        if (need_space)
            output_stream << ' ';
        output_stream << "group=\"" <<
                NStr::PrintableString(m_Job.group) << '"';
        need_space = true;
    }

    if ((m_Job.mask & CNetScheduleAPI::eExclusiveJob) != 0) {
        if (need_space)
            output_stream << ' ';
        output_stream << "exclusive";
    }

    output_stream << NcbiEndl;

    CStringOrBlobStorageReader job_input_reader(m_Job.input, nc_api);
    CRStream job_input_istream(&job_input_reader);
    NcbiStreamCopy(output_stream, job_input_istream);

    return target_file;
}

void CNetScheduleJobSerializer::LoadJobInput(const string& source_file)
{
    CNcbiIfstream input_stream(source_file, CNcbiIfstream::binary);

    if (input_stream.fail() && !input_stream.eof()) {
        NCBI_THROW_FMT(CIOException, eRead,
            "Error while reading job input from '" << source_file << '\'');
    }

    string header;
    getline(input_stream, header);

    CAttrListParser attr_parser;
    attr_parser.Reset(header);

    CAttrListParser::ENextAttributeType attr_type;

    CTempString attr_name;
    string attr_value;
    size_t attr_column;

#define ATTR_POS " at column " << attr_column

    while ((attr_type = attr_parser.NextAttribute(&attr_name,
            &attr_value, &attr_column)) != CAttrListParser::eNoMoreAttributes) {
        if (attr_name == "affinity")
            m_Job.affinity = attr_value;
        else if (attr_name == "group")
            m_Job.group = attr_value;
        else if (attr_name == "exclusive") {
            m_Job.mask = CNetScheduleAPI::eExclusiveJob;
            continue;
        } else {
            NCBI_THROW_FMT(CArgException, eInvalidArg,
                "unknown attribute '" << attr_name << "'" ATTR_POS);
        }

        if (attr_type != CAttrListParser::eAttributeWithValue) {
            NCBI_THROW_FMT(CArgException, eInvalidArg,
                "attribute '" << attr_name << "' requires a value" ATTR_POS);
        }
    }

    if (!input_stream.eof()) {
        CStringOrBlobStorageWriter job_input_writer(
                numeric_limits<size_t>().max(), NULL, m_Job.input);
        CWStream job_input_ostream(&job_input_writer, 0, NULL);
        NcbiStreamCopy(job_input_ostream, input_stream);
    }

    CDirEntry file_name(source_file);
    m_Job.job_id = file_name.GetBase();
}

string CNetScheduleJobSerializer::SaveJobOutput(
        CNetScheduleAPI::EJobStatus job_status,
        const string& target_dir,
        CNetCacheAPI& nc_api)
{
    string target_file = CDirEntry::ConcatPath(target_dir,
            m_Job.job_id + ".out");

    CNcbiOfstream output_stream(target_file, CNcbiOfstream::binary);

    output_stream <<
            "job_status=" << CNetScheduleAPI::StatusToString(job_status) <<
            " ret_code=" << m_Job.ret_code;

    if (!m_Job.error_msg.empty()) {
        output_stream << " error_msg=\"" <<
                NStr::PrintableString(m_Job.error_msg) << '"';
    }

    output_stream << NcbiEndl;

    CStringOrBlobStorageReader job_output_reader(m_Job.output, nc_api);
    CRStream job_output_istream(&job_output_reader);
    NcbiStreamCopy(output_stream, job_output_istream);

    return target_file;
}

END_NCBI_SCOPE
