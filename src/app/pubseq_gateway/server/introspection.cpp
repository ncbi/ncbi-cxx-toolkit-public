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
 *   PSG introspection auxiliary functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
USING_NCBI_SCOPE;

#include "introspection.hpp"
#include "pubseq_gateway_version.hpp"

// Auxiliary functions to append parameters
void AppendBlobIdParameter(CJsonNode &  node)
{
    CJsonNode   blob_id(CJsonNode::NewObjectNode());
    blob_id.SetBoolean("mandatory", true);
    blob_id.SetString("description",
        "<sat>.<sat_key>. The blob sat and sat key. "
        "Both must be positive integers.");
    node.SetByKey("blob_id", blob_id);
}

void AppendTseOptionParameter(CJsonNode &  node,
                              const string &  default_value)
{
    CJsonNode   tse_option(CJsonNode::NewObjectNode());
    tse_option.SetBoolean("mandatory", false);
    tse_option.SetString("description",
        "TSE option. The following blobs depending on the value:\n"
        "Value | ID2 split available                          | ID2 split not available\n"
        "none  | Nothing                                      | Nothing\n"
        "whole | Split INFO blob only                         | Nothing\n"
        "orig  | Split INFO blob only                         | All Cassandra data chunks of the blob itself\n"
        "smart | All split blobs                              | All Cassandra data chunks of the blob itself\n"
        "slim  | All Cassandra data chunks of the blob itself | All Cassandra data chunks of the blob itself\n"
        "Default: " + default_value);
    node.SetByKey("tse", tse_option);
}

void AppendLastModifiedParameter(CJsonNode &  node)
{
    CJsonNode   last_modified(CJsonNode::NewObjectNode());
    last_modified.SetBoolean("mandatory", false);
    last_modified.SetString("description",
        "Last modified, integer. If provided then the exact match will be "
        "requested with the Cassandra storage corresponding field value. "
        "By default the most recent match will be provided.");
    node.SetByKey("last_modified", last_modified);
}

void AppendUseCacheParameter(CJsonNode &  node)
{
    CJsonNode   use_cache(CJsonNode::NewObjectNode());
    use_cache.SetBoolean("mandatory", false);
    use_cache.SetString("description",
        "Allowed values:\n"
        "no: do not use LMDB cache (tables SI2CSI, BIOSEQ_INFO and BLOB_PROP) "
        "at all; go straight to Cassandra storage.\n"
        "yes: do not use tables SI2CSI, BIOSEQ_INFO and BLOB_PROP from "
        "Cassandra storage at all. I.e., exclusively use the cache for all "
        "seq-id resolution steps. If the seq-id cannot be fully resolved "
        "through the cache alone, then code 404 is returned.\n"
        "By default (no use_cache option specified), the behavior is to use "
        "the LMDB cache if at all possible; then, fallback to Cassandra storage.");
    node.SetByKey("use_cache", use_cache);
}

void AppendTraceParameter(CJsonNode &  node)
{
    CJsonNode   trace(CJsonNode::NewObjectNode());
    trace.SetBoolean("mandatory", false);
    trace.SetString("description",
        "The option to include trace messages to the server output. "
        "Acceptable values: yes and no.");
    node.SetByKey("trace", trace);
}

void AppendSeqIdParameter(CJsonNode &  node)
{
    CJsonNode   seq_id(CJsonNode::NewObjectNode());
    seq_id.SetBoolean("mandatory", true);
    seq_id.SetString("description",
        "SeqId of the blob (string).");
    node.SetByKey("seq_id", seq_id);
}

void AppendSeqIdTypeParameter(CJsonNode &  node)
{
    CJsonNode   seq_id_type(CJsonNode::NewObjectNode());
    seq_id_type.SetBoolean("mandatory", false);
    seq_id_type.SetString("description",
        "SeqId type of the blob (integer > 0).");
    node.SetByKey("seq_id_type", seq_id_type);
}

