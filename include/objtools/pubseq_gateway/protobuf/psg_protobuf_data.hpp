#ifndef PSG_PROTOBUF_DATA_HPP
#define PSG_PROTOBUF_DATA_HPP

#include <objtools/pubseq_gateway/protobuf/psg_protobuf.pb.h>

#include <corelib/ncbistd.hpp>

BEGIN_NCBI_SCOPE

class CPubseqGatewayData
{
public:
    static bool UnpackBioseqInfo(const string& accession, int version, int seq_id_type, const string& data, psg::retrieval::bioseq_info& result) {
        return result.mutable_value()->ParseFromString(data);
    }
    static bool UnpackBioseqInfo(const string& data, psg::retrieval::bioseq_info_value& result) {
        return result.ParseFromString(data);
    }
    static bool PackBioseqInfo(const psg::retrieval::bioseq_info_value& value, string& data) {
        return value.SerializeToString(&data);
    }
    static bool PackBioseqInfo(const psg::retrieval::bioseq_info& value, string& data) {
        return value.SerializeToString(&data);
    }


    static bool UnpackSiInfo(const string& data, psg::retrieval::si2csi_value& result) {
        return result.ParseFromString(data);
    }
    static bool PackSiInfo(const psg::retrieval::si2csi_value& info, string& result) {
        return info.SerializeToString(&result);
    }

    static bool UnpackBlobPropInfo(const string& data, psg::retrieval::blob_prop_value& result) {
        return result.ParseFromString(data);
    }
    static bool PackBlobPropInfo(const psg::retrieval::blob_prop_value& value, string& result) {
        return value.SerializeToString(&result);
    }
};

END_NCBI_SCOPE

#endif
