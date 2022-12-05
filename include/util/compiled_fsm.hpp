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
    //constexpr CConstEmits(const value_type* ptr, size_t size): m_size{size}, m_data{ptr} {}
    constexpr CConstEmits(const value_type* ptr): m_data{ptr} {}
    constexpr bool test(size_t index) const {
        size_t left = index / numbits;
        size_t right = index % numbits;
        return m_data[left] & (value_type(1) << right);
    }
protected:
    //size_t m_size = 0;
    const value_type* m_data = nullptr;
};

class CCompiledFSM
{
public:
    using index_type = uint16_t;
    using TEmits  = CConstEmits;
    using TStates = ct::const_vector<index_type>;
    using THits1  = ct::const_vector<index_type>;
    using THits2  = ct::const_vector<std::initializer_list<index_type>>;

    TEmits  m_emit;
    THits1  m_hits1;
    THits2  m_hits2;
    TStates m_states;
    ct::const_vector<unsigned char> m_rules_asn1;

    const THits2::value_type& get_hits(index_type state) const;
};

} // namespace FSM

END_NCBI_SCOPE

#define NCBI_FSM_PREPARE(states, max_vec, hits, compact_size) \
    using TLocalFSM = ::ncbi::FSM::CCompiledFSM;

#define NCBI_FSM_RULES        static constexpr unsigned char s_rules[]
#define NCBI_FSM_EMIT_COMPACT static constexpr ::ncbi::FSM::CCompiledFSM::TEmits::value_type  s_compact []
#define NCBI_FSM_STATES       static constexpr ::ncbi::FSM::CCompiledFSM::TStates::value_type s_states []
#define NCBI_FSM_HITS_1(size) static constexpr ::ncbi::FSM::CCompiledFSM::THits1::value_type  s_hits_init_1[size]
#define NCBI_FSM_HITS_2(size) static constexpr ::ncbi::FSM::CCompiledFSM::THits2::value_type  s_hits_init_2[size]


#endif /* UTIL___COMPILED_FSM__HPP */