void AppendExcludeBlobsParameter(CJsonNode &  node)
{
    CJsonNode   exclude_blobs(CJsonNode::NewObjectNode());
    exclude_blobs.SetBoolean("mandatory", false);
    exclude_blobs.SetString("description",
        "A comma separated list of BlobId (<sat>.<sat_key>) which client "
        "already has. If provided then if the resolution od seq_id/seq_id_type "
        "matches one of the blob id then the blob will not be sent.");
    node.SetByKey("exclude_blobs", exclude_blobs);
}

void AppendClientIdParameter(CJsonNode &  node)
{
    CJsonNode   client_id(CJsonNode::NewObjectNode());
    client_id.SetBoolean("mandatory", false);
    client_id.SetString("description",
        "The client identifier (string). If provided then the exclude blob "
        "feature takes place.");
    node.SetByKey("client_id", client_id);
}

void AppendAccSubstitutionParameter(CJsonNode &  node)
{
    CJsonNode   acc_substitution(CJsonNode::NewObjectNode());
    acc_substitution.SetBoolean("mandatory", false);
    acc_substitution.SetString("description",
        "The option controls how the bioseq info accession substation is done."
        "The supported policy values are:\n"
        "default: substitute if version value (version <= 0) or seq_id_type is Gi(12)\n"
        "limited: substitute only if the resolved record's seq_id_type is GI(12)\n"
        "never: the accession substitution is never done\n"
        "If the substitution is needed then the seq_ids list is analyzed. "
        "If there is one with Gi then it is taken for substitution. "
        "Otherwise an arbitrary one is picked.");
    node.SetByKey("acc_substitution", acc_substitution);
}

void AppendTseIdParameter(CJsonNode &  node)
{
    CJsonNode   tse_id(CJsonNode::NewObjectNode());
    tse_id.SetBoolean("mandatory", true);
    tse_id.SetString("description",
        "<sat>.<sat_key>. The TSE blob sat and sat key. "
        "Both must be positive integers.");
    node.SetByKey("tse_id", tse_id);
}

void AppendChunkParameter(CJsonNode &  node)
{
    CJsonNode   chunk(CJsonNode::NewObjectNode());
    chunk.SetBoolean("mandatory", true);
    chunk.SetString("description",
        "The requied TSE blob chunk number. It must be greater than 0 integer.");
    node.SetByKey("chunk", chunk);
}

void AppendSplitVersionParameter(CJsonNode &  node)
{
    CJsonNode   split_version(CJsonNode::NewObjectNode());
    split_version.SetBoolean("mandatory", true);
    split_version.SetString("description",
        "The TSE blob split version. It must be an integer.");
    node.SetByKey("split_version", split_version);
}

void AppendFmtParameter(CJsonNode &  node)
{
    CJsonNode   fmt(CJsonNode::NewObjectNode());
    fmt.SetBoolean("mandatory", false);
    fmt.SetString("description",
        "The format of the data sent to the client (string). Accepted values:\n"
        "protobuf: bioseq info will be sent as a protobuf binary data\n"
        "json: bioseq info will be sent as a serialized JSON dictionary\n"
        "native:  the server decides what format to use: protobuf or json.\n"
        "Default: native");
    node.SetByKey("fmt", fmt);
}

void AppendBioseqFlagParameter(CJsonNode &  node, const string &  flag_name)
{
    CJsonNode   flag(CJsonNode::NewObjectNode());
    flag.SetBoolean("mandatory", false);
    flag.SetString("description",
        "A flag to specify explicitly what values to include/exclude "
        "from the provided bioseq info. The accepted values are yes and no. "
        "Default: no");
    node.SetByKey(flag_name, flag);
}

void AppendNamesParameter(CJsonNode &  node)
{
    CJsonNode   names(CJsonNode::NewObjectNode());
    names.SetBoolean("mandatory", true);
    names.SetString("description",
        "A comma separated list of named annotations to be retrieved.");
    node.SetByKey("names", names);
}

