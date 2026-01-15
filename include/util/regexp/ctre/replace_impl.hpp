#ifndef UTIL__REGEXP_CTRE_REPLACE_IMPL_HPP
#define UTIL__REGEXP_CTRE_REPLACE_IMPL_HPP

/*
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
 * File Description:
 *     Various internal classes implementing compile-time PCRE replacement
 *     string
 *
 */

#include <cstddef>
#include <utility>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

// Let's allow C++ 17 compilations for awhile
#if NCBI_CNTTP_COMPILER_CHECK
#define ct_fixed_string_input_param ::ct::fixed_string
#else
    #define ct_fixed_string_input_param const auto&
#endif


namespace ct_regex_details {

enum class e_action
{
#ifdef _DEBUG
    intentionanely_unused,
#endif
    empty,
    character,
    group,
    precedes,
    follows,
    input,
    group_name,
    compilation_error,
    // underimplemented these
    //$*MARK or ${*MARK}  insert a control verb name
};

template<typename _CharType, size_t N>
class compiled_regex;

template<typename _CharType, size_t N>
struct group_name_op
{
    ct::fixed_string<_CharType, N> m_name {};
};
template<typename _CharType, size_t N>
group_name_op(ct::fixed_string<_CharType, N>) -> group_name_op<_CharType, N>;

struct group_number_op
{
    size_t m_number = 0;
};

struct action_type
{
    e_action cmd = e_action::empty;
    size_t detail = 0;
};

template<typename _CharType, size_t N>
class compiled_regex
{
// Intermediate compilation stage parser
// The class contains the array of the same size as input string
// Each element of the parsed array represent action, described by e_action enum
// Supporting elements (backslashes, curly brackets, dollar sign) are removed
//   thus making array smaller and having m_real_size set accordinly
// In case parsing error occured, m_error_pos is set and parsing aborts
//    no static_assert or exception is thrown, thus allowing this parser to run
//    as a constexpr or as a regular execution flow. This requires calling party
//    to evaluate m_error_pos value

public:
    using char_type = _CharType;
    using parsed_str_type = std::array<action_type, N>;
    using input_array_type = ct::fixed_string<_CharType, N>;

    constexpr compiled_regex() = default;
    constexpr compiled_regex(const input_array_type& input)
    {
        x_parse(input);
    }
    // non-constexpr method for debugging
    compiled_regex(int, const _CharType (&input)[N+1])
        : compiled_regex(input)
    {}

    constexpr size_t find_end_string(size_t begin) const
    {
        size_t i = begin;

        while (i<m_real_size && m_parsed[i].cmd == e_action::character)
            ++i;

        return i;
    }

    template<size_t len>
    constexpr auto extract_string(size_t begin, std::integral_constant<size_t, len>) const
        -> ct::fixed_string<_CharType, len>
    {
        return x_extract_string(begin, std::make_index_sequence<len>{});
    }

    constexpr size_t real_size() const { return m_real_size; }
    constexpr size_t error_pos() const { return m_error_pos; }
    constexpr const parsed_str_type& parsed() const { return m_parsed; }
    constexpr const action_type& operator[](size_t i) const { return m_parsed[i]; }

private:
    parsed_str_type m_parsed{};
    size_t  m_error_pos = 0;
    size_t  m_real_size = 0;

    template<size_t...Is>
    constexpr auto x_extract_string(size_t begin, std::index_sequence<Is...>) const
        -> ct::fixed_string<_CharType, sizeof...(Is)>
    {
        std::array<_CharType, sizeof...(Is)> arr = {_CharType(m_parsed[begin + Is].detail) ... };
        return ct::fixed_string{arr};
    }

    constexpr
    void x_parse(const input_array_type& input)
    {
        size_t pos_output = 0;
        size_t pos_input = 0;

        while ( pos_input < input.size() )
        {
            _CharType next_c = input[pos_input++];
            if (pos_input == input.size() && (next_c == '$' or next_c == '\\')) {
                m_error_pos = pos_input;
                return;
            }

            action_type next{e_action::character, static_cast<uint32_t>(next_c)};
            if (next_c == '$') {
                // after processing it will point at the last processed symbol
                next = x_parse_dollar(input, pos_input);
                ++pos_input;
            } else
            if (next_c == '\\') {
                next = x_parse_slash(input[pos_input++]);
            }

            if (next.cmd == e_action::compilation_error) {
                m_error_pos = next.detail;
                return;
            }

            m_parsed[pos_output++] = next;
            if (next.cmd == e_action::group_name) {
                size_t len = next.detail;
                for (size_t i = 0; i<len; ++i) {
                    next_c = input[pos_input + i - 1];
                    m_parsed[pos_output++] = action_type{e_action::character, static_cast<uint32_t>(next_c)};
                }
                pos_input += len;
            }
        }
        m_real_size = pos_output;
    }

