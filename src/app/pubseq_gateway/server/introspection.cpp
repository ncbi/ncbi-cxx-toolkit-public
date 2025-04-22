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


static const string     kMandatory = "mandatory";
static const string     kDescription = "description";
static const string     kAllowedValues = "allowed values";
static const string     kDefault = "default";
static const string     kType = "type";
static const string     kTitle = "title";
static const string     kTable = "table";



// Auxiliary functions to append parameters
void AppendBlobIdParameter(CJsonNode &  node)
{
    CJsonNode   blob_id(CJsonNode::NewObjectNode());
    blob_id.SetBoolean(kMandatory, true);
    blob_id.SetString(kType, "String");
    blob_id.SetString(kDescription,
        "<sat>.<sat_key>. The blob sat and sat key. "
        "Both must be positive integers.");
    blob_id.SetString(kAllowedValues,
        "Non empty string. The interpretation of the blob id depends on "
        "a processor. Cassandra processor expects the following format: "
        "<sat>.<sat key> where boath are integers");
    blob_id.SetString(kDefault, "");
    node.SetByKey("blob_id", blob_id);
}

void AppendTseOptionParameter(CJsonNode &  node,
                              const string &  default_value)
{
    CJsonNode   tse_option(CJsonNode::NewObjectNode());
    tse_option.SetBoolean(kMandatory, false);
    tse_option.SetString(kType, "String");

    CJsonNode   tse_table(CJsonNode::NewObjectNode());
    tse_table.SetString(kTitle, "TSE option controls what blob is provided:");

    CJsonNode   table(CJsonNode::NewArrayNode());
    CJsonNode   row1(CJsonNode::NewArrayNode());
    row1.AppendString("Value");
    row1.AppendString("ID2 split available");
    row1.AppendString("ID2 split not available");
    table.Append(row1);
    CJsonNode   row2(CJsonNode::NewArrayNode());
    row2.AppendString("none");
    row2.AppendString("Nothing");
    row2.AppendString("Nothing");
    table.Append(row2);
    CJsonNode   row3(CJsonNode::NewArrayNode());
    row3.AppendString("whole");
    row3.AppendString("Split INFO blob only");
    row3.AppendString("Nothing");
    table.Append(row3);
    CJsonNode   row4(CJsonNode::NewArrayNode());
    row4.AppendString("orig");
    row4.AppendString("Split INFO blob only");
    row4.AppendString("All Cassandra data chunks of the blob itself");
    table.Append(row4);
    CJsonNode   row5(CJsonNode::NewArrayNode());
    row5.AppendString("smart");
    row5.AppendString("All split blobs");
    row5.AppendString("All Cassandra data chunks of the blob itself");
    table.Append(row5);
    CJsonNode   row6(CJsonNode::NewArrayNode());
    row6.AppendString("slim");
    row6.AppendString("All Cassandra data chunks of the blob itself");
    row6.AppendString("All Cassandra data chunks of the blob itself");
    table.Append(row6);
    tse_table.SetByKey(kTable, table);

    tse_option.SetByKey(kDescription, tse_table);
    tse_option.SetString(kAllowedValues,
        "none, whole, orig, smart and slim");
    tse_option.SetString(kDefault, default_value);
    node.SetByKey("tse", tse_option);
}

void AppendLastModifiedParameter(CJsonNode &  node)
{
    CJsonNode   last_modified(CJsonNode::NewObjectNode());
    last_modified.SetBoolean(kMandatory, false);
    last_modified.SetString(kType, "Integer");
    last_modified.SetString(kDescription,
        "The blob last modification. If provided then the exact match will be "
        "requested with the Cassandra storage corresponding field value.");
    last_modified.SetString(kAllowedValues,
        "Positive integer. Not provided means that the most recent "
        "match will be selected.");
    last_modified.SetString(kDefault,
        "");
    node.SetByKey("last_modified", last_modified);
}

void AppendUseCacheParameter(CJsonNode &  node)
{
    CJsonNode   use_cache(CJsonNode::NewObjectNode());
    use_cache.SetBoolean(kMandatory, false);
    use_cache.SetString(kType, "String");

    CJsonNode   use_cache_table(CJsonNode::NewObjectNode());
    use_cache_table.SetString(kTitle,
        "The option controls if the Cassandra LMDB cache and/or database "
        "should be used. It affects the seq id resolution step and "
        "the blob properties lookup step. The following options are available:");

    CJsonNode   table(CJsonNode::NewArrayNode());
    CJsonNode   row1(CJsonNode::NewArrayNode());
    row1.AppendString("no");
    row1.AppendString("do not use LMDB cache (tables SI2CSI, BIOSEQ_INFO and BLOB_PROP) "
                      "at all; use only Cassandra database for the lookups.");
    table.Append(row1);
    CJsonNode   row2(CJsonNode::NewArrayNode());
    row2.AppendString("yes");
    row2.AppendString("do not use tables SI2CSI, BIOSEQ_INFO and BLOB_PROP in the "
                      "Cassandra database; use only the LMDB cache.");
    table.Append(row2);
    CJsonNode   row3(CJsonNode::NewArrayNode());
    row3.AppendString("");
    row3.AppendString("use the LMDB cache if at all possible; "
                      "then, fallback to Cassandra storage.");
    table.Append(row3);
    use_cache_table.SetByKey(kTable, table);

    use_cache.SetByKey(kDescription, use_cache_table);
    use_cache.SetString(kAllowedValues,
        "yes, no and not provided");
    use_cache.SetString(kDefault, "");
    node.SetByKey("use_cache", use_cache);
}

void AppendTraceParameter(CJsonNode &  node)
{
    CJsonNode   trace(CJsonNode::NewObjectNode());
    trace.SetBoolean(kMandatory, false);
    trace.SetString(kType, "String");
    trace.SetString(kDescription,
        "The option to include trace messages to the server output");
    trace.SetString(kAllowedValues, "yes and no");
    trace.SetString(kDefault, "no");
    node.SetByKey("trace", trace);
}

void AppendSeqIdParameter(CJsonNode &  node)
{
    CJsonNode   seq_id(CJsonNode::NewObjectNode());
    seq_id.SetBoolean(kMandatory, true);
    seq_id.SetString(kType, "String");
    seq_id.SetString(kDescription,
        "Sequence identifier");
    seq_id.SetString(kAllowedValues, "A string identifier");
    seq_id.SetString(kDefault, "");
    node.SetByKey("seq_id", seq_id);
}

void AppendSeqIdParameterForGetNA(CJsonNode &  node)
{
    CJsonNode   seq_id(CJsonNode::NewObjectNode());
    seq_id.SetBoolean(kMandatory, false);
    seq_id.SetString(kType, "String");
    seq_id.SetString(kDescription,
        "Sequence identifier. "
        "This or seq_ids parameter value must be provided for the request.");
    seq_id.SetString(kAllowedValues, "A string identifier");
    seq_id.SetString(kDefault, "");
    node.SetByKey("seq_id", seq_id);
}

void AppendSeqIdsParameterForGetNA(CJsonNode &  node)
{
    CJsonNode   seq_ids(CJsonNode::NewObjectNode());
    seq_ids.SetBoolean(kMandatory, false);
    seq_ids.SetString(kType, "String");
    seq_ids.SetString(kDescription,
        "A space separated list of the sequence identifier synonims. "
        "This or seq_id parameter value must be provided for the request.");
    seq_ids.SetString(kAllowedValues,
        "A list of space separated string identifiers");
    seq_ids.SetString(kDefault, "");
    node.SetByKey("seq_ids", seq_ids);
}

void AppendSeqIdTypeParameter(CJsonNode &  node)
{
    CJsonNode   seq_id_type(CJsonNode::NewObjectNode());
    seq_id_type.SetBoolean(kMandatory, false);
    seq_id_type.SetString(kType, "Integer");
    seq_id_type.SetString(kDescription,
        "Sequence identifier type");
    seq_id_type.SetString(kAllowedValues,
        "Integer type greater than 0");
    seq_id_type.SetString(kDefault, "");
    node.SetByKey("seq_id_type", seq_id_type);
}