void AppendUsernameParameter(CJsonNode &  node)
{
    CJsonNode   username(CJsonNode::NewObjectNode());
    username.SetBoolean("mandatory", false);
    username.SetString("description",
        "The user name who wanted to do the shutdown (string). "
        "Default: empty string.");
    node.SetByKey("username", username);
}

void AppendAuthTokenParameter(CJsonNode &  node)
{
    CJsonNode   auth_token(CJsonNode::NewObjectNode());
    auth_token.SetBoolean("mandatory", false);
    auth_token.SetString("description",
        "Authorization token (string). If the configuration "
        "[ADMIN]/auth_token value is provided then the request must have the "
        "token value matching the configured to be granted. "
        "Default: empty string.");
    node.SetByKey("auth_token", auth_token);
}

void AppendTimeoutParameter(CJsonNode &  node)
{
    CJsonNode   timeout(CJsonNode::NewObjectNode());
    timeout.SetBoolean("mandatory", false);
    timeout.SetString("description",
        "The timeout in seconds within which the shutdown must be performed "
        "(integer). If 0 then it leads to an immediate shutdown. If 1 or more "
        "seconds then the server will reject all new requests and waits till "
        "the timeout is over or all the pending requests are completed and "
        "then do the shutdown. Default: 10 (seconds)");
    node.SetByKey("timeout", timeout);
}

void AppendAlertParameter(CJsonNode &  node)
{
    CJsonNode   alert(CJsonNode::NewObjectNode());
    alert.SetBoolean("mandatory", true);
    alert.SetString("description",
        "The alert identifier to acknowledge (string)");
    node.SetByKey("alert", alert);
}

void AppendAckAlertUsernameParameter(CJsonNode &  node)
{
    CJsonNode   username(CJsonNode::NewObjectNode());
    username.SetBoolean("mandatory", true);
    username.SetString("description",
        "The user name who acknowledges the alert (string).");
    node.SetByKey("username", username);
}

void AppendResetParameter(CJsonNode &  node)
{
    CJsonNode   reset_param(CJsonNode::NewObjectNode());
    reset_param.SetBoolean("mandatory", false);
    reset_param.SetString("description",
        "If provided then the collected statistics is rest. Otherwise the "
        "collected statistics is sent to the client. "
        "Accepted values yes and no. Default: no");
    node.SetByKey("reset", reset_param);
}

void AppendMostRecentTimeParameter(CJsonNode &  node)
{
    CJsonNode   most_recent_time(CJsonNode::NewObjectNode());
    most_recent_time.SetBoolean("mandatory", false);
    most_recent_time.SetString("description",
        "Number of seconds in the past for the most recent time range limit. "
        "Default: 0");
    node.SetByKey("most_recent_time", most_recent_time);
}

void AppendMostAncientTimeParameter(CJsonNode &  node)
{
    CJsonNode   most_ancient_time(CJsonNode::NewObjectNode());
    most_ancient_time.SetBoolean("mandatory", false);
    most_ancient_time.SetString("description",
        "Number of seconds in the past for the most ancient time range limit. "
        "Default: infinity");
    node.SetByKey("most_ancient_time", most_ancient_time);
}

void AppendHistogramNamesParameter(CJsonNode &  node)
{
    CJsonNode   histogram_names(CJsonNode::NewObjectNode());
    histogram_names.SetBoolean("mandatory", false);
    histogram_names.SetString("description",
        "Comma separated list of the histogram names. If provided then "
        "the server returns all existing histograms "
        "(listed in histogram_names) which intersect with "
        "the specified time period.");
    node.SetByKey("histogram_names", histogram_names);
}

void AppendReturnDataSizeParameter(CJsonNode &  node)
{
    CJsonNode   return_data_size(CJsonNode::NewObjectNode());
    return_data_size.SetBoolean("mandatory", true);
    return_data_size.SetString("description",
        "Size in bytes (positive integer up to 1000000000) which should be "
        "sent to the client. The data are random.");
    node.SetByKey("return_data_size", return_data_size);
}

