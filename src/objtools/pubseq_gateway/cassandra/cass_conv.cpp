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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  Wrapper class around cassandra "C"-API
 *
 */

#include <ncbi_pch.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/cass_conv.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_exception.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_util.hpp>

#include <atomic>
#include <memory>
#include <sstream>
#include <set>
#include <vector>
#include <string>
#include <limits>
#include <utility>

#include "corelib/ncbitime.hpp"
#include "corelib/ncbistr.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

namespace Convert {

template<>
void CassValueConvert<bool>(const CassValue * Val, bool& v)
{
    if (!Val)
        RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to bool: Cass value is NULL");

    CassError err;
    CassValueType type = cass_value_type(Val);
    cass_bool_t cb = cass_false;
    v = false;
    switch (type) {
        case CASS_VALUE_TYPE_BOOLEAN:
            if ((err = cass_value_get_bool(Val, &cb)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to bool:");
            v = cb;
            break;
        default:
            if (type == CASS_VALUE_TYPE_UNKNOWN && cass_value_is_null(Val))
                return;
            RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to bool: unsupported data type (Cass type - " + NStr::NumericToString(static_cast<int>(type)) + ")" );
    }
}

template<>
void CassValueConvert<int8_t>(const CassValue * Val, int8_t& v)
{
    if (!Val)
        RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int16_t: Cass value is NULL");

    v = 0;
    CassError err;
    CassValueType type = cass_value_type(Val);
    switch (type) {
        case CASS_VALUE_TYPE_TINY_INT:
            if ((err = cass_value_get_int8(Val, &v)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int8_t:");
            break;
        default:
            if (type == CASS_VALUE_TYPE_UNKNOWN && cass_value_is_null(Val))
                return;
            RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int8_t: unsupported data type (Cass type - " + NStr::NumericToString(static_cast<int>(type)) + ")");
    }
}

template<>
void CassValueConvert<int16_t>(const CassValue * Val, int16_t& v)
{
    if (!Val)
        RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int16_t: Cass value is NULL");

    v = 0;
    CassError err;
    CassValueType type = cass_value_type(Val);
    switch (type) {
        case CASS_VALUE_TYPE_TINY_INT: {
            int8_t rv8 = 0;
            if ((err = cass_value_get_int8(Val, &rv8)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int16_t:");
            v = rv8;
            break;
        }
        case CASS_VALUE_TYPE_SMALL_INT:
            if ((err = cass_value_get_int16(Val, &v)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int16_t:");
            break;
        default:
            if (type == CASS_VALUE_TYPE_UNKNOWN && cass_value_is_null(Val))
                return;
            RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int16_t: unsupported data type (Cass type - " + NStr::NumericToString(static_cast<int>(type)) + ")" );
    }
}

template<>
void CassValueConvert<int32_t>(const CassValue *  Val, int32_t& v)
{
    if (!Val)
        RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int32_t: Cass value is NULL");

    v = 0;
    CassError err;
    CassValueType type = cass_value_type(Val);
    switch (type) {
        case CASS_VALUE_TYPE_TIMESTAMP:
        case CASS_VALUE_TYPE_BIGINT:
        case CASS_VALUE_TYPE_COUNTER: {
            int64_t rv64 = 0;
            if ((err = cass_value_get_int64(Val, &rv64)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int32_t:");
            if (rv64 < INT_MIN || rv64 > INT_MAX)
                RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int32_t: Fetched value overflow");
            v = rv64;
            break;
        }
        case CASS_VALUE_TYPE_TINY_INT: {
            int8_t rv8 = 0;
            if ((err = cass_value_get_int8(Val, &rv8)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int32_t:");
            v = rv8;
            break;
        }
        case CASS_VALUE_TYPE_SMALL_INT: {
            int16_t rv16 = 0;
            if ((err = cass_value_get_int16(Val, &rv16)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int32_t:");
            v = rv16;
            break;
        }
        case CASS_VALUE_TYPE_INT:
            if ((err = cass_value_get_int32(Val, &v)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int32_t:");
            break;
        default:
            if (type == CASS_VALUE_TYPE_UNKNOWN && cass_value_is_null(Val))
                return;
            RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int32_t: unsupported data type (Cass type - " + NStr::NumericToString(static_cast<int>(type)) + ")" );
    }
}

template<>
void CassValueConvert<int64_t>(const CassValue *  Val, int64_t& v)
{
    if (!Val)
        RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int64_t: Cass value is NULL");

    CassError           err;
    v = 0;
    CassValueType       type = cass_value_type(Val);
    switch (type) {
        case CASS_VALUE_TYPE_TIMESTAMP:
        case CASS_VALUE_TYPE_BIGINT:
        case CASS_VALUE_TYPE_COUNTER:
            if ((err = cass_value_get_int64(Val, &v)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int64_t:");
            break;
        case CASS_VALUE_TYPE_INT: {
            int32_t rv32 = 0;
            if ((err = cass_value_get_int32(Val, &rv32)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int64_t:");
            v = rv32;
            break;
        }
        case CASS_VALUE_TYPE_TINY_INT: {
            int8_t rv8 = 0;
            if ((err = cass_value_get_int8(Val, &rv8)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int64_t:");
            v = rv8;
            break;
        }
        case CASS_VALUE_TYPE_SMALL_INT: {
            int16_t rv16 = 0;
            if ((err = cass_value_get_int16(Val, &rv16)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to int64_t:");
            v = rv16;
            break;
        }
        default:
            if (type == CASS_VALUE_TYPE_UNKNOWN && cass_value_is_null(Val))
                return;
            RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int64_t: unsupported data type (Cass type - " + NStr::NumericToString(static_cast<int>(type)) + ")" );
    }
}

template<>
void CassValueConvert<double>(const CassValue *  Val, double& v)
{
    if (!Val)
        RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to double: Cass value is NULL");

    CassError err;
    cass_double_t rv = 0;
    if ((err = cass_value_get_double(Val, &rv)) != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to double:");
    v = rv;
}

template<>
void CassValueConvert<string>(const CassValue *  Val, string& v)
{
    if (!Val)
        RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to string: Cass value is NULL");

    CassError err;
    v.clear();
    CassValueType type = cass_value_type(Val);
    switch (type) {
        case CASS_VALUE_TYPE_BOOLEAN:
        case CASS_VALUE_TYPE_ASCII:
        case CASS_VALUE_TYPE_BLOB:
        case CASS_VALUE_TYPE_TEXT:
        case CASS_VALUE_TYPE_VARCHAR:
        case CASS_VALUE_TYPE_CUSTOM: {
            const char * rv = nullptr;
            size_t len = 0;
            if ((err = cass_value_get_string(Val, &rv, &len)) != CASS_OK)
                RAISE_CASS_ERROR(err, eConvFailed, "Failed to convert Cass value to string:");
            v.assign(rv, len);
            break;
        }
        case CASS_VALUE_TYPE_TIMESTAMP: {
            int64_t rv = 0;
            CassValueConvert<int64_t>(Val, rv);
            CTime c;
            TimeTmsToCTime(rv, &c);
            v = c;
            break;
        }
        case CASS_VALUE_TYPE_TIMEUUID:
        case CASS_VALUE_TYPE_UUID: {
            CassUuid fetched_value;
            char fetched_buffer[CASS_UUID_STRING_LENGTH];
            cass_value_get_uuid(Val, &fetched_value);
            cass_uuid_string(fetched_value, fetched_buffer);
            v = string(fetched_buffer);
            break;
        }
        default:
            if (type == CASS_VALUE_TYPE_UNKNOWN && cass_value_is_null(Val))
                return;
            RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to int64_t: unsupported data type (Cass type - " + NStr::NumericToString(static_cast<int>(type)) + ")" );
    }
}

template<>
void ValueToCassCollection<bool>(const bool& v, CassCollection* coll)
{
    CassError err;
    cass_bool_t b = v ? cass_true : cass_false;
    if ((err = cass_collection_append_bool(coll, b)) != CASS_OK) {
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append bool value to Cass collection:");
    }
}

template<>
void ValueToCassCollection<int8_t>(const int8_t& v, CassCollection* coll)
{
    CassError err;
    if ((err = cass_collection_append_int8(coll, v)) != CASS_OK) {
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append int8_t value to Cass collection:");
    }
}

template<>
void ValueToCassCollection<int16_t>(const int16_t& v, CassCollection* coll)
{
    CassError err;
    if ((err = cass_collection_append_int16(coll, v)) != CASS_OK) {
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append int16_t value to Cass collection:");
    }
}

template<>
void ValueToCassCollection<int32_t>(const int32_t& v, CassCollection* coll)
{
    CassError err;
    if ((err = cass_collection_append_int32(coll, v)) != CASS_OK) {
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append int32_t value to Cass collection:");
    }
}

template<>
void ValueToCassCollection<int64_t>(const int64_t& v, CassCollection* coll)
{
    CassError err;
    if ((err = cass_collection_append_int64(coll, v)) != CASS_OK) {
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append int64_t value to Cass collection:");
    }
}

template<>
void ValueToCassCollection<double>(const double& v, CassCollection* coll)
{
    CassError err;
    if ((err = cass_collection_append_double(coll, v)) != CASS_OK) {
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append double value to Cass collection:");
    }
}

template<>
void ValueToCassCollection<float>(const float& v, CassCollection* coll)
{
    CassError err;
    if ((err = cass_collection_append_float(coll, v)) != CASS_OK) {
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append float value to Cass collection:");
    }
}

template<>
void ValueToCassCollection<string>(const string& v, CassCollection* coll)
{
    CassError err;
    if ((err = cass_collection_append_string(coll, v.c_str())) != CASS_OK) {
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append string value to Cass collection:");
    }
}

template<>
void ValueToCassTupleItem<bool>(const bool& value, CassTuple* dest, int index)
{
    CassError err = cass_tuple_set_bool(dest, index, value ? cass_true : cass_false);
    if (err != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append bool value to Cass tuple item:");
}

template<>
void ValueToCassTupleItem<int8_t>(const int8_t& value, CassTuple* dest, int index)
{
    CassError err = cass_tuple_set_int8(dest, index, value);
    if (err != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append int8_t value to Cass tuple item:");
}

template<>
void ValueToCassTupleItem<int16_t>(const int16_t& value, CassTuple* dest, int index)
{
    CassError err = cass_tuple_set_int16(dest, index, value);
    if (err != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append int16_t value to Cass tuple item:");
}

template<>
void ValueToCassTupleItem<int32_t>(const int32_t& value, CassTuple* dest, int index)
{
    CassError err = cass_tuple_set_int32(dest, index, value);
    if (err != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append int32_t value to Cass tuple item:");
}

template<>
void ValueToCassTupleItem<int64_t>(const int64_t& value, CassTuple* dest, int index)
{
    CassError err = cass_tuple_set_int64(dest, index, value);
    if (err != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append int64_t value to Cass tuple item:");
}

template<>
void ValueToCassTupleItem<double>(const double& value, CassTuple* dest, int index)
{
    CassError err = cass_tuple_set_float(dest, index, value);
    if (err != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append double value to Cass tuple item:");
}

template<>
void ValueToCassTupleItem<float>(const float& value, CassTuple* dest, int index)
{
    CassError err = cass_tuple_set_float(dest, index, value);
    if (err != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append float value to Cass tuple item:");
}

template<>
void ValueToCassTupleItem<string>(const string& value, CassTuple* dest, int index)
{
    CassError err = cass_tuple_set_string_n(dest, index, value.c_str(), value.size());
    if (err != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append string value to Cass tuple item:");
}


}

END_IDBLOB_SCOPE
