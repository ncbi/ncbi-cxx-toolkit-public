#ifndef UTIL___MULTIPATTERN_SEARCH__HPP
#define UTIL___MULTIPATTERN_SEARCH__HPP

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
* Author: Sema Kachalo, NCBI
*
*/

/// @file multipattern_search.hpp
/// Simultaneous search of multiple RegEx patterns in the input string


BEGIN_NCBI_SCOPE

class CRegExFSA;


///////////////////////////////////////////////////////////////////////
///
/// CMultipatternSearch
///
/// Simultaneous search of multiple string or RegEx patterns in the input string
///
/// CMultipatternSearch builds a Finite State Machine (FSM)
/// from a list of search strings or regular expression patterns.
/// It can then search all patterns simultaneously,
/// requiring only a single traversal of the input string.
/// Use this class to increase the search performance
/// when the number of search patterns is large (10 and more)
/// If the patterns are known in advance, FSM can be exported as C code
/// and compiled for the further performance improvement.

class NCBI_XUTIL_EXPORT CMultipatternSearch
{
public:
    CMultipatternSearch();
    ~CMultipatternSearch();

    /// Search flags (for non-RegEx patterns only!)
    enum EFlags {
        fNoCase      = 1 << 0,
        fBeginString = 1 << 1,
        fEndString   = 1 << 2,
        fWholeString = fBeginString | fEndString,
        fBeginWord   = 1 << 3,
        fEndWord     = 1 << 4,
        fWholeWord   = fBeginWord | fEndWord
    };
    DECLARE_SAFE_FLAGS_TYPE(EFlags, TFlags);

    
    ///@{
    /// Add search pattern to the FSM
    ///
    /// @param pattern
    ///   A search pattern to add to the FSM.
    ///   If begins with a slash (/), then it is considered a RegEx.
    /// @param flags
    ///   Additional search conditions.
    ///   If the first argument is a RegEx, then the flags are ignored.
    void AddPattern(const char*   pattern, TFlags flags = 0);
    void AddPattern(const string& pattern, TFlags flags = 0)  { AddPattern(pattern.c_str(), flags); }
    ///@}

    /// Generate graphical representation of the FSM in DOT format.
    /// For more details, see http://www.graphviz.org/
    ///
    /// @param out
    ///   A stream to receive the output.
    void GenerateDotGraph(ostream& out) const;

    /// Generate C code for the FSM search.
    ///
    /// @param out
    ///   A stream to receive the output.
    void GenerateSourceCode(ostream& out) const;

    /// When the pattern is found, the search can be stopped or continued
    enum EOnFind {
        eStopSearch,
        eContinueSearch
    };

    ///@{
    /// Run the FSM search on the input string
    ///
    /// @param input
    ///   Input string.
    /// @param found_callback
    ///   Function or function-like object to call when the pattern is found.
    ///   It can accept one or two parameters and return void or EOnFind.
    ///   If it returns eStopSearch, the search terminates.
    ///   If it returns eContinueSearch or void, the search continues.
    /// @sa CFoundCallback
    ///
    /// @code
    ///  Search(str, [](size_t pattern) { cout << "Found " << pattern << "\n"; });
    ///  Search(str, [](size_t pattern) -> CMultipatternSearch::EOnFind { cout << "Found " << pattern << "\n";  return CMultipatternSearch::eContinueSearch; });
    ///  Search(str, [](size_t pattern, size_t position) { cout << "Found " << pattern << " " << position << "\n"; });
    ///  Search(str, [](size_t pattern, size_t position) -> CMultipatternSearch::EOnFind { cout << "Found " << pattern << " " << position << "\n";  return CMultipatternSearch::eContinueSearch; });
    /// @endcode

    template <typename TFoundCallback> void Search(const char*   input, TFoundCallback found_callback) const
    { x_Parse(input, CFoundCallback_Impl<TFoundCallback>(found_callback)); }
    template <typename TFoundCallback> void Search(const string& input, TFoundCallback found_callback) const
    { Search(input.c_str(), found_callback); }
    ///@}


private:
    struct CFoundCallback
    {
        /// @param pattern
        ///  The zero-based index of the found pattern (in the order in which it was AddPattern()ed).
        ///  Note that this is the END position!
        /// @param position
        ///   The zero-based position in the input string where the pattern was found
        ///     Note that this is not the beginning of the found match,
        ///     this is a position on which FSM trigerred the report,
        ///     normally, at the end of the match.
        ///   e.g. given the input "abc",
        ///     /abc/ will trigger at position 2 ('c'), and
        ///     /abc\b/ will trigger at position 3 (end of string).
        /// @return
        ///   Whether to stop parsing; or to continue
        virtual EOnFind operator()(size_t pattern, size_t position) const = 0;
    };

    /// Implementation of the Search() method
    void x_Parse(const char* input, CFoundCallback& found_callback) const;

    /// Finit State Machine that does all work
    unique_ptr<CRegExFSA> m_FSM;

    /// CFoundCallback_Impl - convert various function-like objects to the uniform CFoundCallback type
    template<typename TFoundCallback>
    struct CFoundCallback_Impl : public CFoundCallback
    {
        CFoundCallback_Impl(const TFoundCallback& f) : m_FoundCallback(f) {}
        const TFoundCallback& m_FoundCallback;

        EOnFind operator()(size_t pattern, size_t position) const { return Exec(m_FoundCallback, pattern, position); }

    private:
        template <typename F> EOnFind Exec(F& f, size_t pattern, size_t position, decltype(f(0))* = 0, typename enable_if<is_void<decltype(f(0))>::value>::type* = 0) const {
            f(pattern);
            return eContinueSearch;
        }
        template <typename F> EOnFind Exec(F& f, size_t pattern, size_t position, decltype(f(0))* = 0, typename enable_if<!is_void<decltype(f(0))>::value>::type* = 0) const {
            return f(pattern);
        }
        template <typename F> EOnFind Exec(F& f, size_t pattern, size_t position, decltype(f(0, 0))* = 0, typename enable_if<is_void<decltype(f(0, 0))>::value>::type* = 0) const {
            f(pattern, position);
            return eContinueSearch;
        }
        template <typename F> EOnFind Exec(F& f, size_t pattern, size_t position, decltype(f(0, 0))* = 0, typename enable_if<!is_void<decltype(f(0, 0))>::value>::type* = 0) const {
            return f(pattern, position);
        }
    };
};


END_NCBI_SCOPE

#endif /* UTIL___MULTIPATTERN_SEARCH__HPP */