void AppendLogParameter(CJsonNode &  node)
{
    CJsonNode   log_param(CJsonNode::NewObjectNode());
    log_param.SetBoolean("mandatory", false);
    log_param.SetString("description",
        "Boolean parameter which tells if the logging of the request "
        "is done or not. Accepted values are yes and no. Default: no");
    node.SetByKey("log", log_param);
}


// Requests descriptions

CJsonNode  GetIdGetblobRequestNode(void)
{
    CJsonNode   id_getblob(CJsonNode::NewObjectNode());
    id_getblob.SetString("description",
        "Retrieves blob chunks basing on the blob sat and sat_key");

    CJsonNode   id_getblob_params(CJsonNode::NewObjectNode());
    AppendBlobIdParameter(id_getblob_params);
    AppendTseOptionParameter(id_getblob_params, "orig");
    AppendLastModifiedParameter(id_getblob_params);
    AppendUseCacheParameter(id_getblob_params);
    AppendTraceParameter(id_getblob_params);
    id_getblob.SetByKey("parameters", id_getblob_params);

    CJsonNode   id_getblob_reply(CJsonNode::NewObjectNode());
    id_getblob_reply.SetString("description",
        "The PSG protocol is used in the HTML content. "
        "The blob properties and chunks are provided.");
    id_getblob.SetByKey("reply", id_getblob_reply);

    return id_getblob;
}


CJsonNode  GetIdGetRequestNode(void)
{
    CJsonNode   id_get(CJsonNode::NewObjectNode());
    id_get.SetString("description",
        "Retrieves blob chunks basing on the seq_id and seq_id_type");
    CJsonNode   id_get_params(CJsonNode::NewObjectNode());

    AppendSeqIdParameter(id_get_params);
    AppendSeqIdTypeParameter(id_get_params);
    AppendTseOptionParameter(id_get_params, "orig");
    AppendUseCacheParameter(id_get_params);
    AppendExcludeBlobsParameter(id_get_params);
    AppendClientIdParameter(id_get_params);
    AppendAccSubstitutionParameter(id_get_params);
    AppendTraceParameter(id_get_params);
    id_get.SetByKey("parameters", id_get_params);

    CJsonNode   id_get_reply(CJsonNode::NewObjectNode());
    id_get_reply.SetString("description",
        "The PSG protocol is used in the HTML content. "
        "The bioseq info, blob properties and chunks are provided.");
    id_get.SetByKey("reply", id_get_reply);
    return id_get;
}


CJsonNode  GetIdGetTseChunkRequestNode(void)
{
    CJsonNode   id_get_tse_chunk(CJsonNode::NewObjectNode());
    id_get_tse_chunk.SetString("description",
        "Retrieves a TSE blob chunk basing on the blob sat and sat_key");
    CJsonNode   id_get_tse_chunk_params(CJsonNode::NewObjectNode());

    AppendTseIdParameter(id_get_tse_chunk_params);
    AppendChunkParameter(id_get_tse_chunk_params);
    AppendSplitVersionParameter(id_get_tse_chunk_params);
    AppendUseCacheParameter(id_get_tse_chunk_params);
    AppendTraceParameter(id_get_tse_chunk_params);
    id_get_tse_chunk.SetByKey("parameters", id_get_tse_chunk_params);

    CJsonNode   id_get_tse_chunk_reply(CJsonNode::NewObjectNode());
    id_get_tse_chunk_reply.SetString("description",
        "The PSG protocol is used in the HTML content. "
        "The blob properties and chunks are provided.");
    id_get_tse_chunk.SetByKey("reply", id_get_tse_chunk_reply);

    return id_get_tse_chunk;
}


