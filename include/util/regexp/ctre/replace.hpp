#ifndef UTIL__REGEXP_CTRE_REPLACE_HPP
#define UTIL__REGEXP_CTRE_REPLACE_HPP

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
 *     Compile-time PCRE replace string
 *     It complements CTRE with replace functionality.
 *     Only core rules are implemented from https://www.pcre.org/current/doc/html/pcre2syntax.html#SEC32
 *
 * Usage examples:
 *
 * std::string input("One Two Three Four");
 * // ct::search_replace<search_pattern, replace_rule>;
 *
 * ct::search_replace<" (Two) ", " ${1}.${1} ">(input);
 * ct::search_replace<" Three ", " ">(input);
 */

 /*

REPLACEMENT STRINGS
If the PCRE2_SUBSTITUTE_LITERAL option is set, a replacement string for pcre2_substitute() is not interpreted. Otherwise, by default, the only special character is the dollar character in one of the following forms:

   $$                  insert a dollar character
   $n or ${n}          insert the contents of group n
   $<name>             insert the contents of named group
   $0 or $&            insert the entire matched substring
   $`                  insert the substring that precedes the match
   $'                  insert the substring that follows the match
   $_                  insert the entire input string
   $*MARK or ${*MARK}  insert a control verb name
For ${n}, n can be a name or a number.

If PCRE2_SUBSTITUTE_EXTENDED is set, there is additional interpretation:
1. Backslash is an escape character, and the forms described in "ESCAPED CHARACTERS" above are recognized. Also:

   \Q...\E   can be used to suppress interpretation
   \l        force the next character to lower case
   \u        force the next character to upper case
   \L        force subsequent characters to lower case
   \U        force subsequent characters to upper case
   \u\L      force next character to upper case, then all lower
   \l\U      force next character to lower case, then all upper
   \E        end \L or \U case forcing
   \b        backspace character (note: as in character class in pattern)
   \v        vertical tab character (note: not the same as in a pattern)
2. The Python form \g<n>, where the angle brackets are part of the syntax and n is either a group name or a number, is recognized as an alternative way of inserting the contents of a group, for example \g<3>.
3. Capture substitution supports the following additional forms:

   ${n:-string}             default for unset group
   ${n:+string1:string2}    values for set/unset group
The substitution strings themselves are expanded. Backslash can be used to escape colons and closing curly brackets.
*/

#include <util/compile_time.hpp>
#include "replace_impl.hpp"
#include "ctre.hpp"

namespace ct {


enum class TReplaceOpType { one, all };

template<ct_fixed_string_input_param replace_text>
struct pcre_regex
{
// public part of regex replacement

    using char_type = typename std::decay_t<decltype(replace_text)>::char_type;
    using runtime = ct_regex_details::build_replace_runtime<replace_text>;

    template<typename _Matched, typename _Iterator>
    struct TContext
    {
        _Matched matched;
        _Iterator begin;
        _Iterator end;
        template <size_t Id>
        constexpr auto get() const noexcept {
            return matched.template get<Id>().to_view();
        }
        template <ct_fixed_string_input_param Name>
        constexpr auto get() const noexcept {
            constexpr ctll::fixed_string<Name.size()> ctll_name = Name.get_array();
            return matched.template get<ctll_name>().to_view();
        }
    };

    template<TReplaceOpType kind, typename _Search, typename _Input, typename...TArgs>
    static
    size_t replace_many(std::integral_constant<TReplaceOpType, kind>, const _Search& search, _Input whole, std::basic_string<char_type, TArgs...>& output)
    {
        auto current = whole.begin();
        auto matched = search(current, whole.end());

        if (!matched)
            return 0;

        size_t counter = 0;
        output.clear();

        do {
            counter++;
            auto front_part_len = std::distance(current, matched.begin());
            if (front_part_len) {
                output.append(current, matched.begin());
            }

            auto b = whole.begin();
            auto e = whole.end();

            TContext<decltype(matched), decltype(b)> ctx{matched, b, e};

            runtime::apply_all(ctx, output);

            current = matched.end();

            if constexpr(kind == TReplaceOpType::all) {
                matched = search(current, whole.end());
            } else {
                break;
            }
        }
        while(matched);

        output.append(current, whole.end());

        return counter;

    }