    static constexpr
    action_type x_parse_dollar(const input_array_type& input, size_t& dollar_pos)
    {
        _CharType current = input[dollar_pos];

        _CharType need_braces = 0;

        switch (current)
        {
            case '$':
                return {e_action::character, '$'};
            case '&':
                return {e_action::group, 0};
            case '`':
                return {e_action::precedes, 0};
            case '\'':
                return {e_action::follows, 0};
            case '_':
                return {e_action::input, 0};
            case '{':
                need_braces = '}';
                dollar_pos++;
                break;
            case '<':
                need_braces = '>';
                dollar_pos++;
                break;
            default:
                if (current < '0' || current > '9')
                    return {e_action::compilation_error, dollar_pos};
                break;
        }

        size_t _begin = dollar_pos;

        size_t _end = _begin;

        if (need_braces) {
            while (_end < input.size() && input[_end] != need_braces)
                ++_end;
        } else {
            while (_end < input.size() && input[_end]>='0' && input[_end]<='9')
                ++_end;
        }

        if ((_end == input.size() && need_braces) || _begin == _end)
            return {e_action::compilation_error, _end};

        int group_number = -1;
        if (need_braces == 0 || need_braces == '}')
            group_number = x_try_parse_number(input, _begin, _end);

        if (need_braces && group_number < 0) {
            // we accept anything as a group name
            size_t _len = _end - _begin;
            return {e_action::group_name, _len};
        }

        if (group_number<0 || group_number > std::numeric_limits<_CharType>::max())
            return {e_action::compilation_error, _begin};

        if (need_braces == 0)
            --_end;

        dollar_pos = _end;
        return {e_action::group, static_cast<uint32_t>(group_number)};
    }

    static constexpr
    int x_try_parse_number(const input_array_type& input, size_t _begin, size_t _end)
    {
        // no big numbers allowed
        if (_end - _begin > 4)
            return -1;

        int result = 0;

        while(_begin != _end) {
            int c = input[_begin++];

            if (c < '0' || c > '9')
                return -1;

            c -= '0';
            result = result*10 + c;
        }
        return result;
    }

    static constexpr
    action_type x_parse_slash(_CharType c)
    {
        if (c == 'n')
            c = '\n';
        if (c == 'r')
            c = '\r';

        return {e_action::character, static_cast<uint32_t>(c)};
    }


}; // compiled_regex class

template<typename _CharType, size_t N>
compiled_regex(const _CharType (&)[N]) -> compiled_regex<_CharType, N-1>;
template<typename _CharType, size_t N>
compiled_regex(const ct::fixed_string<_CharType, N>&) -> compiled_regex<_CharType, N>;

template<typename _CharType, size_t N>
compiled_regex(int, const _CharType (&)[N]) -> compiled_regex<_CharType, N-1>;


//template<const compiled_regex& compiled>
// Still allows C++-17
template<typename _CharType, size_t N, const compiled_regex<_CharType, N>& compiled>
struct build_actions_chain
{
    static constexpr auto build()
    {
        if constexpr (compiled.real_size() == 0) {
            auto newcode = std::make_tuple(e_action::empty);
            return newcode;
        } else {
            return x_build_next_action<0>(std::make_tuple());
        }
    }