CJsonNode  GetIdResolveRequestNode(void)
{
    CJsonNode   id_resolve(CJsonNode::NewObjectNode());
    id_resolve.SetString("description",
        "Resolve seq_id and provide bioseq info");
    CJsonNode   id_resolve_params(CJsonNode::NewObjectNode());

    AppendSeqIdParameter(id_resolve_params);
    AppendSeqIdTypeParameter(id_resolve_params);
    AppendUseCacheParameter(id_resolve_params);
    AppendFmtParameter(id_resolve_params);
    AppendBioseqFlagParameter(id_resolve_params, "all_info");
    AppendBioseqFlagParameter(id_resolve_params, "canon_id");
    AppendBioseqFlagParameter(id_resolve_params, "seq_ids");
    AppendBioseqFlagParameter(id_resolve_params, "mol_type");
    AppendBioseqFlagParameter(id_resolve_params, "length");
    AppendBioseqFlagParameter(id_resolve_params, "state");
    AppendBioseqFlagParameter(id_resolve_params, "blob_id");
    AppendBioseqFlagParameter(id_resolve_params, "tax_id");
    AppendBioseqFlagParameter(id_resolve_params, "hash");
    AppendBioseqFlagParameter(id_resolve_params, "date_changed");
    AppendBioseqFlagParameter(id_resolve_params, "gi");
    AppendBioseqFlagParameter(id_resolve_params, "name");
    AppendBioseqFlagParameter(id_resolve_params, "seq_state");
    AppendAccSubstitutionParameter(id_resolve_params);
    AppendTraceParameter(id_resolve_params);
    id_resolve.SetByKey("parameters", id_resolve_params);

    CJsonNode   id_resolve_reply(CJsonNode::NewObjectNode());
    id_resolve_reply.SetString("description",
        "The bioseq info is sent back in the HTML content as binary protobuf "
        "or as PSG protocol chunks depending on the protocol choice");
    id_resolve.SetByKey("reply", id_resolve_reply);

    return id_resolve;
}


CJsonNode  GetIdGetNaRequestNode(void)
{
    CJsonNode   id_get_na(CJsonNode::NewObjectNode());
    id_get_na.SetString("description",
        "Retrieves named annotations");
    CJsonNode   id_get_na_params(CJsonNode::NewObjectNode());

    AppendSeqIdParameter(id_get_na_params);
    AppendSeqIdTypeParameter(id_get_na_params);
    AppendNamesParameter(id_get_na_params);
    AppendUseCacheParameter(id_get_na_params);
    AppendTseOptionParameter(id_get_na_params, "none");
    AppendFmtParameter(id_get_na_params);
    AppendTraceParameter(id_get_na_params);
    id_get_na.SetByKey("parameters", id_get_na_params);

    CJsonNode   id_get_na_reply(CJsonNode::NewObjectNode());
    id_get_na_reply.SetString("description",
        "The PSG protocol is used in the HTML content. "
        "The bioseq info and named annotation chunks are provided.");
    id_get_na.SetByKey("reply", id_get_na_reply);

    return id_get_na;
}


CJsonNode  GetAdminConfigRequestNode(void)
{
    CJsonNode   admin_config(CJsonNode::NewObjectNode());
    admin_config.SetString("description",
        "Provides the server configuration information");

    admin_config.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_config_reply(CJsonNode::NewObjectNode());
    admin_config_reply.SetString("description",
        "The HTML content is a JSON dictionary with "
        "the configuration information.");
    admin_config.SetByKey("reply", admin_config_reply);

    return admin_config;
}


CJsonNode  GetAdminInfoRequestNode(void)
{
    CJsonNode   admin_info(CJsonNode::NewObjectNode());
    admin_info.SetString("description",
        "Provides the server run-time information");

    admin_info.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_info_reply(CJsonNode::NewObjectNode());
    admin_info_reply.SetString("description",
        "The HTML content is a JSON dictionary with "
        "the run-time information like resource consumption");
    admin_info.SetByKey("reply", admin_info_reply);

    return admin_info;
}


CJsonNode  GetAdminStatusRequestNode(void)
{
    CJsonNode   admin_status(CJsonNode::NewObjectNode());
    admin_status.SetString("description",
        "Provides the server event counters");

    admin_status.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_status_reply(CJsonNode::NewObjectNode());
    admin_status_reply.SetString("description",
        "The HTML content is a JSON dictionary with "
        "various event counters");
    admin_status.SetByKey("reply", admin_status_reply);

    return admin_status;
}