    template<typename _Search, typename...TArgs>
    static
    bool replace_inplace(const _Search& search, std::basic_string<char_type, TArgs...>& orig)
    {
        std::basic_string_view<char_type> input(orig);

        auto results = search(input);

        if (!results)
            return false;

        auto front_part = std::distance(input.begin(), results.begin());

        auto b = input.begin();
        auto e = input.end();

        TContext<decltype(results), decltype(b)> ctx{results, b, e};

        // fast shortcut for simple replacements
        if constexpr (runtime::is_single_action) {
            auto replacement = runtime::get_substitute(ctx);
            orig.replace(front_part, results.size(), replacement);
        } else {
            auto back_part = front_part + results.size();

            std::decay_t<decltype(orig)> out; out.reserve(orig.size());

            if (front_part) {
                out = input.substr(0, front_part);
            }

            runtime::apply_all(ctx, out);

            if (back_part) {
                out.append(input.substr(back_part));
            }

            orig = std::move(out);
        }
        return true;
    }

};

template<
    TReplaceOpType kind,
    ct_fixed_string_input_param search_text,
    ct_fixed_string_input_param replace_text
    >
struct search_replace_op
{
    using char_type = typename std::decay_t<decltype(replace_text)>::char_type;
    using view_type = std::basic_string_view<char_type>;
    static constexpr ::ctll::fixed_string<search_text.size()> s_search_text = search_text.m_str;
    using replace_func = pcre_regex<replace_text>;
    static constexpr auto search_func = ::ctre::search<s_search_text>;

    using RE = typename ctre::regex_builder<s_search_text>::type;
    static constexpr bool starts_with_anchor = ctre::starts_with_anchor(ctre::singleline{}, ctll::list<RE>{});
    static_assert(!(starts_with_anchor && kind == TReplaceOpType::all), "using starting anchor with replace_all not allowed");

    template<TReplaceOpType _kind = kind, typename...TMoreArgs>
    bool operator()(std::basic_string<char_type, TMoreArgs...>& input) const
    {
        if constexpr (_kind == TReplaceOpType::one)
            return replace_func::replace_inplace(search_func, input);
        else {
            std::basic_string<char_type, TMoreArgs...> output;

            bool modified = replace_many(input, output);
            if (modified)
                input = std::move(output);
            return modified;
        }
    }

    template<typename...TMoreArgs>
    bool operator()(view_type input, std::basic_string<char_type, TMoreArgs...>& result) const
    {
        return replace_many(input, result);
    }

    template<typename...TMoreArgs>
    [[nodiscard]] static bool replace_many(view_type input, std::basic_string<char_type, TMoreArgs...>& output)
    {  // this method doesn't modify 'result' parameter if no changes occured
        size_t num_replaced = replace_func::replace_many(std::integral_constant<TReplaceOpType, kind>{}, search_func, input, output);
        return (num_replaced>0);
    }

    template<typename _T>
    static void replace_many(ct_regex_details::mutable_string<_T>& input)
    {
        typename ct_regex_details::mutable_string<_T>::output_type output;

        bool replaced = replace_many(input.to_view(), output);

        if (replaced > 0)
            input = std::move(output);
    }

};

// two separate operators to allow implicit conversion of various string-like classes
template<auto..._SearchArgs, typename = std::enable_if_t<std::is_same_v<char, typename search_replace_op<_SearchArgs...>::char_type>>>
[[nodiscard]] auto operator|(std::string_view input, const search_replace_op<_SearchArgs...>& next_op)
{
    ct_regex_details::mutable_string mut_input{input};
    next_op.replace_many(mut_input);
    return mut_input;
}

template<auto..._SearchArgs, typename = std::enable_if_t<std::is_same_v<wchar_t, typename search_replace_op<_SearchArgs...>::char_type>>>
[[nodiscard]] auto operator|(std::wstring_view input, const search_replace_op<_SearchArgs...>& next_op)
{
    ct_regex_details::mutable_string mut_input{input};
    next_op.replace_many(mut_input);
    return mut_input;
}


template<typename _T, auto..._SearchArgs>
[[nodiscard]] ct_regex_details::mutable_string<_T>&& operator|(ct_regex_details::mutable_string<_T>&& input, const search_replace_op<_SearchArgs...>& next_op)
{
    next_op.replace_many(input);
    return std::move(input);
}

template<typename _T, auto...TArgs>
[[nodiscard]] auto operator|(const ct_regex_details::mutable_string<_T>& input, const search_replace_op<TArgs...>& next_op)
{
    return operator|(input.to_view(), next_op);
}

template <
    ct_fixed_string_input_param search_text,
    ct_fixed_string_input_param replace_text
    >
static constexpr inline auto search_replace = search_replace_op<TReplaceOpType::one, search_text, replace_text>();

template <
    ct_fixed_string_input_param search_text,
    ct_fixed_string_input_param replace_text
    >
static constexpr inline auto search_replace_all = search_replace_op<TReplaceOpType::all, search_text, replace_text>();

template <
    ct_fixed_string_input_param search_text,
    ct_fixed_string_input_param replace_text
    >
static constexpr inline auto search_replace_one = search_replace_op<TReplaceOpType::one, search_text, replace_text>();


}; // namespace ct

#endif