void AppendExcludeBlobsParameter(CJsonNode &  node)
{
    CJsonNode   exclude_blobs(CJsonNode::NewObjectNode());
    exclude_blobs.SetBoolean(kMandatory, false);
    exclude_blobs.SetString(kType, "String");
    exclude_blobs.SetString(kDescription,
        "A comma separated list of blob identifiers which client "
        "already has. If provided then if the resolution of "
        "sequence identifier/sequence identifier type "
        "matches one of the blob identifiers in the list then the blob "
        "will not be sent. The format of the blob identifier depends on "
        "a processor. For example, a Cassandra processor expects the format as "
        "<sat>.<sat key> where both of them are integers. Note: it works "
        "in conjunction with the client_id parameter.");
    exclude_blobs.SetString(kAllowedValues,
        "A list of blob indentifiers");
    exclude_blobs.SetString(kDefault, "");
    node.SetByKey("exclude_blobs", exclude_blobs);
}

void AppendClientIdParameter(CJsonNode &  node)
{
    CJsonNode   client_id(CJsonNode::NewObjectNode());
    client_id.SetBoolean(kMandatory, false);
    client_id.SetString(kType, "String");
    client_id.SetString(kDescription,
        "The client identifier. If provided then the exclude blob "
        "feature takes place.");
    client_id.SetString(kAllowedValues,
        "A string identifier");
    client_id.SetString(kDefault, "");
    node.SetByKey("client_id", client_id);
}

void AppendAccSubstitutionParameter(CJsonNode &  node)
{
    CJsonNode   acc_substitution(CJsonNode::NewObjectNode());
    acc_substitution.SetBoolean(kMandatory, false);
    acc_substitution.SetString(kType, "String");

    CJsonNode   subst_table(CJsonNode::NewObjectNode());
    subst_table.SetString(kTitle,
        "The option controls how the bioseq info accession substitution is done. "
        "If the substitution is needed then the seq_ids list is analyzed. "
        "If there is one with Gi then it is taken for substitution. "
        "Otherwise an arbitrary one is picked. "
        "The supported policy values are:");
    CJsonNode   table(CJsonNode::NewArrayNode());
    CJsonNode   row1(CJsonNode::NewArrayNode());
    row1.AppendString("default");
    row1.AppendString("substitute if version value (version <= 0) or seq_id_type is Gi(12)");
    table.Append(row1);
    CJsonNode   row2(CJsonNode::NewArrayNode());
    row2.AppendString("limited");
    row2.AppendString("substitute only if the resolved record's seq_id_type is GI(12)");
    table.Append(row2);
    CJsonNode   row3(CJsonNode::NewArrayNode());
    row3.AppendString("never");
    row3.AppendString("the accession substitution is never done");
    table.Append(row3);
    subst_table.SetByKey(kTable, table);

    acc_substitution.SetByKey(kDescription, subst_table);
    acc_substitution.SetString(kAllowedValues,
        "limited, never or default");
    acc_substitution.SetString(kDefault, "default");
    node.SetByKey("acc_substitution", acc_substitution);
}

void AppendId2ChunkParameter(CJsonNode &  node)
{
    CJsonNode   id2_chunk(CJsonNode::NewObjectNode());
    id2_chunk.SetBoolean(kMandatory, true);
    id2_chunk.SetString(kType, "Integer");
    id2_chunk.SetString(kDescription,
        "The tse blob chunk number. The Cassandra processor recognizes "
        "a special value of 999999999 for the parameter. In this case the "
        "effective chunk number will be taken from the id2_info parameter");
    id2_chunk.SetString(kAllowedValues,
        "Integer greater or equal 0. "
        "Some processors may introduce more strict rules. For example, "
        "Cassandra processor requires the chunk number to be greater than 0.");
    id2_chunk.SetString(kDefault, "");
    node.SetByKey("id2_chunk", id2_chunk);
}

void AppendId2InfoParameter(CJsonNode &  node)
{
    CJsonNode   id2_info(CJsonNode::NewObjectNode());
    id2_info.SetBoolean(kMandatory, true);
    id2_info.SetString(kType, "String");

    CJsonNode   fmt_table(CJsonNode::NewObjectNode());
    fmt_table.SetString(kTitle,
        "The following formats are recognized:");

    CJsonNode   table(CJsonNode::NewArrayNode());
    CJsonNode   row1(CJsonNode::NewArrayNode());
    row1.AppendString("Cassandra processor, option 1");
    row1.AppendString("3 or 4 integers separated by '.': "
                      "<sat>.<info>.<chunks>[.<split version>]");
    table.Append(row1);
    CJsonNode   row2(CJsonNode::NewArrayNode());
    row2.AppendString("Cassandra processor, option 2");
    row2.AppendString("psg~~tse_id-<sat>.<sat key>[~~tse_last_modified-<int>[~~tse_split_version-<int>]");
    table.Append(row2);
    CJsonNode   row3(CJsonNode::NewArrayNode());
    row3.AppendString("Other processors");
    row3.AppendString("id2~~tse_id-<string>~~tse_last_modified-<int>~~tse_split_version-<int>");
    table.Append(row3);
    fmt_table.SetByKey(kTable, table);

    id2_info.SetByKey(kDescription, fmt_table);
    id2_info.SetString(kAllowedValues,
        "A string in a format recognisable by one of the processors");
    id2_info.SetString(kDefault, "");
    node.SetByKey("id2_info", id2_info);
}

void AppendProteinParameter(CJsonNode &  node)
{
    CJsonNode   protein(CJsonNode::NewObjectNode());
    protein.SetBoolean(kMandatory, true);
    protein.SetString(kType, "String");
    protein.SetString(kDescription,
        "The protein to be resolved. It may be ommitted if ipg is provided.");
    protein.SetString(kAllowedValues,
        "A string identifier");
    protein.SetString(kDefault, "");
    node.SetByKey("protein", protein);
}

void AppendNucleotideParameter(CJsonNode &  node)
{
    CJsonNode   nucleotide(CJsonNode::NewObjectNode());
    nucleotide.SetBoolean(kMandatory, false);
    nucleotide.SetString(kType, "String");
    nucleotide.SetString(kDescription,
        "The nucleotide to be resolved.");
    nucleotide.SetString(kAllowedValues,
        "A string identifier");
    nucleotide.SetString(kDefault, "");
    node.SetByKey("nucleotide", nucleotide);
}

void AppendIpgParameter(CJsonNode &  node)
{
    CJsonNode   ipg(CJsonNode::NewObjectNode());
    ipg.SetBoolean(kMandatory, true);
    ipg.SetString(kType, "Integer");
    ipg.SetString(kDescription,
        "The ipg to be resolved. It may be ommitted if protein is provided.");
    ipg.SetString(kAllowedValues,
        "An integer greater than 0");
    ipg.SetString(kDefault, "");
    node.SetByKey("ipg", ipg);
}

void AppendFmtParameter(CJsonNode &  node)
{
    CJsonNode   fmt(CJsonNode::NewObjectNode());
    fmt.SetBoolean(kMandatory, false);
    fmt.SetString(kType, "String");

    CJsonNode   fmt_table(CJsonNode::NewObjectNode());
    fmt_table.SetString(kTitle,
        "The format of the data sent to the client. Available options:");

    CJsonNode   table(CJsonNode::NewArrayNode());
    CJsonNode   row1(CJsonNode::NewArrayNode());
    row1.AppendString("protobuf");
    row1.AppendString("bioseq info will be sent as a protobuf binary data");
    table.Append(row1);
    CJsonNode   row2(CJsonNode::NewArrayNode());
    row2.AppendString("json");
    row2.AppendString("bioseq info will be sent as a serialized JSON dictionary");
    table.Append(row2);
    CJsonNode   row3(CJsonNode::NewArrayNode());
    row3.AppendString("native");
    row3.AppendString("the server decides what format to use: protobuf or json");
    table.Append(row3);
    fmt_table.SetByKey(kTable, table);

    fmt.SetByKey(kDescription, fmt_table);
    fmt.SetString(kAllowedValues,
        "protobuf, json or native");
    fmt.SetString(kDefault, "native");
    node.SetByKey("fmt", fmt);
}

