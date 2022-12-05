#ifndef UTIL___COMPILED_FSM__HPP
#define UTIL___COMPILED_FSM__HPP

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
    constexpr CConstEmits(const value_type* ptr, size_t size): m_size{size}, m_data{ptr} {}
    constexpr bool test(size_t index) const {
        size_t left = index / numbits;
        size_t right = index % numbits;
        return m_data[left] & (value_type(1) << right);
    }
protected:
    size_t m_size = 0;
    const value_type* m_data = nullptr;
};

// temporal class until generator is improved
template<size_t N>
class CConstEmitsImpl: public CConstEmits
{
public:
    using TEmitsSet = ct::const_bitset<N, size_t>;
    constexpr CConstEmitsImpl(bool const (&init)[N]):
        m_bits{TEmitsSet::traits::set_bits(ct::make_array(init))}
    {
        CConstEmits::m_size = N;
        CConstEmits::m_data = m_bits.data();
    }
    constexpr CConstEmitsImpl(const std::initializer_list<bool>& init):
        m_bits{TEmitsSet::traits::set_bits(init)}
    {
        CConstEmits::m_size = N;
        CConstEmits::m_data = m_bits.data();
    }

    template<size_t other>
    static constexpr auto MakeEmits(bool const (&init)[other])
    {
        return CConstEmitsImpl<other>(init);
    }

protected:
    typename TEmitsSet::_Array_t m_bits;
};

class CCompiledFSM
{
public:
    using index_type = uint32_t;
    using TEmits  = CConstEmits;
    using TStates = ct::const_vector<index_type>;
    using THits = ct::const_map_presorted<index_type, std::initializer_list<index_type>>;

    TEmits  m_emit;
    THits   m_hits;
    TStates m_states;
    ct::const_vector<unsigned char> m_rules_asn1;
};

template<size_t _states_size, size_t _max_vec, size_t _num_hits, size_t _compacted_size>
class CCompiledFSM_Impl: public CCompiledFSM
{
public:

    template<typename _emit_init, typename _hits_init, typename _states_init, typename _rules_init>
    constexpr
    CCompiledFSM_Impl(_emit_init&& _emit, _hits_init&& hits_init, _states_init&& states_init, _rules_init&& rules)
        : CCompiledFSM{
            CConstEmits(_emit, _num_hits),
            THits::presorted(hits_init),
            states_init,
            rules}
    {
    }
    using THitsInit = THits::init_type;
};

} // namespace FSM

END_NCBI_SCOPE

#define NCBI_FSM_PREPARE(states, max_vec, hits, compact_size) \
    using TLocalFSM = ::ncbi::FSM::CCompiledFSM_Impl<states, max_vec, hits, compact_size>;

#define NCBI_FSM_RULES        static constexpr unsigned char s_rules[]
#define NCBI_FSM_EMIT_COMPACT static constexpr TLocalFSM::TEmits::value_type s_compact []
#define NCBI_FSM_HITS         static constexpr TLocalFSM::THitsInit s_hits_init[]
#define NCBI_FSM_STATES       static constexpr TLocalFSM::TStates::value_type s_states []


#endif /* UTIL___COMPILED_FSM__HPP */
