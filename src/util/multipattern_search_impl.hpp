#ifndef UTIL___MULTIPATTERN_SEARCH_IMPL__HPP
#define UTIL___MULTIPATTERN_SEARCH_IMPL__HPP

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
* Author: Sema
*
* File Description:
*
*/

#include <iostream>
#include <array>
#include <sstream>

BEGIN_NCBI_SCOPE

class CMultipatternSearch;
class CRegExFSA;

class CRegEx
{
public:
    enum EType {
        eTypeNone = 0,
        eTypeStart = 1,
        eTypeNoWord = 2,
        eTypeWord = 4,
        eTypeStop = 8,
        eTypeToNoWord = 16,
        eTypeToWord = 32,
        eTypeToStop = 64,
        eTypePass = eTypeStart | eTypeNoWord | eTypeWord | eTypeStop,
        eTypeCheck = eTypeToNoWord | eTypeToWord | eTypeToStop
    };
    // if string atarts with "/", treat it as RefEx and ignore flags
    // otherwise, treat it as plain text search with flags
    CRegEx(const char* s, CMultipatternSearch::TFlags f = 0) : m_Str(s), m_Flag(f) { x_Parse(); }
    CRegEx(const string& s, CMultipatternSearch::TFlags f = 0) : m_Str(s), m_Flag(f) { x_Parse(); }
    operator bool() const { return m_RegX != 0; }
    static bool IsWordCharacter(unsigned char c) { return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_'; }

protected:

    // inner classes
    struct CRegX // root for other RegX types; also an empty RegX
    {
        virtual ~CRegX() {}
        virtual operator bool() { return true; }
        virtual void SetCaseInsensitive() {}
        virtual bool IsCaseInsensitive() const = 0;
        virtual bool IsAssert() const { return false; }
        virtual void Print(ostream& out, size_t off) const = 0;
        virtual void Render(CRegExFSA& fsa, size_t from, size_t to) const = 0;
        static void PrintOffset(ostream& out, size_t off) { for (size_t n = 0; n < off; n++) out << ' '; }
        static void DummyTrans(CRegExFSA& fsa, size_t x, unsigned char t);
    };

    struct CRegXEmpty : public CRegX  // /()/
    {
        operator bool() { return false; }
        bool IsCaseInsensitive() const { return true; }
        void Print(ostream& out, size_t off) const { PrintOffset(out, off); out << "<empty>\n"; }
        void Render(CRegExFSA& fsa, size_t from, size_t to) const;
    };

    struct CRegXChar : public CRegX  // /a/
    {
        CRegXChar(char c, bool neg = false) : m_Neg(neg) { m_Set.insert(c); }
        CRegXChar(const set<unsigned char>& t, bool neg = false) : m_Neg(neg), m_Set(t) {}
        void Set(const set<unsigned char>& t) { m_Set = t; }
        void SetCaseInsensitive();
        bool IsCaseInsensitive() const;
        void Print(ostream& out, size_t off) const;
        void Render(CRegExFSA& fsa, size_t from, size_t to) const;
        bool m_Neg;
        set<unsigned char> m_Set;
    };

    struct CRegXTerm : public CRegX  // /a?/ /a+/ /a*/ /a{1,2}/
    {
        CRegXTerm(unique_ptr<CRegX>& x, unsigned int min, unsigned int max, bool lazy = false) : m_RegX(move(x)), m_Min(min), m_Max(max), m_Lazy(lazy) {}
        void SetCaseInsensitive() { m_RegX->SetCaseInsensitive(); }
        bool IsCaseInsensitive() const { return m_RegX->IsCaseInsensitive(); }
        void Print(ostream& out, size_t off) const;
        void Render(CRegExFSA& fsa, size_t from, size_t to) const;
        unique_ptr<CRegX> m_RegX;
        unsigned int m_Min;
        unsigned int m_Max;
        bool m_Lazy;
    };

    struct CRegXConcat : public CRegX  // /abc/
    {
        CRegXConcat() {}
        CRegXConcat(vector<unique_ptr<CRegX> >& v) : m_Vec(move(v)) {}
        void SetCaseInsensitive() { for (size_t n = 0; n < m_Vec.size(); n++) m_Vec[n]->SetCaseInsensitive(); }
        bool IsCaseInsensitive() const { for (size_t n = 0; n < m_Vec.size(); n++) if (!m_Vec[n]->IsCaseInsensitive()) return false; return true; }
        void Print(ostream& out, size_t off) const { PrintOffset(out, off); out << "<concat>\n"; for (size_t n = 0; n < m_Vec.size(); n++) m_Vec[n]->Print(out, off + 2); }
        void Render(CRegExFSA& fsa, size_t from, size_t to) const;
        vector<unique_ptr<CRegX> > m_Vec;
    };

    struct CRegXSelect : public CRegX  // /a|b|c/
    {
        CRegXSelect() {}
        CRegXSelect(vector<unique_ptr<CRegX> >& v) : m_Vec(move(v)) {}
        void SetCaseInsensitive() { for (size_t n = 0; n < m_Vec.size(); n++) m_Vec[n]->SetCaseInsensitive(); }
        bool IsCaseInsensitive() const { for (size_t n = 0; n < m_Vec.size(); n++) if (!m_Vec[n]->IsCaseInsensitive()) return false; return true; }
        void Print(ostream& out, size_t off) const { PrintOffset(out, off); out << "<select>\n"; for (size_t n = 0; n < m_Vec.size(); n++) m_Vec[n]->Print(out, off + 2); }
        void Render(CRegExFSA& fsa, size_t from, size_t to) const;
        vector<unique_ptr<CRegX> > m_Vec;
    };

    struct CRegXAssert : public CRegX  // /^$\b\B(?=)(?!)/
    {
        enum EAssert {
            eAssertNone = 0,
            eAssertBegin,         // ^
            eAssertEnd,           // $
            eAssertWord,          // \b
            eAssertWordNeg,       // \B
            eAssertLookAhead,     // (?=...) no FSM
            eAssertLookAheadNeg,  // (?!...) no FSM
            eAssertLookBack,      // (?<=...) will not support
            eAssertLookBackNeg    // (?<!...) will not support
        };
        CRegXAssert(EAssert a) : m_Assert(a) {}
        CRegXAssert(EAssert a, unique_ptr<CRegX>& x) : m_Assert(a), m_RegX(move(x)) {}
        void SetCaseInsensitive() { if (m_RegX) m_RegX->SetCaseInsensitive(); }
        bool IsCaseInsensitive() const { return m_RegX ? m_RegX->IsCaseInsensitive() : true; }
        virtual bool IsAssert() const { return true; }
        void Print(ostream& out, size_t off) const;
        void Render(CRegExFSA& fsa, size_t from, size_t to) const;
        EAssert m_Assert;
        unique_ptr<CRegX> m_RegX;
    };

    struct CRegXBackRef : public CRegX  // /\1/
    {
        CRegXBackRef(unsigned int n) : m_Num(n) {}
        bool IsCaseInsensitive() const { return false; }
        void Print(ostream& out, size_t off) const { PrintOffset(out, off); out << "<bkref>\t" << m_Num << "\n"; }
        void Render(CRegExFSA& fsa, size_t from, size_t to) const { throw string("back reference"); }
        unsigned int m_Num;
    };

    void x_Print(ostream& out) const;
    void x_Parse();
    void x_ParseOptions();
    unique_ptr<CRegX> x_ParseSelect();
    unique_ptr<CRegX> x_ParseConcat();
    unique_ptr<CRegX> x_ParseTerm();
    unique_ptr<CRegX> x_ParseAtom();
    unique_ptr<CRegX> x_ParsePlain();
    bool x_ParseRepeat(int& from, int& to, bool& lazy);
    void x_ParseSquare(set<unsigned char>& t);
    unsigned char x_ParseEscape();
    int x_ParseHex(size_t len = 0);
    int x_ParseDec(size_t len = 0);
    void x_ThrowUnexpectedCharacter();
    void x_ThrowUnexpectedEndOfLine();
    void x_ThrowError(const string msg, size_t pos, size_t len);
    void x_Consume(char c);

    string m_Str;
    string m_Err;
    size_t m_Cur;
    CMultipatternSearch::TFlags m_Flag;
    bool m_Unsupported; // RegEx is syntatically correct, but not supported: lookahead, back reference
    unique_ptr<CRegX> m_RegX;
    friend ostream& operator<<(ostream&, const CRegEx&);
    friend class CRegExFSA;
};


class CRegExFSA
{
    struct CRegExState
    {
        int m_Type;
        array<size_t, 256> m_Trans;
        set<size_t> m_Short;
        set<size_t> m_Emit;
        set<size_t> m_Forward1;
        set<size_t> m_Forward2;
        set<size_t> m_Forward3;
        CRegExState(unsigned char t = CRegEx::eTypePass) : m_Type(t), m_Trans({ { // may be faster than array::fill()
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0,  0, 0, 0, 0, 0, 0, 0, 0
            } }) {}
        void Trans(unsigned char c, size_t n) { m_Trans[c] = n; };
        void Short(size_t n) { m_Short.insert(n); };
        void Emit(size_t n) { m_Emit.insert(n); };
    };
    struct THasher {
        size_t operator()(const vector<size_t>& s) {
            size_t ret = 0;
            for (auto e : s) ret ^= hash<size_t>()((e));
            return ret;
        }
    };
    typedef vector<unique_ptr<CRegExState>> TStates;
    typedef pair<size_t, CRegEx::EType> TNode;
    typedef vector<TNode> TNodeSet;
    typedef map<TNodeSet, size_t> TNodeSetMap;
    typedef vector<TNodeSet> TNodeSetList;
    typedef array<vector<size_t>, 4> TScratch;

