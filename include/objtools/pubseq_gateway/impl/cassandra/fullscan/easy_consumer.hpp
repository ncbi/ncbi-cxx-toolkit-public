/*****************************************************************************
 *  $Id$
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
 *  Db Cassandra: interface of cassandra fullscan record consumer.
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__EASY_CONSUMER_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__EASY_CONSUMER_HPP

#include <corelib/ncbistl.hpp>

#include <objtools/pubseq_gateway/impl/cassandra/IdCassScope.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/cass_driver.hpp>
#include <objtools/pubseq_gateway/impl/cassandra/fullscan/consumer.hpp>

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

template <size_t N>
struct SFullscanFieldName {
    array<char, N> data;
    constexpr SFullscanFieldName(const char (&s)[N])
    {
        std::copy(s, s + N, begin(data));
    }
    constexpr operator string_view() const {
        return string_view(data.data(), N - 1);
    }
};

template <typename... TFullscanField>
struct TFullscanFieldList;

template <typename TFullscanField>
struct TFullscanFieldList<TFullscanField>
{
    using head_type = TFullscanField;
    using tail_type = false_type;
};

template <typename TFullscanField, typename... Tail>
struct TFullscanFieldList<TFullscanField, Tail...>
{
    using head_type = TFullscanField;
    using tail_type = TFullscanFieldList<Tail...>;
};

template <
    SFullscanFieldName TFieldName,
    typename TRecordType,
    typename TFieldType,
    TFieldType TRecordType::* TFieldMemberPtr
>
struct CFullScanField
{
    using record_type = TRecordType;
    static consteval string_view FieldName()
    {
        return TFieldName;
    }
};

template <
    SFullscanFieldName TFieldName,
    typename TRecordType,
    typename TFieldType,
    typename TArgumentType,
    void (TRecordType::*TFieldAssignMethod)(TArgumentType)
>
struct CFullScanFieldMethod
{
    using record_type = TRecordType;
    static consteval string_view FieldName()
    {
        return TFieldName;
    }
};

namespace impl {

    template <typename T, template <typename...> class Template>
    concept is_specialization_of = requires {
        []<typename... Args>(const Template<Args...>&){}(std::declval<T>());
    };

    template <typename T>
    concept has_push_back = requires(T container, typename T::value_type v) {
        container.push_back(v);
    };

    class CFullscanHelper final
    {
    public:
        CFullscanHelper() = default;

        template<typename TFullScanFieldList>
        vector<string> GetFieldList()
        {
            vector<string> fields;
            GetFieldListImpl<TFullScanFieldList>(fields, TFullScanFieldList());
            return fields;
        }

        template<typename RecordType, typename TFullScanFieldList>
        void ReadFields(CCassQuery const &query, RecordType &record)
        {
            ReadFieldsImpl<RecordType, TFullScanFieldList>(0, query, record, TFullScanFieldList());
        }

    private:
        template<typename TFieldList>
        void GetFieldListImpl(vector<string> &fields, TFieldList)
        {
            fields.push_back(string(TFieldList::head_type::FieldName()));
            if constexpr (!is_same_v<typename TFieldList::tail_type, std::false_type>) {
                GetFieldListImpl(fields, typename TFieldList::tail_type());
            }
        }

        template<typename TRecordType, typename TFieldList>
        void ReadFieldsImpl(int index, CCassQuery const &query, TRecordType &record, TFieldList)
        {
            ReadFieldImpl(index, query, record, typename TFieldList::head_type());
            if constexpr (!is_same_v<typename TFieldList::tail_type, std::false_type>) {
                ReadFieldsImpl(index + 1, query, record, typename TFieldList::tail_type());
            }
        }

        template<
            SFullscanFieldName TFieldName,
            typename TRecordType,
            typename TFieldType,
            TFieldType TRecordType::* TFieldMemberPtr
        >
        void ReadFieldImpl(int index, CCassQuery const &query, TRecordType &record,
                           CFullScanField<TFieldName, TRecordType, TFieldType, TFieldMemberPtr>)
        {
            if (!query.FieldIsNull(index)) {
                record.*TFieldMemberPtr = FetchFieldValue(index, query, TFieldType());
            }
        }

        template<
            SFullscanFieldName TFieldName,
            typename TRecordType,
            typename TFieldType,
            typename TArgumentType,
            void (TRecordType::*TFieldAssignMethod)(TArgumentType)
        >
        void ReadFieldImpl(int index, CCassQuery const &query, TRecordType &record,
                           CFullScanFieldMethod<TFieldName, TRecordType, TFieldType, TArgumentType, TFieldAssignMethod>)
        {
            if (!query.FieldIsNull(index)) {
                (record.*TFieldAssignMethod)(FetchFieldValue(index, query, TFieldType()));
            }
        }

        template<typename TFieldType>
        auto FetchFieldValue(int index, CCassQuery const &query, TFieldType)
        {
            if constexpr (is_same_v<TFieldType, int64_t>) {
                return query.FieldGetInt64Value(index);
            }
            else if constexpr (is_same_v<TFieldType, int32_t>) {
                return query.FieldGetInt32Value(index);
            }
            else if constexpr (is_same_v<TFieldType, int16_t>) {
                return query.FieldGetInt16Value(index);
            }
            else if constexpr (is_same_v<TFieldType, int8_t>) {
                return query.FieldGetInt8Value(index);
            }
            else if constexpr (is_same_v<TFieldType, bool>) {
                return query.FieldGetBoolValue(index);
            }
            else if constexpr (is_same_v<TFieldType, float> || is_same_v<TFieldType, double>) {
                return query.FieldGetFloatValue(index);
            }
            else if constexpr (is_same_v<TFieldType, string>) {
                return query.FieldGetStrValue(index);
            }
            else if constexpr (impl::is_specialization_of<TFieldType, std::tuple>) {
                return query.FieldGetTupleValue<TFieldType>(index);
            }
            else if constexpr (impl::is_specialization_of<TFieldType, std::map>) {
                TFieldType map_value;
                query.FieldGetMapValue<TFieldType>(index, map_value);
                return map_value;
            }
            else if constexpr (impl::is_specialization_of<TFieldType, std::set>) {
                TFieldType set_value;
                query.FieldGetSetValues(index, set_value);
                return set_value;
            }
            else if constexpr (impl::has_push_back<TFieldType>) {
                TFieldType container_value;
                query.FieldGetContainerValue(index, back_inserter(container_value));
                return container_value;
            }
            else {
                static_assert(is_same_v<TFieldType, false_type>, "Reading is not implemented for field type");
            }
        }
    };
}  // impl

/**
 * Templated Fullscan consumer
 *
 * ! Should be always used with ECassandraFullscanConsumerPolicy::eOnePerQuery
 *
 * @tparam TContainterType
 * @tparam TFullscanFieldList
 */