CJsonNode  GetAdminShutdownRequestNode(void)
{
    CJsonNode   admin_shutdown(CJsonNode::NewObjectNode());
    admin_shutdown.SetString("description",
        "Performs the server shutdown");
    CJsonNode   admin_shutdown_params(CJsonNode::NewObjectNode());

    AppendUsernameParameter(admin_shutdown_params);
    AppendAuthTokenParameter(admin_shutdown_params);
    AppendTimeoutParameter(admin_shutdown_params);
    admin_shutdown.SetByKey("parameters", admin_shutdown_params);

    CJsonNode   admin_status_reply(CJsonNode::NewObjectNode());
    admin_status_reply.SetString("description",
        "The standard HTTP protocol is used");
    admin_shutdown.SetByKey("reply", admin_status_reply);

    return admin_shutdown;
}


CJsonNode  GetAdminGetAlertsRequestNode(void)
{
    CJsonNode   admin_get_alerts(CJsonNode::NewObjectNode());
    admin_get_alerts.SetString("description",
        "Provides the server alerts");

    admin_get_alerts.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_get_alerts_reply(CJsonNode::NewObjectNode());
    admin_get_alerts_reply.SetString("description",
        "The HTML content is a JSON dictionary with "
        "the current server alerts");
    admin_get_alerts.SetByKey("reply", admin_get_alerts_reply);

    return admin_get_alerts;
}


CJsonNode  GetAdminAckAlertsRequestNode(void)
{
    CJsonNode   admin_ack_alert(CJsonNode::NewObjectNode());
    admin_ack_alert.SetString("description",
        "Acknowledge an alert");
    CJsonNode   admin_ack_alert_params(CJsonNode::NewObjectNode());

    AppendAlertParameter(admin_ack_alert_params);
    AppendAckAlertUsernameParameter(admin_ack_alert_params);
    admin_ack_alert.SetByKey("parameters", admin_ack_alert_params);

    CJsonNode   admin_ack_alerts_reply(CJsonNode::NewObjectNode());
    admin_ack_alerts_reply.SetString("description",
        "The standard HTTP protocol is used");
    admin_ack_alert.SetByKey("reply", admin_ack_alerts_reply);

    return admin_ack_alert;
}


CJsonNode  GetAdminStatisticsRequestNode(void)
{
    CJsonNode   admin_statistics(CJsonNode::NewObjectNode());
    admin_statistics.SetString("description",
        "Provides the server collected statistics");
    CJsonNode   admin_statistics_params(CJsonNode::NewObjectNode());

    AppendResetParameter(admin_statistics_params);
    AppendMostRecentTimeParameter(admin_statistics_params);
    AppendMostAncientTimeParameter(admin_statistics_params);
    AppendHistogramNamesParameter(admin_statistics_params);
    admin_statistics.SetByKey("parameters", admin_statistics_params);

    CJsonNode   admin_statistics_reply(CJsonNode::NewObjectNode());
    admin_statistics_reply.SetString("description",
        "The HTML content is a JSON dictionary with "
        "the collected statistics information");
    admin_statistics.SetByKey("reply", admin_statistics_reply);

    return admin_statistics;
}


CJsonNode  GetTestIoRequestNode(void)
{
    CJsonNode   test_io(CJsonNode::NewObjectNode());
    test_io.SetString("description",
        "Sends back random binary data to test the I/O performance. It works "
        "only if the server configuration file has the [DEBUG]/psg_allow_io_test "
        "value set to true");
    CJsonNode   test_io_params(CJsonNode::NewObjectNode());

    AppendReturnDataSizeParameter(test_io_params);
    AppendLogParameter(test_io_params);
    test_io.SetByKey("parameters", test_io_params);

    CJsonNode   test_io_reply(CJsonNode::NewObjectNode());
    test_io_reply.SetString("description",
        "The HTML content is a random data of the requested length");
    test_io.SetByKey("reply", test_io_reply);

    return test_io;
}