    template<size_t begin, typename...TArgs>
    static constexpr auto x_build_next_action(const std::tuple<TArgs...>& tup)
    {
        if constexpr (begin >= compiled.real_size()) {
            return tup;
        } else {
            constexpr const action_type& cmd = compiled[begin];
            if constexpr (cmd.cmd == e_action::empty) {
                return tup;
            } else
            if constexpr (cmd.cmd == e_action::group_name) {
#if NCBI_CNTTP_COMPILER_CHECK
                constexpr size_t len = cmd.detail;
                ct::fixed_string group_name = compiled.extract_string(begin+1, std::integral_constant<size_t, len>{});
                group_name_op op{group_name};
                auto newcode = std::make_tuple(op);
                auto res = std::tuple_cat(tup, newcode);
                return x_build_next_action<begin+1+len>(res);
#else
                static_assert(0, "group names are only allowed with C++-20 enabled");
                return tup;
#endif
            } else
            if constexpr (cmd.cmd == e_action::character) {
                constexpr size_t next_end = compiled.find_end_string(begin);
                auto newcode = std::make_tuple(compiled.extract_string(begin, std::integral_constant<size_t, next_end-begin>{}));
                auto res = std::tuple_cat(tup, newcode);
                return x_build_next_action<next_end>(res);
            } else
            if constexpr (cmd.cmd == e_action::group) {
                auto newcode = std::make_tuple(group_number_op{cmd.detail});
                auto res = std::tuple_cat(tup, newcode);
                return x_build_next_action<begin+1>(res);
            } else
            if constexpr (ct::inline_bitset<
                    e_action::precedes,
                    e_action::follows,
                    e_action::input>.test(cmd.cmd)) {

                auto newcode = std::make_tuple(cmd.cmd);
                auto res = std::tuple_cat(tup, newcode);
                return x_build_next_action<begin+1>(res);
            } else {
                //static_assert(false, "Underimplemented");
                return tup;
            }
        }
    }
};

template<ct_fixed_string_input_param input>
struct compiled_regex_struct
{
    // it exists for C++-17 compatibility
    static constexpr auto compiled = compiled_regex(input);
    static_assert (compiled.error_pos() == 0);
    //static_assert (compiled.real_size() > 0);
};

template<ct_fixed_string_input_param input>
constexpr auto build_replace_runtime_actions()
{
//  This function construct an optimized structure for future use at runtime
//  It produced a tuple of actions thus, requires running as constexpr, regex text
//     is provided as a class non-type template parameter
//  cNTTP parameter of fixed_string is a template and requested without template parameters
//     this forces adequate deduction rules for fixed_string to be maintained
//  The tuple of actions, each of different size and type is composed
//  Actions can be text strings, references to regex match groups, etc

// Still allows C++-17

    using compiled = compiled_regex_struct<input>;
    using char_type = typename std::decay_t<decltype(input)>::char_type;
    return build_actions_chain<char_type, input.size(), compiled::compiled>::build();
}

template<ct_fixed_string_input_param input>
class build_replace_runtime
{
// The runtime part of regex replacement
// The class is initialised via cNTTP regex replace text
// It compiles and initialises action list as class static members
//    thus allows optimal application of replace actions
// Since it works with a matching part of serach&replace scenario,
//    its logic is bound to ::ctre::regex_results semantics which is
//    passed as matched parameter
// Instantiation of this apply_all method will fail at compile time in case
//    RE searching rules do not correspond to replace rules. For example
//    when replace rule refer to an RE group that doesn't exist.
//    This is achieved by enforcing 'if constexpr' and 'get<>' methods
public:

    using char_type = typename std::decay_t<decltype(input)>::char_type;

    // compile and compose runtime structure
    static constexpr auto runtime = build_replace_runtime_actions<input>();

    static constexpr size_t size = std::tuple_size<decltype(runtime)>::value;
    static constexpr bool is_single_action = size == 1;
    static_assert(size>0);

    // non-constexpr method for debugging purposes
    static auto build()
    {
        return build_replace_runtime_actions<input>();
    }

    // runtime methods, use compile time structures
    // this method expected to unroll whole structure into a simple linear code,
    // possibly inlinable
    template<typename _Context, typename...TArgs>
    static void apply_all(_Context& ctx, std::basic_string<char_type, TArgs...>& out)
    {
        x_apply_all<0>(ctx, out);
    }

    template<typename _Context, size_t N = 0>
    static auto get_substitute(_Context& ctx)
    {
        std::basic_string_view subst = x_get_substitute<N>(ctx, runtime);
        return subst;
    }

private:

    template<size_t N, typename _Context, typename...TArgs>
    static void x_apply_all(_Context& ctx, std::basic_string<char_type, TArgs...>& out)
    {
        if constexpr (N < size)
        {
            constexpr const auto& op = std::get<N>(runtime);
            std::basic_string_view<char_type> subst = x_get_substitute<N>(ctx, op);
            out.append(subst);
            x_apply_all<N+1>(ctx, out);
        }
    }

    template<size_t pos, typename _Context, typename...TArgs>
    static auto x_get_substitute(_Context& ctx, const std::tuple<TArgs...>&)
    {
        constexpr const auto& cmd = std::get<pos>(runtime);
        return x_get_substitute<pos>(ctx, cmd);
    }

    template<size_t pos, size_t N, typename _Context>
    static auto x_get_substitute(_Context&, const ct::fixed_string<char_type, N>& s)
    {
        return s.to_view();
    }