    CRegExFSA();
    size_t AddState(unsigned char t = CRegEx::eTypePass) { size_t n = m_States.size(); m_States.push_back(unique_ptr<CRegExState>(new CRegExState(t))); return n; }
    void Trans(size_t x, unsigned char c, size_t y) { m_States[x]->Trans(c, y); };
    void Short(size_t x, size_t y) { m_States[x]->Short(y); }
    void Emit(size_t x, size_t n) { m_States[x]->Emit(n); }
    void Create(const CRegEx& rx, size_t emit);
    void Add(const CRegEx& rx);
    void Add(const vector<unique_ptr<CRegEx>>& v);
    void Merge(unique_ptr<CRegExFSA> fsa); // fsa will be consumed
    void GenerateDotGraph(ostream& out) const;
    void GenerateSourceCode(ostream& out) const;
    void GenerateArrayMapData(ostream& out) const;
    void Refine();  // build DSA from NSA
    static size_t Collect(TScratch& VV, CRegEx::EType t, TStates& src, TStates& dest, TNodeSetMap& NM, TNodeSetList& NL, TNodeSet& NS, TScratch& HH);
    static void Extend(size_t x, unsigned char c, TStates& src, TStates& dest, TNodeSetMap& NM, TNodeSetList& NL, TNodeSet& NS, TScratch& VV, TScratch& HH);
    static void Push(size_t x, vector<size_t>& v, vector<size_t>& h) {  // performance critical, keep it inline
        size_t i;
        for (i = 0; i < h.size(); ++i) {
            if (h[i] == x) return;
            if (h[i] > x) break;
        }
        v.push_back(x);
        h.push_back(x);
        for (size_t j = h.size() - 1; j > i; --j) h[j] = h[j - 1];
        h[i] = x;
    }
    static bool In(size_t x, vector<size_t>& h) {  // performance critical, keep it inline
        size_t i;
        for (i = 0; i < h.size(); ++i) {
            if (h[i] == x) return true;
            if (h[i] > x) break;
        }
        return false;
    }
    TStates m_States;
    vector<string> m_Str;
    friend class CRegEx;
    friend class CMultipatternSearch;
};


END_NCBI_SCOPE

#endif /* UTIL___MULTIPATTERN_SEARCH_IMPL__HPP */
