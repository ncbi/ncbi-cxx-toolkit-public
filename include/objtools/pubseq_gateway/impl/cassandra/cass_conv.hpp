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
 *  Wrapper classes around cassandra "C"-API
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASS_CONV_HPP_
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASS_CONV_HPP_

#include <cassandra.h>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbiexpt.hpp>
#include <corelib/ncbitime.hpp>

#include "IdCassScope.hpp"
#include "cass_exception.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

namespace Convert {

template <typename T>
void CassValueConvert(const CassValue* Val, T& t)
{
    static_assert(sizeof(T) == 0, "CassValueConvert(): Conversion for this type is not implemented yet");
}

template<> void CassValueConvert<bool>(const CassValue * Val, bool& v);
template<> void CassValueConvert<int8_t>(const CassValue * Val, int8_t& v);
template<> void CassValueConvert<int16_t>(const CassValue * Val, int16_t& v);
template<> void CassValueConvert<int32_t>(const CassValue *  Val, int32_t& v);
template<> void CassValueConvert<int64_t>(const CassValue *  Val, int64_t& v);
template<> void CassValueConvert<double>(const CassValue *  Val, double& v);
template<> void CassValueConvert<string>(const CassValue *  Val, string& v);


template<size_t C, size_t N, typename ...T>
void CassIteratorToTupleImpl(CassIterator*, std::tuple<T...>&, char)
{
}

template<size_t C, size_t N, typename ...T>
void CassIteratorToTupleImpl(CassIterator* itr, std::tuple<T...>& t, int64_t)
{
    if (!cass_iterator_next(itr))
        RAISE_DB_ERROR(eConvFailed, "Cass tuple iterator next failed");
    const CassValue* value = cass_iterator_get_value(itr);
    if (!value)
        RAISE_DB_ERROR(eConvFailed, "Cass tuple iterator fetch failed");

    CassValueConvert(value, std::get<C, T...>(t));

    using TSwitch = typename conditional< C + 1 == N, char, int64_t>::type;
    CassIteratorToTupleImpl<C + 1, N, T...>(itr, t, TSwitch());
}

template<typename ...T>
void CassValueConvert(const CassValue* Val, std::tuple<T...>& t) {
    if (!Val)
        RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to tuple: Cass value is NULL");

    CassValueType type = cass_value_type(Val);
    switch (type) {
        case CASS_VALUE_TYPE_TUPLE:
            if (!cass_value_is_null(Val)) {
                unique_ptr<CassIterator, function<void(CassIterator*)> > tuple_iterator(
                    cass_iterator_from_tuple(Val), 
                    [](CassIterator* itr) {
                        if (itr)
                            cass_iterator_free(itr);
                    }
                );
                if (!tuple_iterator)
                    RAISE_DB_ERROR(eConvFailed, "Failed to get Cass tuple iterator");

                using TSwitch = typename conditional<sizeof...(T) == 0, char, int64_t>::type;
                CassIteratorToTupleImpl<0, sizeof...(T), T...> (tuple_iterator.get(), t, TSwitch());
            }
            else
                t = std::tuple<T...>();
            break;
        default:
            if (type == CASS_VALUE_TYPE_UNKNOWN && (!Val || cass_value_is_null(Val)))
                t = std::tuple<T...>();
            else
                RAISE_DB_ERROR(eConvFailed, "Failed to convert Cass value to tuple: unsupported data type (Cass type - " + NStr::NumericToString(static_cast<int>(type)) + ")" );
    }
}


template<class T>
void CassValueConvertDef(const CassValue* Val, const T& _default, T& v)
{
    if (Val && cass_value_is_null(Val))
        v = _default;
    else
        CassValueConvert(Val, v);
}

template<typename T>
void ValueToCassCollection(const T& v, CassCollection* coll) {
    static_assert(sizeof(T) == 0, "ValueToCassCollection() is not implemented for this type");
}

template<> void ValueToCassCollection<bool>(const bool& v, CassCollection* coll);
template<> void ValueToCassCollection<int8_t>(const int8_t& v, CassCollection* coll);
template<> void ValueToCassCollection<int16_t>(const int16_t& v, CassCollection* coll);
template<> void ValueToCassCollection<int32_t>(const int32_t& v, CassCollection* coll);
template<> void ValueToCassCollection<int64_t>(const int64_t& v, CassCollection* coll);
template<> void ValueToCassCollection<double>(const double& v, CassCollection* coll);
template<> void ValueToCassCollection<float>(const float& v, CassCollection* coll);
template<> void ValueToCassCollection<string>(const string& v, CassCollection* coll);

template<typename T>
void ValueToCassTupleItem(const T& value, CassTuple* dest, int index) {
    static_assert(sizeof(T) == 0, "ValueToCassTupleItem() is not implemented for adding this item type to Cassandra tuple");
}

template<> void ValueToCassTupleItem<bool>(const bool& value, CassTuple* dest, int index);
template<> void ValueToCassTupleItem<int8_t>(const int8_t& value, CassTuple* dest, int index);
template<> void ValueToCassTupleItem<int16_t>(const int16_t& value, CassTuple* dest, int index);
template<> void ValueToCassTupleItem<int32_t>(const int32_t& value, CassTuple* dest, int index);
template<> void ValueToCassTupleItem<int64_t>(const int64_t& value, CassTuple* dest, int index);
template<> void ValueToCassTupleItem<double>(const double& value, CassTuple* dest, int index);
template<> void ValueToCassTupleItem<float>(const float& value, CassTuple* dest, int index);
template<> void ValueToCassTupleItem<string>(const string& value, CassTuple* dest, int index);


template<size_t N, typename... T>
void TupleToCassTupleImpl(const std::tuple<T...>&, CassTuple*, char)
{
}

template<size_t N, typename ...T>
void TupleToCassTupleImpl(const std::tuple<T...>& t, CassTuple* dest, int64_t)
{
    const auto& item = get<N - 1>(t);
    ValueToCassTupleItem(item, dest, N - 1);

    using TSwitch = typename conditional< (N - 1) == 0, char, int64_t>::type;
    TupleToCassTupleImpl<N - 1, T...>(t, dest, TSwitch());
}

template<typename ...T>
void ValueToCassCollection(const std::tuple<T...>& v, CassCollection* coll) {
    unique_ptr<CassTuple, function<void(CassTuple*)> > cass_tuple(
        cass_tuple_new(tuple_size< std::tuple<T...> >::value),
        [](CassTuple* tpl) {
            if (tpl)
                cass_tuple_free(tpl);
        }
    );
    if (!cass_tuple)
        RAISE_DB_ERROR(eConvFailed, "Failed to create Cass tuple");

    using TSwitch = typename conditional<sizeof...(T) == 0, char, int64_t>::type;
    TupleToCassTupleImpl< sizeof...(T), T... > (v, cass_tuple.get(), TSwitch());

    CassError err;
    if ((err = cass_collection_append_tuple(coll, cass_tuple.get())) != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append tuple value to Cassandra collection:");
}

template<typename ...T>
void ValueToCassTupleItem(const std::tuple<T...>& value, CassTuple* dest, int index) {
    unique_ptr<CassTuple, function<void(CassTuple*)> > cass_tuple(
        cass_tuple_new(tuple_size< std::tuple<T...> >::value),
        [](CassTuple* tpl) {
            if (tpl)
                cass_tuple_free(tpl);
        }
    );
    if (!cass_tuple)
        RAISE_DB_ERROR(eConvFailed, "Failed to create Cass tuple");
    
    using TSwitch = typename conditional<sizeof...(T) == 0, char, int64_t>::type;
    TupleToCassTupleImpl< sizeof...(T), T... > (value, cass_tuple.get(), TSwitch());

    CassError err = cass_tuple_set_tuple(dest, index, cass_tuple.get());
    if (err != CASS_OK)
        RAISE_CASS_ERROR(err, eConvFailed, "Failed to append tuple value to Cassandra tuple:");
}

template<typename ...T>
void ValueToCassTuple(const std::tuple<T...>& value, CassTuple* dest) {
    using TSwitch = typename conditional<sizeof...(T) == 0, char, int64_t>::type;
    TupleToCassTupleImpl< sizeof...(T), T... > (value, dest, TSwitch());
}

} // namespace Convert

END_IDBLOB_SCOPE


#endif