void AppendBioseqFlagParameter(CJsonNode &  node, const string &  flag_name)
{
    CJsonNode   flag(CJsonNode::NewObjectNode());
    flag.SetBoolean(kMandatory, false);
    flag.SetString(kType, "String");
    flag.SetString(kDescription,
        "A flag to specify explicitly what values to include/exclude "
        "from the provided bioseq info");
    flag.SetString(kAllowedValues,
        "yes and no");
    flag.SetString(kDefault, "no");
    node.SetByKey(flag_name, flag);
}

void AppendIncludeHupParameter(CJsonNode &  node)
{
    CJsonNode   include_hup(CJsonNode::NewObjectNode());
    include_hup.SetBoolean(kMandatory, false);
    include_hup.SetString(kType, "String");
    include_hup.SetString(kDescription,
        "Explicitly tells the server if it should try to retrieve data from HUP keyspaces. "
        "If data are coming from a secure keyspace then the following logic is used. "
        "If the include_hup option is provided then the decision is made basing on the provided value. "
        "Otherwise a decision is made basing on the presence of the WebCubbyUser cookie. "
        "If it was decided that HUP data needs to be provided then the server uses the "
        "WebCubbyUser cookie value to perform an authorization check.");
    include_hup.SetString(kAllowedValues,
        "yes and no");
    include_hup.SetString(kDefault, "");
    node.SetByKey("include_hup", include_hup);
}

void AppendNamesParameter(CJsonNode &  node)
{
    CJsonNode   names(CJsonNode::NewObjectNode());
    names.SetBoolean(kMandatory, true);
    names.SetString(kType, "String");
    names.SetString(kDescription,
        "A comma separated list of named annotations to be retrieved.");
    names.SetString(kAllowedValues,
        "A string");
    names.SetString(kDefault, "");
    node.SetByKey("names", names);
}

void AppendHopsParameter(CJsonNode &  node)
{
    CJsonNode   hops(CJsonNode::NewObjectNode());
    hops.SetBoolean(kMandatory, false);
    hops.SetString(kType, "Integer");
    hops.SetString(kDescription,
        "A numbers of hops before the request reached the server");
    hops.SetString(kAllowedValues, "An integer greater or equal 0");
    hops.SetInteger(kDefault, 0);
    node.SetByKey("hops", hops);
}

void AppendEnableProcessorParameter(CJsonNode &  node)
{
    CJsonNode   enable_processor(CJsonNode::NewObjectNode());
    enable_processor.SetBoolean(kMandatory, false);
    enable_processor.SetString(kType, "String");
    enable_processor.SetString(kDescription,
        "A name of a processor which is allowed to process a request. "
        "The parameter can be repeated as many times as needed.");
    enable_processor.SetString(kAllowedValues, "A string");
    enable_processor.SetString(kDefault, "");
    node.SetByKey("enable_processor", enable_processor);
}

void AppendDisableProcessorParameter(CJsonNode &  node)
{
    CJsonNode   disable_processor(CJsonNode::NewObjectNode());
    disable_processor.SetBoolean(kMandatory, false);
    disable_processor.SetString(kType, "String");
    disable_processor.SetString(kDescription,
        "A name of a processor which is disallowed to process a request. "
        "The parameter can be repeated as many times as needed.");
    disable_processor.SetString(kAllowedValues, "A string");
    disable_processor.SetString(kDefault, "");
    node.SetByKey("disable_processor", disable_processor);
}

void AppendZExcludeParameter(CJsonNode &  node)
{
    CJsonNode   zexclude(CJsonNode::NewObjectNode());
    zexclude.SetBoolean(kMandatory, false);
    zexclude.SetString(kType, "String");
    zexclude.SetString(kDescription,
        "A name of a check which should not be performed. "
        "The parameter can be repeated as many times as needed.");
    zexclude.SetString(kAllowedValues, "A string");
    zexclude.SetString(kDefault, "");
    node.SetByKey("exclude", zexclude);
}

void AppendProcessorEventsParameter(CJsonNode &  node)
{
    CJsonNode   processor_events(CJsonNode::NewObjectNode());
    processor_events.SetBoolean(kMandatory, false);
    processor_events.SetString(kType, "String");
    processor_events.SetString(kDescription,
        "Switch on/off additional reply chunks which tell about the processor");
    processor_events.SetString(kAllowedValues, "yes and no");
    processor_events.SetString(kDefault, "no");
    node.SetByKey("processor_events", processor_events);
}

void AppendSendBlobIfSmallParameter(CJsonNode &  node)
{
    CJsonNode   send_blob_if_small(CJsonNode::NewObjectNode());
    send_blob_if_small.SetBoolean(kMandatory, false);
    send_blob_if_small.SetString(kType, "Integer");

    CJsonNode   send_table(CJsonNode::NewObjectNode());
    send_table.SetString(kTitle,
        "Controls what blob or chunk will be sent to the client. "
        "If [SERVER]/send_blob_if_small config value is greater than "
        "what is provided then [SERVER]/send_blob_if_small will be used.");

    CJsonNode   table(CJsonNode::NewArrayNode());
    CJsonNode   row1(CJsonNode::NewArrayNode());
    row1.AppendString("tse (value of the tse URL parameter)");
    row1.AppendString("id2-split (whether the ID2-split version of the blob is available)");
    row1.AppendString("small blob (size of the compressed blob data <= send_blob_if_small)");
    row1.AppendString("large blob (size of the compressed blob data > send_blob_if_small)");
    table.Append(row1);
    CJsonNode   row2(CJsonNode::NewArrayNode());
    row2.AppendString("slim");
    row2.AppendString("no");
    row2.AppendString("Send original (non-split) blob data");
    row2.AppendString("Do not send original (non-split) blob data");
    table.Append(row2);
    CJsonNode   row3(CJsonNode::NewArrayNode());
    row3.AppendString("smart");
    row3.AppendString("no");
    row3.AppendString("Send original (non-split) blob data");
    row3.AppendString("Send original (non-split) blob data");
    table.Append(row3);
    CJsonNode   row4(CJsonNode::NewArrayNode());
    row4.AppendString("slim");
    row4.AppendString("yes");
    row4.AppendString("Send all ID2 chunks of the blob");
    row4.AppendString("Send only split-info chunk");
    table.Append(row4);
    CJsonNode   row5(CJsonNode::NewArrayNode());
    row5.AppendString("smart");
    row5.AppendString("yes");
    row5.AppendString("Send all ID2 chunks of the blob");
    row5.AppendString("Send only split-info chunk");
    table.Append(row5);
    send_table.SetByKey(kTable, table);

    send_blob_if_small.SetByKey(kDescription, table);
    send_blob_if_small.SetString(kAllowedValues,
        "An integer greater or equal 0");
    send_blob_if_small.SetString(kDefault, "0");
    node.SetByKey("send_blob_if_small", send_blob_if_small);
}

void AppendSeqIdResolveParameter(CJsonNode &  node)
{
    CJsonNode   seq_id_resolve(CJsonNode::NewObjectNode());
    seq_id_resolve.SetBoolean(kMandatory, false);
    seq_id_resolve.SetString(kType, "String");
    seq_id_resolve.SetString(kDescription,
        "If yes then use the full resolution procedure using all provided "
        "sequence identifiers. Otherwise use only bioseq info table and "
        "GI sequence identifiers.");
    seq_id_resolve.SetString(kAllowedValues, "yes and no");
    seq_id_resolve.SetString(kDefault, "yes");
    node.SetByKey("seq_id_resolve", seq_id_resolve);
}

void AppendUsernameParameter(CJsonNode &  node)
{
    CJsonNode   username(CJsonNode::NewObjectNode());
    username.SetBoolean(kMandatory, false);
    username.SetString(kType, "String");
    username.SetString(kDescription,
        "The user name who requested the shutdown");
    username.SetString(kAllowedValues, "A string identifier");
    username.SetString(kDefault, "");
    node.SetByKey("username", username);
}