CJsonNode  GetFaviconRequestNode(void)
{
    CJsonNode   favicon(CJsonNode::NewObjectNode());
    favicon.SetString("description",
        "Always replies 'not found'");

    favicon.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   favicon_reply(CJsonNode::NewObjectNode());
    favicon_reply.SetString("description",
        "The standard HTTP protocol is used");
    favicon.SetByKey("reply", favicon_reply);

    return favicon;
}


CJsonNode  GetUnknownRequestNode(void)
{
    CJsonNode   unknown(CJsonNode::NewObjectNode());
    unknown.SetString("description",
        "Always replies 'ok' in terms of http. The nested PSG protocol always "
        "tells 'bad request'");

    unknown.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   unknown_reply(CJsonNode::NewObjectNode());
    unknown_reply.SetString("description",
        "The HTML content uses PSG protocol for a 'bad request' message");
    unknown.SetByKey("reply", unknown_reply);
    return unknown;
}


CJsonNode   GetAboutNode(void)
{
    CJsonNode   about_node(CJsonNode::NewObjectNode());

    about_node.SetString("name", "PubSeq Gateway Daemon");
    about_node.SetString("description",
        "The primary daemon functionality is to provide the following services: "
        "accession resolution, blobs retrieval based on accession or on blob "
        "identification and named annotations retrieval");
    about_node.SetString("version", PUBSEQ_GATEWAY_VERSION);
    about_node.SetString("build-date", PUBSEQ_GATEWAY_BUILD_DATE);
    return about_node;
}


CJsonNode   GetRequestsNode(void)
{
    CJsonNode   requests_node(CJsonNode::NewObjectNode());

    requests_node.SetByKey("ID/getblob", GetIdGetblobRequestNode());
    requests_node.SetByKey("ID/get", GetIdGetRequestNode());
    requests_node.SetByKey("ID/get_tse_chunk", GetIdGetTseChunkRequestNode());
    requests_node.SetByKey("ID/resolve", GetIdResolveRequestNode());
    requests_node.SetByKey("ID/get_na", GetIdGetNaRequestNode());
    requests_node.SetByKey("ADMIN/config", GetAdminConfigRequestNode());
    requests_node.SetByKey("ADMIN/info", GetAdminInfoRequestNode());
    requests_node.SetByKey("ADMIN/status", GetAdminStatusRequestNode());
    requests_node.SetByKey("ADMIN/shutdown", GetAdminShutdownRequestNode());
    requests_node.SetByKey("ADMIN/get_alerts", GetAdminGetAlertsRequestNode());
    requests_node.SetByKey("ADMIN/ack_alerts", GetAdminAckAlertsRequestNode());
    requests_node.SetByKey("ADMIN/statistics", GetAdminStatisticsRequestNode());
    requests_node.SetByKey("TEST/io", GetTestIoRequestNode());
    requests_node.SetByKey("favicon.ico", GetFaviconRequestNode());
    requests_node.SetByKey("unknown", GetUnknownRequestNode());
    return requests_node;
}


CJsonNode   GetReferencesNode(void)
{
    CJsonNode   doc_link(CJsonNode::NewObjectNode());

    doc_link.SetString("description",
        "PubSeq Gateway Server Overview and the Protocol Specification");
    doc_link.SetString("link",
        "https://github.com/ncbi/cxx-toolkit/blob/gh-pages/misc/PSG%20Server.docx");

    CJsonNode   references_node(CJsonNode::NewArrayNode());
    references_node.Append(doc_link);
    return references_node;
}


CJsonNode   GetIntrospectionNode(void)
{
    CJsonNode   introspection(CJsonNode::NewObjectNode());

    introspection.SetByKey("about", GetAboutNode());
    introspection.SetByKey("requests", GetRequestsNode());
    introspection.SetByKey("references", GetReferencesNode());
    return introspection;
}