template<typename TContainterType, typename TFullscanFieldList>
class CFullscanEasyConsumer
    : public ICassandraFullscanConsumer
{
public:
    CFullscanEasyConsumer() = delete;
    CFullscanEasyConsumer(mutex *mtx, TContainterType *data)
        : m_Mutex(mtx)
        , m_Data(data)
    {}

    CFullscanEasyConsumer(CFullscanEasyConsumer &&) = default;
    CFullscanEasyConsumer(CFullscanEasyConsumer const &) = delete;
    CFullscanEasyConsumer &operator=(CFullscanEasyConsumer &&) = default;
    CFullscanEasyConsumer &operator=(CFullscanEasyConsumer const &) = delete;

    static vector<string> GetFieldList()
    {
        return impl::CFullscanHelper().GetFieldList<TFullscanFieldList>();
    }

    void OnQueryRestart() override final
    {
        m_Buffer.clear();
    }

    void Finalize() override
    {
        {
            lock_guard _(*m_Mutex);
            std::copy(begin(m_Buffer), end(m_Buffer), back_inserter(*m_Data));
        }
        m_Buffer.clear();
    }

    bool ReadRow(CCassQuery const &query) override
    {
        using TRecordType = TFullscanFieldList::head_type::record_type;
        TRecordType r{};
        impl::CFullscanHelper().ReadFields<TRecordType, TFullscanFieldList>(query, r);
        m_Buffer.push_back(r);
        return true;
    }

protected:
    size_t BufferSize() const
    {
        return m_Buffer.size();
    }

    size_t ContainerSize() const
    {
        lock_guard _(*m_Mutex);
        return m_Data->size();
    }

private:
    mutex *m_Mutex{nullptr};
    vector<typename TContainterType::value_type> m_Buffer;
    TContainterType *m_Data{nullptr};
};

struct CFullscanEasyRunner
{
    CFullscanEasyRunner() = delete;
    template <typename TConsumer, typename TRunner, typename TContainer>
    static bool Execute(TRunner& r, TContainer& c)
    {
        r.SetConsumerCreationPolicy(ECassandraFullscanConsumerPolicy::eOnePerQuery);
        mutex fullscan_mutex;
        r.SetConsumerFactory(
            [&fullscan_mutex, &c] {
                return make_unique<TConsumer>(&fullscan_mutex, &c);
            }
        );
        return r.Execute();
    }
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__FULLSCAN__EASY_CONSUMER_HPP