void AppendAuthTokenParameter(CJsonNode &  node)
{
    CJsonNode   auth_token(CJsonNode::NewObjectNode());
    auth_token.SetBoolean(kMandatory, false);
    auth_token.SetString(kType, "String");
    auth_token.SetString(kDescription,
        "Authorization token. If the configuration "
        "[ADMIN]/auth_token value is configured then the request must have the "
        "token value matching the configured to be granted.");
    auth_token.SetString(kAllowedValues, "A string identifier");
    auth_token.SetString(kDefault, "");
    node.SetByKey("auth_token", auth_token);
}

void AppendTimeoutParameter(CJsonNode &  node)
{
    CJsonNode   timeout(CJsonNode::NewObjectNode());
    timeout.SetBoolean(kMandatory, false);
    timeout.SetString(kType, "Integer");
    timeout.SetString(kDescription,
        "The timeout in seconds within which the shutdown must be performed. "
        "If 0 then it leads to an immediate shutdown. If 1 or more "
        "seconds then the server will reject all new requests and wait till "
        "the timeout is over or all the pending requests are completed and "
        "then do the shutdown.");
    timeout.SetString(kAllowedValues, "An integer greater or equal 0");
    timeout.SetString(kDefault, "10");
    node.SetByKey("timeout", timeout);
}

void AppendAlertParameter(CJsonNode &  node)
{
    CJsonNode   alert(CJsonNode::NewObjectNode());
    alert.SetBoolean(kMandatory, true);
    alert.SetString(kType, "String");
    alert.SetString(kDescription,
        "The alert identifier to acknowledge");
    alert.SetString(kAllowedValues, "A string identifier");
    alert.SetString(kDefault, "");
    node.SetByKey("alert", alert);
}

void AppendAckAlertUsernameParameter(CJsonNode &  node)
{
    CJsonNode   username(CJsonNode::NewObjectNode());
    username.SetBoolean(kMandatory, true);
    username.SetString(kType, "String");
    username.SetString(kDescription,
        "The user name who acknowledges the alert");
    username.SetString(kAllowedValues, "A string identifier");
    username.SetString(kDefault, "");
    node.SetByKey("username", username);
}

void AppendResetParameter(CJsonNode &  node)
{
    CJsonNode   reset_param(CJsonNode::NewObjectNode());
    reset_param.SetBoolean(kMandatory, false);
    reset_param.SetString(kType, "String");
    reset_param.SetString(kDescription,
        "If provided then the collected statistics is rest. Otherwise the "
        "collected statistics is sent to the client.");
    reset_param.SetString(kAllowedValues, "yes and no");
    reset_param.SetString(kDefault, "no");
    node.SetByKey("reset", reset_param);
}

void AppendZVerboseParameter(CJsonNode &  node)
{
    CJsonNode   zverbose_param(CJsonNode::NewObjectNode());
    zverbose_param.SetBoolean(kMandatory, false);
    zverbose_param.SetString(kType, "");
    zverbose_param.SetString(kDescription,
        "This is a flag parameter, no value is needed "
        "(if value is provided then it will be silently ignored). "
        "If the flag is provided then the reply HTTP body "
        "will contain a JSON dictionary, Otherwise the reply HTTP body will be empty");
    zverbose_param.SetString(kAllowedValues, "");
    zverbose_param.SetString(kDefault, "");
    node.SetByKey("verbose", zverbose_param);
}

void AppendMostRecentTimeParameter(CJsonNode &  node)
{
    CJsonNode   most_recent_time(CJsonNode::NewObjectNode());
    most_recent_time.SetBoolean(kMandatory, false);
    most_recent_time.SetString(kType, "Integer");
    most_recent_time.SetString(kDescription,
        "Number of seconds in the past for the most recent time range limit");
    most_recent_time.SetString(kAllowedValues, "An integer greater or equal 0");
    most_recent_time.SetString(kDefault, "0");
    node.SetByKey("most_recent_time", most_recent_time);
}

void AppendMostAncientTimeParameter(CJsonNode &  node)
{
    CJsonNode   most_ancient_time(CJsonNode::NewObjectNode());
    most_ancient_time.SetBoolean(kMandatory, false);
    most_ancient_time.SetString(kType, "Integer");
    most_ancient_time.SetString(kDescription,
        "Number of seconds in the past for the most ancient time range limit");
    most_ancient_time.SetString(kAllowedValues, "An integer greater or equal 0");
    most_ancient_time.SetString(kDefault, "Effectively infinity, max integer");
    node.SetByKey("most_ancient_time", most_ancient_time);
}

void AppendHistogramNamesParameter(CJsonNode &  node)
{
    CJsonNode   histogram_names(CJsonNode::NewObjectNode());
    histogram_names.SetBoolean(kMandatory, false);
    histogram_names.SetString(kType, "String");
    histogram_names.SetString(kDescription,
        "Comma separated list of the histogram names. If provided then "
        "the server returns all existing histograms "
        "(listed in histogram_names) which intersect with "
        "the specified time period.");
    histogram_names.SetString(kAllowedValues, "A comma separated list of string identifiers");
    histogram_names.SetString(kDefault, "");
    node.SetByKey("histogram_names", histogram_names);
}

void AppendReturnDataSizeParameter(CJsonNode &  node)
{
    CJsonNode   return_data_size(CJsonNode::NewObjectNode());
    return_data_size.SetBoolean(kMandatory, true);
    return_data_size.SetString(kType, "Integer");
    return_data_size.SetString(kDescription,
        "Size in bytes (positive integer up to 1000000000) which should be "
        "sent to the client. The data are random.");
    return_data_size.SetString(kAllowedValues, "An integer in the range 0 ... 1000000000");
    return_data_size.SetString(kDefault, "");
    node.SetByKey("return_data_size", return_data_size);
}

void AppendLogParameter(CJsonNode &  node)
{
    CJsonNode   log_param(CJsonNode::NewObjectNode());
    log_param.SetBoolean(kMandatory, false);
    log_param.SetString(kType, "String");
    log_param.SetString(kDescription,
        "It tells if the logging of the request should be done or not");
    log_param.SetString(kAllowedValues, "yes and no");
    log_param.SetString(kDefault, "no");
    node.SetByKey("log", log_param);
}


void AppendTimeSeriesParameter(CJsonNode &  node)
{
    CJsonNode   time_series_param(CJsonNode::NewObjectNode());
    time_series_param.SetBoolean(kMandatory, false);
    time_series_param.SetString(kType, "String");
    time_series_param.SetString(kDescription,
        "Describes the aggregation of the per-minute data collected "
        "by the server. Format: <int>:<int>[ <int:<int>]* <int>:\n"
        "There are pairs of integers divided by ':'. "
        "The pairs are divided by spaces. The first integer is how many "
        "minutes to be aggregated, the second integer is the last index "
        "of the data sequence to be aggregated. For each aggregation "
        "the server calculates the average number of requests per second. "
        "The last pair must not have the second integer - this is an item "
        "which describes the aggregation till the end of the available data. "
        "A special value is also supported: no. This value means that "
        "the server will not send time series data at all.");
    time_series_param.SetString(kAllowedValues, "no or aggregation description string");
    time_series_param.SetString(kDefault, "1:59 5:1439 60:");
    node.SetByKey("time_series", time_series_param);
}

void AppendResendTimeoutParameter(CJsonNode &  node)
{
    CJsonNode   resend_timeout_param(CJsonNode::NewObjectNode());
    resend_timeout_param.SetBoolean(kMandatory, false);
    resend_timeout_param.SetString(kType, "Float");
    resend_timeout_param.SetString(kDescription,
        "If the blob has already been sent to the client more than "
        "this time ago then the blob will be sent anyway. If less then "
        "the 'already sent' reply will have an additional field "
        "'sent_seconds_ago' with the corresponding value. "
        "The special value 0 means that the blob will be sent regardless "
        "when it was already sent.");
    resend_timeout_param.SetString(kAllowedValues,
        "Floating point value greater or equal 0.0");
    resend_timeout_param.SetString(kDefault,
        "Taken from [SERVER]/resend_timeout configuration setting");
    node.SetByKey("resend_timeout", resend_timeout_param);
}


