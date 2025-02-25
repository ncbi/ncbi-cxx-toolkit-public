#ifndef UTIL___IMPL_GENERATED_FSM__HPP
#define UTIL___IMPL_GENERATED_FSM__HPP

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
* Author: Sergiy Gotvyanskyy
*
*/

#include <corelib/ncbistd.hpp>
#include <functional>
#include <util/compile_time.hpp>

BEGIN_NCBI_SCOPE

namespace FSM
{

class CConstEmits
{
public:
    using value_type = uint64_t;
    static constexpr size_t numbits = sizeof(value_type)*8;
    constexpr CConstEmits() = default;
    constexpr CConstEmits(const value_type* ptr): m_data{ptr} {}
    constexpr bool test(size_t index) const {
        size_t left = index / numbits;
        size_t right = index % numbits;
        return m_data[left] & (value_type(1) << right);
    }
protected:
    const value_type* m_data = nullptr;
};

template<typename _Iterator>
struct range: private std::pair<_Iterator, _Iterator>
{
    using _MyBase = std::pair<_Iterator, _Iterator>;
    using _MyBase::_MyBase;

    using const_iterator = _Iterator;
    constexpr const_iterator cbegin() const noexcept { return _MyBase::first; }
    constexpr const_iterator begin() const noexcept  { return cbegin(); }
    constexpr const_iterator cend() const noexcept   { return _MyBase::second; }
    constexpr const_iterator end() const noexcept    { return cend(); }
};

/*
    MSVC compiler cannot instantiate array of std::initializer_lists.
    This doesn't work on windows. It compiles, but in fact in run-time it's empty
        static constexpr
        std::initializer_list<int> il_two_by_three [] =  { {1, 2, 3}, {4, 5} };

    But this still does:
        static constexpr std::initializer_list<int> il_three_items = {1, 2, 3};
*/

//#ifndef NCBI_OS_MSWIN
//#   define FSM_USE_INITIALIZER_LIST
//#endif

class CCompiledFSM
{
public:
    using index_type = uint16_t;
    using TRulesAsn1 = ct::const_vector<unsigned char>;
    using TEmits  = CConstEmits;
    using TStates = ct::const_vector<index_type>;
    using THits1  = ct::const_vector<index_type>;

    constexpr CCompiledFSM() = default;

    TStates m_states;

    TEmits  m_emit;
    THits1  m_hits1;
    TRulesAsn1 m_rules_asn1;

#ifdef FSM_USE_INITIALIZER_LIST
    using hits_iterator = std::initializer_list<index_type>::const_iterator;
#else
    using hits_iterator = const index_type*;
#endif

    typedef range<hits_iterator> (*get_hits_func_type) (const CCompiledFSM* self, index_type state);
    get_hits_func_type m_get_hits = nullptr;

    range<hits_iterator> get_hits(index_type state) const
    {
        if (m_get_hits)
            return m_get_hits(this, state);
        else
            return {};
    }
};

template<typename _THits2>
class TCompiledFSMImpl: public CCompiledFSM
{
public:
    using index_type = CCompiledFSM::index_type;
    using THits2  = _THits2;

    constexpr TCompiledFSMImpl() = default;
    constexpr TCompiledFSMImpl(const TEmits& emit, const THits1& hits1, const THits2& hits2, const TStates& states, const TRulesAsn1& rules_asn1)
        : m_hits2{hits2}
    {
        CCompiledFSM::m_emit = emit;
        CCompiledFSM::m_hits1 = hits1;
        CCompiledFSM::m_states = states;
        CCompiledFSM::m_rules_asn1 = rules_asn1;
        CCompiledFSM::m_get_hits = s_get_hits;
    }

    ct::const_vector<typename _THits2::value_type> m_hits2;

private:
    static range<hits_iterator> s_get_hits(const CCompiledFSM* self, index_type state)
    {
        const TCompiledFSMImpl* _this = static_cast<const TCompiledFSMImpl*>(self);
        return _this->s_get_hits(state);
    }

    range<hits_iterator> s_get_hits(index_type state) const
    {
        auto it = std::lower_bound(m_hits1.begin(), m_hits1.end(), state);
#ifdef _DEBUG
        _ASSERT(it != m_hits1.end());
#endif
        auto dist = std::distance(m_hits1.begin(), it);
        const auto& hits = m_hits2[dist];
        return {hits.begin(), hits.end()};
    }

};

#ifdef FSM_USE_INITIALIZER_LIST
    template<size_t _max_vec, size_t _Hits>
    using TLocalFSMImpl = TCompiledFSMImpl<std::array<std::initializer_list<CCompiledFSM::index_type>, _Hits>>;
#else
    // this is will copy-construct initialer_list contains instead of relying on the underlying array which apparently doesn't work with MSVC
    template<typename _T, size_t _max_vec>
    class TVector
    {
    public:
        using TCont = std::array<_T, _max_vec>;
        constexpr TVector() = default;
        constexpr TVector(const std::initializer_list<_T>& _init) :
            m_realsize{uint8_t(_init.size())}
        {
            auto dest = m_vec.begin();
            for (auto rec: _init) {
                *dest++ = rec;
            }
        }

        using const_iterator = typename TCont::const_pointer;
        using iterator       = const_iterator;
        using value_type     = typename TCont::value_type;
        constexpr const_iterator cbegin() const noexcept { return m_vec.data(); }
        constexpr const_iterator begin()  const noexcept { return cbegin(); }
        constexpr const_iterator cend()   const noexcept { return m_vec.data() + m_realsize; }
        constexpr const_iterator end()    const noexcept { return cend(); }
    private:
        TCont m_vec {};
        uint8_t m_realsize{0};
    };

    template<size_t _max_vec, size_t _Hits>
    using TLocalFSMImpl = TCompiledFSMImpl<std::array<TVector<CCompiledFSM::index_type, _max_vec>, _Hits>>;

#endif

} // namespace FSM

END_NCBI_SCOPE

#define NCBI_FSM_PREPARE(states, max_vec, hits, compact_size) \
    using TLocalFSM = ::ncbi::FSM::TLocalFSMImpl<max_vec, hits>;

#define NCBI_FSM_RULES        static constexpr unsigned char s_rules[]
#define NCBI_FSM_EMIT_COMPACT static constexpr TLocalFSM::TEmits::value_type  s_compact []
#define NCBI_FSM_STATES       static constexpr TLocalFSM::TStates::value_type s_states []
#define NCBI_FSM_HITS_1(size) static constexpr TLocalFSM::THits1::value_type  s_hits_init_1[size]
#define NCBI_FSM_HITS_2(size) static constexpr TLocalFSM::THits2              s_hits_init_2


#endif /* UTIL___IMPL_GENERATED_FSM__HPP */