    template<size_t pos, size_t N, typename _Context>
    static auto x_get_substitute(_Context& ctx, const group_name_op<char_type, N>&)
    {
        constexpr const auto& cmd = std::get<pos>(runtime);
        // next will fail at compile time if 'search' pattern doesn't have
        // a group specified in 'replace' pattern
        return ctx.template get<cmd.m_name>();
    }

    template<size_t pos, typename _Context>
    static auto x_get_substitute(const _Context& ctx, const group_number_op&)
    {
        constexpr const group_number_op& cmd = std::get<pos>(runtime);
        // next will fail at compile time if 'search' pattern doesn't have
        // a group specified in 'replace' pattern
        return ctx.template get<cmd.m_number>();
    }

    template<size_t pos, typename _Context>
    static auto x_get_substitute(const _Context& ctx, const e_action&)
    {
        constexpr const e_action& cmd = std::get<pos>(runtime);

        if constexpr (cmd == e_action::precedes) {
            return std::basic_string_view<char_type>(ctx.begin, ctx.matched.begin());
        } else
        if constexpr (cmd == e_action::follows) {
            return std::basic_string_view<char_type>(ctx.matched.end(), ctx.end);
        } else
        if constexpr (cmd == e_action::input) {
            return std::basic_string_view<char_type>(ctx.begin, ctx.end);
        } else
        if constexpr (cmd == e_action::empty) {
            return std::basic_string_view<char_type>{};
        } else {
            //throw std::runtime_error("ct::pcre_regex wrong operation");
            return std::basic_string_view<char_type>{};
        }
    }
}; // build_replace_runtime class

template<typename _Input, typename = _Input>
struct DeduceMutableString;

template<typename...TArgs>
struct DeduceMutableString<std::basic_string<TArgs...>&>
{
    using output_type = std::basic_string<TArgs...>;
};

template<typename _T>
struct DeduceMutableString<const _T&> : DeduceMutableString<_T&> {};

template<typename...TArgs>
struct DeduceMutableString<std::basic_string_view<TArgs...>>
{
    using output_type = std::basic_string<TArgs...>;
};

template<typename _Input>
class mutable_string
{
public:
    // all access methods are non-const

    mutable_string() = delete;
    mutable_string(const mutable_string&) = default;
    mutable_string(mutable_string&&) = default;
    mutable_string& operator=(const mutable_string&) = default;
    mutable_string& operator=(mutable_string&&) = default;

    using output_type = typename DeduceMutableString<_Input>::output_type;
    using input_type = _Input;
    using view_type   = std::basic_string_view<typename output_type::value_type, typename output_type::traits_type>;

    static_assert(std::is_same_v<_Input, output_type> || std::is_constructible_v<output_type, _Input>);

    explicit mutable_string(input_type orig): m_orig{orig} {}

    using cref_type = const output_type&;
    using ref_type  = output_type;

    operator input_type ()
    {
        if (m_modified)
            return m_modified.value();
        else
            return m_orig;
    }

    view_type to_view() const
    {
        if (m_modified)
            return *m_modified;
        else
            return m_orig;
    }

    template<typename _T=_Input, typename = std::enable_if_t<std::is_constructible_v<output_type, const _T&>, _T>>
    bool operator==(const _T& v) const
    {
        return to_view() == v;
    }

    output_type& set()
    {
        if (!m_modified) {
            m_modified = m_orig;
        }
        return *m_modified;
    }
    operator output_type()
    {
        if (m_modified)
            return std::move(m_modified.value());
        else
            return output_type(std::move(m_orig));
    }

    bool modified() const
    {
        return (bool) m_modified;
    }

    template<typename _Change, typename = std::enable_if_t<std::is_assignable_v<output_type, _Change>>>
    mutable_string& operator=(_Change&& new_value)
    {
        m_modified = std::forward<_Change>(new_value);
        return *this;
    }

private:
    input_type m_orig;
    std::optional<output_type> m_modified;
};

template<typename...TArgs>
mutable_string(const std::basic_string_view<TArgs...>&) -> mutable_string<std::basic_string_view<TArgs...>>;

template<typename...TArgs>
mutable_string(std::basic_string<TArgs...>&) -> mutable_string<std::basic_string<TArgs...>&>;

template<typename...TArgs>
mutable_string(const std::basic_string<TArgs...>&) -> mutable_string<const std::basic_string<TArgs...>&>;

template<typename _CharType>
mutable_string(_CharType*) -> mutable_string<std::basic_string_view<std::remove_const_t<_CharType>>>;


}; // namespace ct_regex_details

#endif