void AppendSNPScaleLimitParameter(CJsonNode &  node)
{
    CJsonNode   snp_scale_limit_param(CJsonNode::NewObjectNode());
    snp_scale_limit_param.SetBoolean(kMandatory, false);
    snp_scale_limit_param.SetString(kType, "String");
    snp_scale_limit_param.SetString(kDescription,
        "GenBank ID2 SNP reader parameter");
    snp_scale_limit_param.SetString(kAllowedValues,
        "chromosome, contig, supercontig, unit and not provided");
    snp_scale_limit_param.SetString(kDefault, "");
    node.SetByKey("snp_scale_limit", snp_scale_limit_param);
}


// Requests descriptions

CJsonNode  GetIdGetblobRequestNode(void)
{
    CJsonNode   id_getblob(CJsonNode::NewObjectNode());
    id_getblob.SetString(kDescription,
        "Retrieves blob chunks basing on the blob identifier");

    CJsonNode   id_getblob_params(CJsonNode::NewObjectNode());
    AppendBlobIdParameter(id_getblob_params);
    AppendTseOptionParameter(id_getblob_params, "orig");
    AppendLastModifiedParameter(id_getblob_params);
    AppendUseCacheParameter(id_getblob_params);
    AppendIncludeHupParameter(id_getblob_params);
    AppendClientIdParameter(id_getblob_params);
    AppendSendBlobIfSmallParameter(id_getblob_params);
    AppendTraceParameter(id_getblob_params);
    AppendHopsParameter(id_getblob_params);
    AppendEnableProcessorParameter(id_getblob_params);
    AppendDisableProcessorParameter(id_getblob_params);
    AppendProcessorEventsParameter(id_getblob_params);
    id_getblob.SetByKey("parameters", id_getblob_params);

    CJsonNode   id_getblob_reply(CJsonNode::NewObjectNode());
    id_getblob_reply.SetString(kDescription,
        "The PSG protocol is used in the HTTP body. "
        "The blob properties and chunks are provided.");
    id_getblob.SetByKey("reply", id_getblob_reply);

    return id_getblob;
}


CJsonNode  GetIdGetRequestNode(void)
{
    CJsonNode   id_get(CJsonNode::NewObjectNode());
    id_get.SetString(kDescription,
        "Retrieves blob chunks basing on the seq_id and seq_id_type");
    CJsonNode   id_get_params(CJsonNode::NewObjectNode());

    AppendSeqIdParameter(id_get_params);
    AppendSeqIdTypeParameter(id_get_params);
    AppendUseCacheParameter(id_get_params);
    AppendTseOptionParameter(id_get_params, "orig");
    AppendExcludeBlobsParameter(id_get_params);
    AppendClientIdParameter(id_get_params);
    AppendAccSubstitutionParameter(id_get_params);
    AppendSendBlobIfSmallParameter(id_get_params);
    AppendResendTimeoutParameter(id_get_params);
    AppendSeqIdResolveParameter(id_get_params);
    AppendIncludeHupParameter(id_get_params);
    AppendTraceParameter(id_get_params);
    AppendHopsParameter(id_get_params);
    AppendEnableProcessorParameter(id_get_params);
    AppendDisableProcessorParameter(id_get_params);
    AppendProcessorEventsParameter(id_get_params);
    id_get.SetByKey("parameters", id_get_params);

    CJsonNode   id_get_reply(CJsonNode::NewObjectNode());
    id_get_reply.SetString(kDescription,
        "The PSG protocol is used in the HTTP body. "
        "The bioseq info, blob properties and chunks are provided.");
    id_get.SetByKey("reply", id_get_reply);
    return id_get;
}


