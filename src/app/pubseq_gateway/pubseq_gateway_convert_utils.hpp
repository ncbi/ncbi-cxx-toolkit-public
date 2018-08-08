#ifndef PUBSEQ_GATEWAY_CONVERT_UTILS__HPP
#define PUBSEQ_GATEWAY_CONVERT_UTILS__HPP

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
 * File Description: Various format conversion utilities
 *
 *
 */

#include <connect/services/json_over_uttp.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/blob_record.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/bioseq_info.hpp>

#include "pubseq_gateway_types.hpp"

USING_NCBI_SCOPE;
USING_IDBLOB_SCOPE;

#include <string>
using namespace std;


void ConvertBioseqInfoToBioseqProtobuf(const SBioseqInfo &  bioseq_info,
                                       string &  bioseq_protobuf);
void ConvertBioseqProtobufToBioseqInfo(const string &  bioseq_protobuf,
                                       SBioseqInfo &  bioseq_info);
void ConvertBioseqInfoToJson(const SBioseqInfo &  bioseq_info,
                             TServIncludeData  include_data_flags,
                             string &  bioseq_json);
void ConvertSi2csiProtobufToBioseqProtobuf(const string &  si2csi_protobuf,
                                           string &  bioseq_protobuf);
void ConvertSi2sciToBioseqProtobuf(const SBioseqInfo &  bioseq_info,
                                   string &  bioseq_protobuf);
void ConvertSi2csiProtobufToBioseqJson(const string &  si2csi_protobuf,
                                       string &  bioseq_json);
void ConvertSi2csiToBioseqJson(const SBioseqInfo &  bioseq_info,
                               string &  bioseq_json);

CJsonNode ConvertBioseqInfoToJson(const SBioseqInfo &  bioseq_info,
                                  TServIncludeData  include_data_flags);
CJsonNode ConvertBlobPropToJson(const CBlobRecord &  blob);

#endif