CJsonNode  GetIdGetTseChunkRequestNode(void)
{
    CJsonNode   id_get_tse_chunk(CJsonNode::NewObjectNode());
    id_get_tse_chunk.SetString(kDescription,
        "Retrieves a TSE blob chunk");
    CJsonNode   id_get_tse_chunk_params(CJsonNode::NewObjectNode());

    AppendId2ChunkParameter(id_get_tse_chunk_params);
    AppendId2InfoParameter(id_get_tse_chunk_params);
    AppendUseCacheParameter(id_get_tse_chunk_params);
    AppendIncludeHupParameter(id_get_tse_chunk_params);
    AppendTraceParameter(id_get_tse_chunk_params);
    AppendHopsParameter(id_get_tse_chunk_params);
    AppendEnableProcessorParameter(id_get_tse_chunk_params);
    AppendDisableProcessorParameter(id_get_tse_chunk_params);
    AppendProcessorEventsParameter(id_get_tse_chunk_params);
    id_get_tse_chunk.SetByKey("parameters", id_get_tse_chunk_params);

    CJsonNode   id_get_tse_chunk_reply(CJsonNode::NewObjectNode());
    id_get_tse_chunk_reply.SetString(kDescription,
        "The PSG protocol is used in the HTTP body. "
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
    AppendSeqIdResolveParameter(id_resolve_params);
    AppendTraceParameter(id_resolve_params);
    AppendHopsParameter(id_resolve_params);
    AppendEnableProcessorParameter(id_resolve_params);
    AppendDisableProcessorParameter(id_resolve_params);
    AppendProcessorEventsParameter(id_resolve_params);
    id_resolve.SetByKey("parameters", id_resolve_params);

    CJsonNode   id_resolve_reply(CJsonNode::NewObjectNode());
    id_resolve_reply.SetString(kDescription,
        "The bioseq info is sent back in the HTTP body as binary protobuf "
        "or as PSG protocol chunks depending on the protocol choice");
    id_resolve.SetByKey("reply", id_resolve_reply);

    return id_resolve;
}


CJsonNode  GetIpgResolveRequestNode(void)
{
    CJsonNode   ipg_resolve(CJsonNode::NewObjectNode());
    ipg_resolve.SetString(kDescription,
        "Resolve nucleotide/protein/ipg and provide ipg info");
    CJsonNode   ipg_resolve_params(CJsonNode::NewObjectNode());

    AppendProteinParameter(ipg_resolve_params);
    AppendNucleotideParameter(ipg_resolve_params);
    AppendIpgParameter(ipg_resolve_params);
    AppendUseCacheParameter(ipg_resolve_params);
    AppendSeqIdResolveParameter(ipg_resolve_params);
    AppendTraceParameter(ipg_resolve_params);
    AppendEnableProcessorParameter(ipg_resolve_params);
    AppendDisableProcessorParameter(ipg_resolve_params);
    AppendProcessorEventsParameter(ipg_resolve_params);

    ipg_resolve.SetByKey("parameters", ipg_resolve_params);

    CJsonNode   ipg_resolve_reply(CJsonNode::NewObjectNode());
    ipg_resolve_reply.SetString(kDescription,
        "The ipg record(s) is sent baback in the HTTP body as PSG protocol "
        "chunks");
    ipg_resolve.SetByKey("reply", ipg_resolve_reply);

    return ipg_resolve;
}


CJsonNode  GetIdGetNaRequestNode(void)
{
    CJsonNode   id_get_na(CJsonNode::NewObjectNode());
    id_get_na.SetString(kDescription,
        "Retrieves named annotations");
    CJsonNode   id_get_na_params(CJsonNode::NewObjectNode());

    AppendSeqIdParameterForGetNA(id_get_na_params);
    AppendSeqIdsParameterForGetNA(id_get_na_params);
    AppendSeqIdTypeParameter(id_get_na_params);
    AppendNamesParameter(id_get_na_params);
    AppendUseCacheParameter(id_get_na_params);
    AppendFmtParameter(id_get_na_params);
    AppendTseOptionParameter(id_get_na_params, "none");
    AppendClientIdParameter(id_get_na_params);
    AppendSendBlobIfSmallParameter(id_get_na_params);
    AppendSeqIdResolveParameter(id_get_na_params);
    AppendResendTimeoutParameter(id_get_na_params);
    AppendSNPScaleLimitParameter(id_get_na_params);
    AppendTraceParameter(id_get_na_params);
    AppendHopsParameter(id_get_na_params);
    AppendEnableProcessorParameter(id_get_na_params);
    AppendDisableProcessorParameter(id_get_na_params);
    AppendProcessorEventsParameter(id_get_na_params);
    id_get_na.SetByKey("parameters", id_get_na_params);

    CJsonNode   id_get_na_reply(CJsonNode::NewObjectNode());
    id_get_na_reply.SetString(kDescription,
        "The PSG protocol is used in the HTTP body. "
        "The bioseq info and named annotation chunks are provided.");
    id_get_na.SetByKey("reply", id_get_na_reply);

    return id_get_na;
}


CJsonNode  GetIdAccessionVersionHistoryRequestNode(void)
{
    CJsonNode   id_acc_ver_hist(CJsonNode::NewObjectNode());
    id_acc_ver_hist.SetString(kDescription,
        "Retrieves accession version history");
    CJsonNode   id_acc_ver_hist_params(CJsonNode::NewObjectNode());

    AppendSeqIdParameter(id_acc_ver_hist_params);
    AppendSeqIdTypeParameter(id_acc_ver_hist_params);
    AppendUseCacheParameter(id_acc_ver_hist_params);
    AppendTraceParameter(id_acc_ver_hist_params);
    AppendHopsParameter(id_acc_ver_hist_params);
    AppendEnableProcessorParameter(id_acc_ver_hist_params);
    AppendDisableProcessorParameter(id_acc_ver_hist_params);
    AppendProcessorEventsParameter(id_acc_ver_hist_params);
    id_acc_ver_hist.SetByKey("parameters", id_acc_ver_hist_params);

    CJsonNode   id_acc_ver_hist_reply(CJsonNode::NewObjectNode());
    id_acc_ver_hist_reply.SetString(kDescription,
        "The PSG protocol is used in the HTTP body. "
        "The bioseq info and accession version history chunks are provided.");
    id_acc_ver_hist.SetByKey("reply", id_acc_ver_hist_reply);

    return id_acc_ver_hist;
}


CJsonNode  GetIdGetSatMappingRequestNode(void)
{
    CJsonNode   getsatmapping(CJsonNode::NewObjectNode());
    getsatmapping.SetString(kDescription,
        "Provides the cassandra sat to keyspace mapping information");

    getsatmapping.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   id_sat_mapping_reply(CJsonNode::NewObjectNode());
    id_sat_mapping_reply.SetString(kDescription,
        "The HTTP body is a JSON dictionary with "
        "the cassandra sat to keyspace mapping information.");
    getsatmapping.SetByKey("reply", id_sat_mapping_reply);

    return getsatmapping;
}


CJsonNode  GetAdminConfigRequestNode(void)
{
    CJsonNode   admin_config(CJsonNode::NewObjectNode());
    admin_config.SetString(kDescription,
        "Provides the server configuration information");

    admin_config.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_config_reply(CJsonNode::NewObjectNode());
    admin_config_reply.SetString(kDescription,
        "The HTTP body is a JSON dictionary with "
        "the configuration information. "
        "The request may be configured as protected in the server settings. "
        "If so then the server will use the value of the AdminAuthToken cookie "
        "to check permissions.");
    admin_config.SetByKey("reply", admin_config_reply);

    return admin_config;
}


CJsonNode  GetAdminInfoRequestNode(void)
{
    CJsonNode   admin_info(CJsonNode::NewObjectNode());
    admin_info.SetString(kDescription,
        "Provides the server run-time information");

    admin_info.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_info_reply(CJsonNode::NewObjectNode());
    admin_info_reply.SetString(kDescription,
        "The HTTP body is a JSON dictionary with "
        "the run-time information like resource consumption. "
        "The request may be configured as protected in the server settings. "
        "If so then the server will use the value of the AdminAuthToken cookie "
        "to check permissions.");
    admin_info.SetByKey("reply", admin_info_reply);

    return admin_info;
}


CJsonNode  GetAdminStatusRequestNode(void)
{
    CJsonNode   admin_status(CJsonNode::NewObjectNode());
    admin_status.SetString(kDescription,
        "Provides the server event counters");

    admin_status.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_status_reply(CJsonNode::NewObjectNode());
    admin_status_reply.SetString(kDescription,
        "The HTTP body is a JSON dictionary with "
        "various event counters. "
        "The request may be configured as protected in the server settings. "
        "If so then the server will use the value of the AdminAuthToken cookie "
        "to check permissions.");
    admin_status.SetByKey("reply", admin_status_reply);

    return admin_status;
}


CJsonNode  GetAdminShutdownRequestNode(void)
{
    CJsonNode   admin_shutdown(CJsonNode::NewObjectNode());
    admin_shutdown.SetString(kDescription,
        "Performs the server shutdown");
    CJsonNode   admin_shutdown_params(CJsonNode::NewObjectNode());

    AppendUsernameParameter(admin_shutdown_params);
    AppendAuthTokenParameter(admin_shutdown_params);
    AppendTimeoutParameter(admin_shutdown_params);
    admin_shutdown.SetByKey("parameters", admin_shutdown_params);

    CJsonNode   admin_status_reply(CJsonNode::NewObjectNode());
    admin_status_reply.SetString(kDescription,
        "The standard HTTP protocol is used. "
        "The request may be configured as protected in the server settings. "
        "If so then the server will use the value of the AdminAuthToken cookie "
        "to check permissions.");
    admin_shutdown.SetByKey("reply", admin_status_reply);

    return admin_shutdown;
}


CJsonNode  GetAdminGetAlertsRequestNode(void)
{
    CJsonNode   admin_get_alerts(CJsonNode::NewObjectNode());
    admin_get_alerts.SetString(kDescription,
        "Provides the server alerts");

    admin_get_alerts.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_get_alerts_reply(CJsonNode::NewObjectNode());
    admin_get_alerts_reply.SetString(kDescription,
        "The HTTP body is a JSON dictionary with "
        "the current server alerts. "
        "The request may be configured as protected in the server settings. "
        "If so then the server will use the value of the AdminAuthToken cookie "
        "to check permissions.");
    admin_get_alerts.SetByKey("reply", admin_get_alerts_reply);

    return admin_get_alerts;
}


CJsonNode  GetAdminAckAlertsRequestNode(void)
{
    CJsonNode   admin_ack_alert(CJsonNode::NewObjectNode());
    admin_ack_alert.SetString(kDescription,
        "Acknowledge an alert");
    CJsonNode   admin_ack_alert_params(CJsonNode::NewObjectNode());

    AppendAlertParameter(admin_ack_alert_params);
    AppendAckAlertUsernameParameter(admin_ack_alert_params);
    admin_ack_alert.SetByKey("parameters", admin_ack_alert_params);

    CJsonNode   admin_ack_alerts_reply(CJsonNode::NewObjectNode());
    admin_ack_alerts_reply.SetString(kDescription,
        "The standard HTTP protocol is used. "
        "The request may be configured as protected in the server settings. "
        "If so then the server will use the value of the AdminAuthToken cookie "
        "to check permissions.");
    admin_ack_alert.SetByKey("reply", admin_ack_alerts_reply);

    return admin_ack_alert;
}


CJsonNode  GetAdminStatisticsRequestNode(void)
{
    CJsonNode   admin_statistics(CJsonNode::NewObjectNode());
    admin_statistics.SetString(kDescription,
        "Provides the server collected statistics");
    CJsonNode   admin_statistics_params(CJsonNode::NewObjectNode());

    AppendResetParameter(admin_statistics_params);
    AppendMostRecentTimeParameter(admin_statistics_params);
    AppendMostAncientTimeParameter(admin_statistics_params);
    AppendHistogramNamesParameter(admin_statistics_params);
    AppendTimeSeriesParameter(admin_statistics_params);
    admin_statistics.SetByKey("parameters", admin_statistics_params);

    CJsonNode   admin_statistics_reply(CJsonNode::NewObjectNode());
    admin_statistics_reply.SetString(kDescription,
        "The HTTP body is a JSON dictionary with "
        "the collected statistics information. "
        "The request may be configured as protected in the server settings. "
        "If so then the server will use the value of the AdminAuthToken cookie "
        "to check permissions.");
    admin_statistics.SetByKey("reply", admin_statistics_reply);

    return admin_statistics;
}


CJsonNode GetAdminDispatcherStatusRequestNode(void)
{
    CJsonNode   admin_dispatcher_status(CJsonNode::NewObjectNode());
    admin_dispatcher_status.SetString(kDescription,
        "Provides the request dispatcher status");

    admin_dispatcher_status.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_dispatcher_status_reply(CJsonNode::NewObjectNode());
    admin_dispatcher_status_reply.SetString(kDescription,
        "The HTTP body is a JSON dictionary with "
        "the current request dispatcher status.");
    admin_dispatcher_status.SetByKey("reply", admin_dispatcher_status_reply);

    return admin_dispatcher_status;
}


CJsonNode GetAdminConnectionsStatusRequestNode(void)
{
    CJsonNode   admin_connections_status(CJsonNode::NewObjectNode());
    admin_connections_status.SetString(kDescription,
        "Provides the connections status");

    admin_connections_status.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   admin_connections_status_reply(CJsonNode::NewObjectNode());
    admin_connections_status_reply.SetString(kDescription,
        "The HTTP body is a JSON dictionary with "
        "the current connections status.");
    admin_connections_status.SetByKey("reply", admin_connections_status_reply);

    return admin_connections_status;
}


// /healthz
CJsonNode  GetHealthzRequestNode(void)
{
    CJsonNode   healthz(CJsonNode::NewObjectNode());
    healthz.SetString(kDescription,
        "Performs a functionality check. It can be used by monitoring facilities.");
    CJsonNode   healthz_params(CJsonNode::NewObjectNode());

    AppendZVerboseParameter(healthz_params);
    AppendZExcludeParameter(healthz_params);
    healthz.SetByKey("parameters", healthz_params);

    CJsonNode   healthz_reply(CJsonNode::NewObjectNode());
    healthz_reply.SetString(kDescription,
        "HTTP status is set to 200 if the functionality check succeeded. "
        "Otherwise the HTTP status is set to 500. If verbose flag is provided "
        "then the HTTP body contains a JSON dictionary with detailed information.");
    healthz.SetByKey("reply", healthz_reply);

    return healthz;
}


// /livez
CJsonNode  GetLivezRequestNone(void)
{
    CJsonNode   livez(CJsonNode::NewObjectNode());
    livez.SetString(kDescription,
        "Performs a live check. It can be used by monitoring facilities.");
    CJsonNode   livez_params(CJsonNode::NewObjectNode());

    AppendZVerboseParameter(livez_params);
    livez.SetByKey("parameters", livez_params);

    CJsonNode   livez_reply(CJsonNode::NewObjectNode());
    livez_reply.SetString(kDescription,
        "HTTP status is set to 200 if the live check succeeded. "
        "Otherwise the HTTP status is set to 500. If verbose flag is provided "
        "then the HTTP body contains a JSON dictionary with detailed information.");
    livez.SetByKey("reply", livez_reply);

    return livez;
}


// /readyz
CJsonNode  GetReadyzRequestNode(void)
{
    CJsonNode   readyz(CJsonNode::NewObjectNode());
    readyz.SetString(kDescription,
        "Performs a functionality check. It can be used by monitoring facilities.");
    CJsonNode   readyz_params(CJsonNode::NewObjectNode());

    AppendZVerboseParameter(readyz_params);
    AppendZExcludeParameter(readyz_params);
    readyz.SetByKey("parameters", readyz_params);

    CJsonNode   readyz_reply(CJsonNode::NewObjectNode());
    readyz_reply.SetString(kDescription,
        "HTTP status is set to 200 if the functionality check succeeded. "
        "Otherwise the HTTP status is set to 500. If verbose flag is provided "
        "then the HTTP body contains a JSON dictionary with detailed information.");
    readyz.SetByKey("reply", readyz_reply);

    return readyz;
}


// /readyz/cassandra
CJsonNode  GetReadyzCassandraRequestNode(void)
{
    CJsonNode   readyzcassandra(CJsonNode::NewObjectNode());
    readyzcassandra.SetString(kDescription,
        "Performs a cassandra retrieval check. It can be used by monitoring facilities.");
    CJsonNode   readyzcassandra_params(CJsonNode::NewObjectNode());

    AppendZVerboseParameter(readyzcassandra_params);
    readyzcassandra.SetByKey("parameters", readyzcassandra_params);

    CJsonNode   readyzcassandra_reply(CJsonNode::NewObjectNode());
    readyzcassandra_reply.SetString(kDescription,
        "HTTP status is set to 200 if cassandra retrieval is ok. "
        "Otherwise the HTTP status is set to 500. If verbose flag is provided "
        "then the HTTP body contains a JSON dictionary with detailed information.");
    readyzcassandra.SetByKey("reply", readyzcassandra_reply);

    return readyzcassandra;
}


// /readyz/connections
CJsonNode GetReadyzConnectionsRequestNode(void)
{
    CJsonNode   readyzconnections(CJsonNode::NewObjectNode());
    readyzconnections.SetString(kDescription,
        "Checks that the current number of the client connections is within the soft limit. It can be used by monitoring facilities.");
    CJsonNode   readyzconnections_params(CJsonNode::NewObjectNode());

    AppendZVerboseParameter(readyzconnections_params);
    readyzconnections.SetByKey("parameters", readyzconnections_params);

    CJsonNode   readyzconnections_reply(CJsonNode::NewObjectNode());
    readyzconnections_reply.SetString(kDescription,
        "HTTP status is set to 200 if the current number of the client connections is within the soft limit. "
        "Otherwise the HTTP status is set to 503. If verbose flag is provided "
        "then the HTTP body contains a JSON dictionary with detailed information.");
    readyzconnections.SetByKey("reply", readyzconnections_reply);

    return readyzconnections;
}


// /readyz/lmdb
CJsonNode  GetReadyzLMDBRequestNode(void)
{
    CJsonNode   readyzlmdbresolve(CJsonNode::NewObjectNode());
    readyzlmdbresolve.SetString(kDescription,
        "Performs an LMDB retrieval check. It can be used by monitoring facilities.");
    CJsonNode   readyzlmdbresolve_params(CJsonNode::NewObjectNode());

    AppendZVerboseParameter(readyzlmdbresolve_params);
    readyzlmdbresolve.SetByKey("parameters", readyzlmdbresolve_params);

    CJsonNode   readyzlmdbresolve_reply(CJsonNode::NewObjectNode());
    readyzlmdbresolve_reply.SetString(kDescription,
        "HTTP status is set to 200 if LMDB retrieval check succeeded. "
        "Otherwise the HTTP status is set to 500. If verbose flag is provided "
        "then the HTTP body contains a JSON dictionary with detailed information.");
    readyzlmdbresolve.SetByKey("reply", readyzlmdbresolve_reply);

    return readyzlmdbresolve;
}


// /readyz/wgs
CJsonNode  GetReadyzWGSRequestNode(void)
{
    CJsonNode   readyzwgsresolve(CJsonNode::NewObjectNode());
    readyzwgsresolve.SetString(kDescription,
        "Performs a WGS retrieval check. It can be used by monitoring facilities.");
    CJsonNode   readyzwgsresolve_params(CJsonNode::NewObjectNode());

    AppendZVerboseParameter(readyzwgsresolve_params);
    readyzwgsresolve.SetByKey("parameters", readyzwgsresolve_params);

    CJsonNode   readyzwgsresolve_reply(CJsonNode::NewObjectNode());
    readyzwgsresolve_reply.SetString(kDescription,
        "HTTP status is set to 200 if WGS retrieval check succeeded. "
        "Otherwise the HTTP status is set to 500. If verbose flag is provided "
        "then the HTTP body contains a JSON dictionary with detailed information.");
    readyzwgsresolve.SetByKey("reply", readyzwgsresolve_reply);

    return readyzwgsresolve;
}


// /readyz/cdd
CJsonNode  GetReadyzCDDRequestNode(void)
{
    CJsonNode   readyzcddresolve(CJsonNode::NewObjectNode());
    readyzcddresolve.SetString(kDescription,
        "Performs a CDD retrieval check. It can be used by monitoring facilities.");
    CJsonNode   readyzcddresolve_params(CJsonNode::NewObjectNode());

    AppendZVerboseParameter(readyzcddresolve_params);
    readyzcddresolve.SetByKey("parameters", readyzcddresolve_params);

    CJsonNode   readyzcddresolve_reply(CJsonNode::NewObjectNode());
    readyzcddresolve_reply.SetString(kDescription,
        "HTTP status is set to 200 if CDD retrieval check succeeded. "
        "Otherwise the HTTP status is set to 500. If verbose flag is provided "
        "then the HTTP body contains a JSON dictionary with detailed information.");
    readyzcddresolve.SetByKey("reply", readyzcddresolve_reply);

    return readyzcddresolve;
}


// /readyz/snp
CJsonNode  GetReadyzSNPRequestNode(void)
{
    CJsonNode   readyzsnpresolve(CJsonNode::NewObjectNode());
    readyzsnpresolve.SetString(kDescription,
        "Performs a SNP retrieval check. It can be used by monitoring facilities.");
    CJsonNode   readyzsnpresolve_params(CJsonNode::NewObjectNode());

    AppendZVerboseParameter(readyzsnpresolve_params);
    readyzsnpresolve.SetByKey("parameters", readyzsnpresolve_params);

    CJsonNode   readyzsnpresolve_reply(CJsonNode::NewObjectNode());
    readyzsnpresolve_reply.SetString(kDescription,
        "HTTP status is set to 200 if SNP retrieval check succeeded. "
        "Otherwise the HTTP status is set to 500. If verbose flag is provided "
        "then the HTTP body contains a JSON dictionary with detailed information.");
    readyzsnpresolve.SetByKey("reply", readyzsnpresolve_reply);

    return readyzsnpresolve;
}


CJsonNode  GetTestIoRequestNode(void)
{
    CJsonNode   test_io(CJsonNode::NewObjectNode());
    test_io.SetString(kDescription,
        "Sends back random binary data to test the I/O performance. It works "
        "only if the server configuration file has the [DEBUG]/psg_allow_io_test "
        "value set to true");
    CJsonNode   test_io_params(CJsonNode::NewObjectNode());

    AppendReturnDataSizeParameter(test_io_params);
    AppendLogParameter(test_io_params);
    test_io.SetByKey("parameters", test_io_params);

    CJsonNode   test_io_reply(CJsonNode::NewObjectNode());
    test_io_reply.SetString(kDescription,
        "The HTTP body is a random data of the requested length");
    test_io.SetByKey("reply", test_io_reply);

    return test_io;
}


CJsonNode  GetHealthRequestNode(void)
{
    CJsonNode   health(CJsonNode::NewObjectNode());
    health.SetString(kDescription,
        "Performs a basic functionality check. It can be used by monitoring facilities.");

    health.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   health_reply(CJsonNode::NewObjectNode());
    health_reply.SetString(kDescription,
        "HTTP status is set to 200 if the functionality check succeeded. "
        "Otherwise the HTTP status is set to 500 and the HTTP body "
        "may have a detailed message.");
    health.SetByKey("reply", health_reply);
    return health;
}


CJsonNode  GetDeepHealthRequestNode(void)
{
    CJsonNode   health(CJsonNode::NewObjectNode());
    health.SetString(kDescription,
        "At the moment the implementation matches the 'health' command.");

    health.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   health_reply(CJsonNode::NewObjectNode());
    health_reply.SetString(kDescription,
        "HTTP status is set to 200 if the functionality check succeeded. "
        "Otherwise the HTTP status is set to 500 and the HTTP body "
        "may have a detailed message.");
    health.SetByKey("reply", health_reply);
    return health;
}


CJsonNode  GetFaviconRequestNode(void)
{
    CJsonNode   favicon(CJsonNode::NewObjectNode());
    favicon.SetString(kDescription,
        "Always replies 'not found'");

    favicon.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   favicon_reply(CJsonNode::NewObjectNode());
    favicon_reply.SetString(kDescription,
        "The standard HTTP protocol is used");
    favicon.SetByKey("reply", favicon_reply);

    return favicon;
}


CJsonNode  GetUnknownRequestNode(void)
{
    CJsonNode   unknown(CJsonNode::NewObjectNode());
    unknown.SetString(kDescription,
        "Always replies 'ok' in terms of http. The nested PSG protocol always "
        "tells 'bad request'");

    unknown.SetByKey("parameters", CJsonNode::NewObjectNode());

    CJsonNode   unknown_reply(CJsonNode::NewObjectNode());
    unknown_reply.SetString(kDescription,
        "The HTTP body uses PSG protocol for a 'bad request' message");
    unknown.SetByKey("reply", unknown_reply);
    return unknown;
}


CJsonNode   GetAboutNode(void)
{
    CJsonNode   about_node(CJsonNode::NewObjectNode());

    about_node.SetString("name", "PubSeq Gateway");
    about_node.SetString(kDescription,
        "Accession resolution and retrieval of bio-sequences. Retrieval of named annotations.");
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
    requests_node.SetByKey("ID/get_acc_ver_history", GetIdAccessionVersionHistoryRequestNode());
    requests_node.SetByKey("ID/get_sat_mapping", GetIdGetSatMappingRequestNode());
    requests_node.SetByKey("IPG/resolve", GetIpgResolveRequestNode());
    requests_node.SetByKey("ADMIN/config", GetAdminConfigRequestNode());
    requests_node.SetByKey("ADMIN/info", GetAdminInfoRequestNode());
    requests_node.SetByKey("ADMIN/status", GetAdminStatusRequestNode());
    requests_node.SetByKey("ADMIN/shutdown", GetAdminShutdownRequestNode());
    requests_node.SetByKey("ADMIN/get_alerts", GetAdminGetAlertsRequestNode());
    requests_node.SetByKey("ADMIN/ack_alerts", GetAdminAckAlertsRequestNode());
    requests_node.SetByKey("ADMIN/statistics", GetAdminStatisticsRequestNode());
    requests_node.SetByKey("ADMIN/dispatcher_status", GetAdminDispatcherStatusRequestNode());
    requests_node.SetByKey("ADMIN/connections_status", GetAdminConnectionsStatusRequestNode());

    requests_node.SetByKey("healthz", GetHealthzRequestNode());
    requests_node.SetByKey("livez", GetLivezRequestNone());
    requests_node.SetByKey("readyz", GetReadyzRequestNode());
    requests_node.SetByKey("/readyz/cassandra", GetReadyzCassandraRequestNode());
    requests_node.SetByKey("/readyz/connections", GetReadyzConnectionsRequestNode());
    requests_node.SetByKey("/readyz/lmdb", GetReadyzLMDBRequestNode());
    requests_node.SetByKey("/readyz/wgs", GetReadyzWGSRequestNode());
    requests_node.SetByKey("/readyz/cdd", GetReadyzCDDRequestNode());
    requests_node.SetByKey("/readyz/snp", GetReadyzSNPRequestNode());

    requests_node.SetByKey("TEST/io", GetTestIoRequestNode());
    requests_node.SetByKey("health", GetHealthRequestNode());
    requests_node.SetByKey("deep-health", GetDeepHealthRequestNode());
    requests_node.SetByKey("favicon.ico", GetFaviconRequestNode());
    requests_node.SetByKey("unknown", GetUnknownRequestNode());
    return requests_node;
}


CJsonNode   GetReferencesNode(void)
{
    CJsonNode   doc_link(CJsonNode::NewObjectNode());

    doc_link.SetString(kDescription,
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
    introspection.SetByKey("references", GetReferencesNode());
    introspection.SetByKey("requests", GetRequestsNode());
    return introspection;
}

