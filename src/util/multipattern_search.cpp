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

#include <ncbi_pch.hpp>
#include <util/multipattern_search.hpp>
#include "multipattern_search_impl.hpp"

BEGIN_NCBI_SCOPE

CMultipatternSearch::CMultipatternSearch() : m_FSM(new CRegExFSA) {}
CMultipatternSearch::~CMultipatternSearch() {}

void CMultipatternSearch::AddPattern(const char* s, TFlags f) { m_FSM->Add(CRegEx(s, f)); }

void CMultipatternSearch::AddPatterns(const vector<string>& patterns)
{
    vector<unique_ptr<CRegEx>> v;
    for (const string& s : patterns) {
        v.push_back(unique_ptr<CRegEx>(new CRegEx(s)));
    }
    m_FSM->Add(v);
}

void CMultipatternSearch::AddPatterns(const vector<pair<string, TFlags>>& patterns)
{
    vector<unique_ptr<CRegEx>> v;
    for (auto& p : patterns) {
        v.push_back(unique_ptr<CRegEx>(new CRegEx(p.first, p.second)));
    }
    m_FSM->Add(v);
}

void CMultipatternSearch::GenerateDotGraph(ostream& out) const { m_FSM->GenerateDotGraph(out); }

void CMultipatternSearch::GenerateArrayMapData(ostream& out) const { m_FSM->GenerateArrayMapData(out); }

void CMultipatternSearch::GenerateSourceCode(ostream& out) const { m_FSM->GenerateSourceCode(out); }

string CMultipatternSearch::QuoteString(const string& str)
{
    string out;
    for (auto c : str) {
        switch (c) {
            case ' ':
                out += "\\s+";
                break;
            case '^':
            case '$':
            case '(':
            case ')':
            case '[':
            case ']':
            case '\\':
            case '/':
            case '?':
            case '*':
            case '+':
            case '.':
            case '|':
                out += '\\';
                // there is no break here!
            default:
                out += c;
        }
    }
    return out;
}


void CMultipatternSearch::Search(const char* input, VoidCall1 report) const
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(input);
    size_t state = 1;

    set<size_t>& emit = m_FSM->m_States[state]->m_Emit;
    for (auto e : emit) {
        report(e);
    }
    while (true) {
        state = m_FSM->m_States[state]->m_Trans[*p];
        set<size_t>& emit = m_FSM->m_States[state]->m_Emit;
        for (auto e : emit) {
            report(e);
        }
        if (!*p) {
            return;
        }
        ++p;
    }
}


void CMultipatternSearch::Search(const char* input, VoidCall2 report) const
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(input);
    size_t state = 1;

    set<size_t>& emit = m_FSM->m_States[state]->m_Emit;
    for (auto e : emit) {
        report(e, p - (const unsigned char*)input);
    }
    while (true) {
        state = m_FSM->m_States[state]->m_Trans[*p];
        set<size_t>& emit = m_FSM->m_States[state]->m_Emit;
        for (auto e : emit) {
            report(e, p - (const unsigned char*)input);
        }
        if (!*p) {
            return;
        }
        ++p;
    }
}


void CMultipatternSearch::Search(const char* input, BoolCall1 report) const
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(input);
    size_t state = 1;

    set<size_t>& emit = m_FSM->m_States[state]->m_Emit;
    for (auto e : emit) {
        if (report(e)) {
            return;
        }
    }
    while (true) {
        state = m_FSM->m_States[state]->m_Trans[*p];
        set<size_t>& emit = m_FSM->m_States[state]->m_Emit;
        for (auto e : emit) {
            if (report(e)) {
                return;
            }
        }
        if (!*p) {
            return;
        }
        ++p;
    }
}


void CMultipatternSearch::Search(const char* input, BoolCall2 report) const
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(input);
    size_t state = 1;

    set<size_t>& emit = m_FSM->m_States[state]->m_Emit;
    for (auto e : emit) {
        if (report(e, p - (const unsigned char*)input)) {
            return;
        }
    }
    while (true) {
        state = m_FSM->m_States[state]->m_Trans[*p];
        set<size_t>& emit = m_FSM->m_States[state]->m_Emit;
        for (auto e : emit) {
            if (report(e, p - (const unsigned char*)input)) {
                return;
            }
        }
        if (!*p) {
            return;
        }
        ++p;
    }
}


////////////////////////////////////////////////////////////////

void CMultipatternSearch::Search(const char* input, const size_t* states, const bool* emit, const map<size_t, vector<size_t>>& hits, VoidCall1 report)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(input);
    size_t state = 0;

    if (emit[state]) {
        for (auto e : hits.at(state)) {
            report(e);
        }
    }
    while (true) {
        state = states[state * 256 + *p];
        if (emit[state]) {
            for (auto e : hits.at(state)) {
                report(e);
            }
        }
        if (!*p) {
            return;
        }
        ++p;
    }
}


void CMultipatternSearch::Search(const char* input, const size_t* states, const bool* emit, const map<size_t, vector<size_t>>& hits, VoidCall2 report)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(input);
    size_t state = 0;

    if (emit[state]) {
        for (auto e : hits.at(state)) {
            report(e, p - (const unsigned char*)input);
        }
    }
    while (true) {
        state = states[state * 256 + *p];
        if (emit[state]) {
            for (auto e : hits.at(state)) {
                report(e, p - (const unsigned char*)input);
            }
        }
        if (!*p) {
            return;
        }
        ++p;
    }
}


void CMultipatternSearch::Search(const char* input, const size_t* states, const bool* emit, const map<size_t, vector<size_t>>& hits, BoolCall1 report)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(input);
    size_t state = 0;

    if (emit[state]) {
        for (auto e : hits.at(state)) {
            if (report(e)) {
                return;
            }
        }
    }
    while (true) {
        state = states[state * 256 + *p];
        if (emit[state]) {
            for (auto e : hits.at(state)) {
                if (report(e)) {
                    return;
                }
            }
        }
        if (!*p) {
            return;
        }
        ++p;
    }
}


void CMultipatternSearch::Search(const char* input, const size_t* states, const bool* emit, const map<size_t, vector<size_t>>& hits, BoolCall2 report)
{
    const unsigned char* p = reinterpret_cast<const unsigned char*>(input);
    size_t state = 0;

    if (emit[state]) {
        for (auto e : hits.at(state)) {
            if (report(e, p - (const unsigned char*)input)) {
                return;
            }
        }
    }
    while (true) {
        state = states[state * 256 + *p];
        if (emit[state]) {
            for (auto e : hits.at(state)) {
                if (report(e, p - (const unsigned char*)input)) {
                    return;
                }
            }
        }
        if (!*p) {
            return;
        }
        ++p;
    }
}


////////////////////////////////////////////////////////////////

void CRegEx::x_ThrowUnexpectedEndOfLine()
{
    throw (string) "unexpected end of string";
}


void CRegEx::x_ThrowUnexpectedCharacter()
{
    ostringstream oss;
    oss << "unexpected character "
        << (m_Str[m_Cur] == '\'' ? '\"' : '\'') << m_Str[m_Cur] << (m_Str[m_Cur] == '\'' ? '\"' : '\'')
        << " in position " << (m_Cur + 1);
    throw oss.str();
}


void CRegEx::x_ThrowError(const string msg, size_t pos, size_t len)
{
    ostringstream oss;
    oss << msg << " \'" << m_Str.substr(pos, len) << "\' in position " << (pos + 1);
    throw oss.str();
}


void CRegEx::x_Consume(char c)
{
    if (m_Cur >= m_Str.length()) {
        x_ThrowUnexpectedEndOfLine();
    }
    if (m_Str[m_Cur] != c) {
        x_ThrowUnexpectedCharacter();
    }
    m_Cur++;
}


void CRegEx::x_Parse()
{
    m_Cur = 0;
    try {
        if (m_Cur < m_Str.length() && m_Str[m_Cur] == '/') {
            m_Cur++;
            m_RegX = x_ParseSelect();
            x_Consume('/');
            x_ParseOptions();
        }
        else {
            m_RegX = x_ParsePlain();
            if (m_Flag & CMultipatternSearch::fNoCase) {
                m_RegX->SetCaseInsensitive();
            }
        }
    }
    catch (const string& s) {
        m_Err = s;
        m_RegX.reset();
    }
}


unique_ptr<CRegEx::CRegX> CRegEx::x_ParsePlain()  // plain string
{
    vector<unique_ptr<CRegX> > V;
    if (m_Flag & CMultipatternSearch::fBeginString) {
        V.push_back(unique_ptr<CRegX>(new CRegXAssert(CRegXAssert::eAssertBegin)));
    }
    else if (m_Flag & CMultipatternSearch::fBeginWord) {
        V.push_back(unique_ptr<CRegX>(new CRegXAssert(CRegXAssert::eAssertWord)));
    }
    for (size_t i = 0; i < m_Str.length(); i++) {
        V.push_back(unique_ptr<CRegX>(new CRegXChar(m_Str[i])));
    }
    if (m_Flag & CMultipatternSearch::fEndString) {
        V.push_back(unique_ptr<CRegX>(new CRegXAssert(CRegXAssert::eAssertEnd)));
    }
    else if (m_Flag & CMultipatternSearch::fEndWord) {
        V.push_back(unique_ptr<CRegX>(new CRegXAssert(CRegXAssert::eAssertWord)));
    }
    if (V.empty()) {
        return unique_ptr<CRegX>(new CRegXEmpty());
    }
    if (V.size() == 1) {
        return move(V[0]);
    }
    return unique_ptr<CRegX>(new CRegXConcat(V));
}


unique_ptr<CRegEx::CRegX> CRegEx::x_ParseSelect()  // e.g.: /a|b|c/
{
    vector<unique_ptr<CRegX> > V;
    while (unique_ptr<CRegX> x = x_ParseConcat()) {
        V.push_back(move(x));
        if (m_Cur >= m_Str.length() || m_Str[m_Cur] != '|') {
            break;
        }
        m_Cur++;
    }
    if (V.empty()) {
        return unique_ptr<CRegX>(new CRegXEmpty());
    }
    if (V.size() == 1) {
        return move(V[0]);
    }
    return unique_ptr<CRegX>(new CRegXSelect(V));
}


unique_ptr<CRegEx::CRegX> CRegEx::x_ParseConcat()  // e.g.: /abc/
{
    vector<unique_ptr<CRegX> > V;
    while (unique_ptr<CRegX> x = x_ParseTerm()) {
        if (*x) {
            V.push_back(move(x));
        }
        if (m_Cur >= m_Str.length() || m_Str[m_Cur] == '|' || m_Str[m_Cur] == ')' || m_Str[m_Cur] == '/') {
            break;
        }
    }
    if (V.empty()) {
        return unique_ptr<CRegX>(new CRegXEmpty());
    }
    if (V.size() == 1) {
        return move(V[0]);
    }
    return unique_ptr<CRegX>(new CRegXConcat(V));
}


unique_ptr<CRegEx::CRegX> CRegEx::x_ParseTerm()  // e.g.: /a+/ /a?/ /a{2,3}/
{
    if (m_Cur >= m_Str.length()) {
        return 0;
    }
    int from, to;
    bool lazy;
    size_t k = m_Cur;
    if (x_ParseRepeat(from, to, lazy)) {
        x_ThrowError("nothing to repeat:", k, m_Cur - k);
    }
    unique_ptr<CRegX> x = x_ParseAtom();
    k = m_Cur;
    if (x && !x->IsAssert() && x_ParseRepeat(from, to, lazy)) {
        if (to && from > to) {
            x_ThrowError("numbers out of order:", k, m_Cur - k);
        }
        return unique_ptr<CRegX>(new CRegXTerm(x, from, to, lazy));
    }
    return x;
}


bool CRegEx::x_ParseRepeat(int& from, int& to, bool& lazy)
{
    if (m_Cur >= m_Str.length()) {
        return false;
    }
    switch (m_Str[m_Cur]) {
    case '?':
        m_Cur++;
        from = 0;
        to = 1;
        break;
    case '+':
        m_Cur++;
        from = 1;
        to = 0;
        break;
    case '*':
        m_Cur++;
        from = 0;
        to = 0;
        break;
    case '{':   // {3} {3,} {,5} {3,5}, but not {} {,}
    {
        size_t k = m_Cur;
        m_Cur++;
        from = x_ParseDec();
        if (from >= 0 && m_Cur < m_Str.length() && m_Str[m_Cur] == '}') {
            m_Cur++;
            to = from;
            break;
        }
        else if (m_Cur < m_Str.length() && m_Str[m_Cur] == ',') {
            m_Cur++;
            to = x_ParseDec();
            if ((from >= 0 || to >= 0) && m_Cur < m_Str.length() && m_Str[m_Cur] == '}') {
                m_Cur++;
                from = from < 0 ? 0 : from;
                to = to < 0 ? 0 : to;
                break;
            }
        }
        m_Cur = k;
        return false;
    }
    default:
        return false;
    }
    lazy = false;
    if (m_Cur < m_Str.length() && m_Str[m_Cur] == '?') {
        m_Cur++;
        lazy = true;
    }
    return true;
}


static inline void InsertDigit(set<unsigned char>& t)
{
    for (unsigned char c = '0'; c <= '9'; c++) {
        t.insert(c);
    }
}


static inline void InsertNoDigit(set<unsigned char>& t)
{
    for (unsigned char c = 1; c <= 255; c++) {
        if (c < '0' || c > '9') {
            t.insert(c);
        }
    }
}


static inline void InsertAlpha(set<unsigned char>& t)
{
    for (unsigned char c = '0'; c <= '9'; c++) {
        t.insert(c);
    }
    for (unsigned char c = 'A'; c <= 'Z'; c++) {
        t.insert(c);
    }
    for (unsigned char c = 'a'; c <= 'z'; c++) {
        t.insert(c);
    }
    t.insert('_');
}


static inline void InsertNoAlpha(set<unsigned  char>& t)
{
    for (unsigned char c = 1; c <= 255; c++) {
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
            continue;
        }
        t.insert(c);
    }
}


static inline void InsertSpace(set<unsigned char>& t)
{
    t.insert(' ');
    t.insert('\f');
    t.insert('\n');
    t.insert('\r');
    t.insert('\t');
    t.insert('\v');
}


static inline void InsertNoSpace(set<unsigned char>& t)
{
    for (unsigned char c = 1; c <= 255; c++) {
        if (c != ' ' && c != '\f' && c != '\n' && c != '\r' && c != '\t' && c != '\v') {
            t.insert(c);
        }
    }
}


unique_ptr<CRegEx::CRegX> CRegEx::x_ParseAtom()  // single character or an expression in braces
{
    switch (m_Str[m_Cur]) {
    case '|':
    case ')':
    case '/':
        return 0;
    case '.':
    {
        m_Cur++;
        set<unsigned char> t;
        for (int c = 1; c < 256; c++) {
            t.insert((unsigned char)c);
        }
        return unique_ptr<CRegX>(new CRegXChar(t));
    }
    case '^':
        m_Cur++;
        return unique_ptr<CRegX>(new CRegXAssert(CRegXAssert::eAssertBegin));
    case '$':
        m_Cur++;
        return unique_ptr<CRegX>(new CRegXAssert(CRegXAssert::eAssertEnd));
    case '(':
    {
        m_Cur++;
        CRegXAssert::EAssert assert = CRegXAssert::eAssertNone;
        if (m_Cur + 2 < m_Str.length() && m_Str[m_Cur] == '?') {
            if (m_Str[m_Cur + 1] == ':') {  // non-capturing parentheses: (?:...)
                m_Cur += 2;
            }
            else if (m_Str[m_Cur + 1] == '=') {  // lookahead is unsupported: (?=...)
                assert = CRegXAssert::eAssertLookAhead;
                m_Cur += 2;
            }
            else if (m_Str[m_Cur + 1] == '!') {  // lookahead is unsupported: (?!...)
                assert = CRegXAssert::eAssertLookAheadNeg;
                m_Cur += 2;
            }
            else if (m_Str[m_Cur + 1] == '<' && m_Cur + 2 < m_Str.length()) {
                if (m_Str[m_Cur + 2] == '=') {  // lookahead is unsupported: (?<=...)
                    assert = CRegXAssert::eAssertLookBack;
                    m_Cur += 3;
                }
                else if (m_Str[m_Cur + 2] == '!') {  // lookahead is unsupported: (?<!...)
                    assert = CRegXAssert::eAssertLookBackNeg;
                    m_Cur += 3;
                }
            }
        }
        unique_ptr<CRegX> x = x_ParseSelect();
        x_Consume(')');
        if (assert != CRegXAssert::eAssertNone) {
            m_Unsupported = true;
            return unique_ptr<CRegX>(new CRegXAssert(assert, x));
        }
        return x;
    }
    case '[':
    {
        m_Cur++;
        bool neg = false;
        set<unsigned char> t;
        if (m_Cur + 1 < m_Str.length() && m_Str[m_Cur] == '^') {
            neg = true;
            m_Cur++;
        }
        x_ParseSquare(t);
        x_Consume(']');
        return unique_ptr<CRegX>(new CRegXChar(t, neg));
    }
    case '\\':
        m_Cur++;
        if (m_Cur >= m_Str.length()) {
            x_ThrowUnexpectedEndOfLine();
        }
        switch (m_Str[m_Cur]) {
        case 'b':
            m_Cur++;
            return unique_ptr<CRegX>(new CRegXAssert(CRegXAssert::eAssertWord));
        case 'B':
            m_Cur++;
            return unique_ptr<CRegX>(new CRegXAssert(CRegXAssert::eAssertWordNeg));
        case 'd':
        case 'D':
        {
            set<unsigned char> t;
            InsertDigit(t);
            return unique_ptr<CRegX>(new CRegXChar(t, m_Str[m_Cur++] == 'D'));
        }
        case 'w':
        case 'W':
        {
            set<unsigned char> t;
            InsertAlpha(t);
            return unique_ptr<CRegX>(new CRegXChar(t, m_Str[m_Cur++] == 'W'));
        }
        case 's':
        case 'S':
        {
            set<unsigned char> t;
            InsertSpace(t);
            return unique_ptr<CRegX>(new CRegXChar(t, m_Str[m_Cur++] == 'S'));
        }
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            m_Unsupported = true;
            return unique_ptr<CRegX>(new CRegXBackRef(m_Str[m_Cur++] - '0'));
        default:
        {
            unsigned char c = x_ParseEscape();
            return unique_ptr<CRegX>(new CRegXChar(c));
        }
        }
    default:
        return unique_ptr<CRegX>(new CRegXChar(m_Str[m_Cur++]));
    }
}


void CRegEx::x_ParseSquare(set<unsigned char>& t)
{
    unsigned char range = 0; // 0: initial; 1: valid begining; 2: dash; 3: valid ending;
    unsigned char curr = 0;
    unsigned char prev = 0;
    size_t pos = 0;
    size_t prev_pos = 0;

    // Commented out Perl syntax: closing square bracket is treated as regular character if in the first position
    //if (m_Cur < m_Str.length() && m_Str[m_Cur] == ']') {
    //    range = 1;
    //    prev = m_Str[m_Cur++];
    //    t.insert(prev);
    //}
    while (m_Cur < m_Str.length()) {
        pos = m_Cur;
        switch (m_Str[m_Cur]) {
        case ']':
            if (range == 2) {
                t.insert('-');
            }
            return;
        case '-':
            curr = '-';
            range++;
            break;
        case '\\':
            m_Cur++;
            if (m_Cur >= m_Str.length()) {
                x_ThrowUnexpectedEndOfLine();
            }
            switch (m_Str[m_Cur]) {
            case 'd':
                InsertDigit(t);
                if (range == 2) {
                    t.insert('-');
                }
                range = 0;
                break;
            case 'D':
                InsertNoDigit(t);
                if (range == 2) {
                    t.insert('-');
                }
                range = 0;
                break;
            case 'w':
                InsertAlpha(t);
                if (range == 2) {
                    t.insert('-');
                }
                range = 0;
                break;
            case 'W':
                InsertNoAlpha(t);
                if (range == 2) {
                    t.insert('-');
                }
                range = 0;
                break;
            case 's':
                InsertSpace(t);
                if (range == 2) {
                    t.insert('-');
                }
                range = 0;
                break;
            case 'S':
                InsertNoSpace(t);
                if (range == 2) {
                    t.insert('-');
                }
                range = 0;
                break;
            default:
                curr = x_ParseEscape();
                m_Cur--;
                if (range != 1) {
                    range++;
                }
            }
            break;
        default:
            curr = m_Str[m_Cur];
            if (range != 1) {
                range++;
            }
        }
        if (range == 3) {
            if (prev > curr) {
                x_ThrowError("invalid range:", prev_pos, m_Cur - prev_pos + 1);
            }
            for (unsigned int n = prev; n <= curr; n++) {
                t.insert((unsigned char)n);
            }
            range = 0;
        }
        if (range == 1) {
            t.insert(curr);
            prev = curr;
            prev_pos = pos;
        }
        m_Cur++;
    }
}


unsigned char CRegEx::x_ParseEscape()
{
    switch (m_Str[m_Cur]) {
    case '0':
        m_Cur++;
        return 0;
    case 'b':
        m_Cur++;
        return '\b';
    case 'c':   // control letter if followed by letter, otherwise just 'c'
        m_Cur++;
        if (m_Cur < m_Str.length()) {
            if (m_Str[m_Cur] >= 'A' && m_Str[m_Cur] <= 'Z') {
                return (unsigned char)(m_Str[m_Cur++] - 'A' + 1);
            }
            if (m_Str[m_Cur] >= 'a' && m_Str[m_Cur] <= 'z') {
                return (unsigned char)(m_Str[m_Cur++] - 'a' + 1);
            }
        }
        return 'c';
    case 'f':
        m_Cur++;
        return '\f';
    case 'n':
        m_Cur++;
        return '\n';
    case 'r':
        m_Cur++;
        return '\r';
    case 't':
        m_Cur++;
        return '\t';
    case 'v':
        m_Cur++;
        return '\v';
    case 'x':
        m_Cur++;
        if (m_Cur < m_Str.length()) {
            int n = x_ParseHex(2);
            if (n >= 0) {
                return (unsigned char)n;
            }
        }
        return 'x';
    case 'u':
        m_Cur++;
        if (m_Cur + 1 < m_Str.length() && m_Str[m_Cur] == '{') {    // \u{XXXX}
            size_t k = m_Cur;
            m_Cur++;
            int n = x_ParseHex(4);
            if (n >= 0 && m_Cur < m_Str.length() && m_Str[m_Cur] == '}') {
                m_Cur++;
                if (n > 255) {
                    m_Unsupported = true;
                    return 0;
                }
                return (unsigned char)n;
            }
            m_Cur = k;
        }
        else if (m_Cur < m_Str.length()) {  // \uXXXX
            int n = x_ParseHex(4);
            if (n >= 0) {
                if (n > 255) {
                    m_Unsupported = true;
                    return 0;
                }
                return (unsigned char)n;
            }
        }
        return 'u';
    default:
        return m_Str[m_Cur++];
    }
}


int CRegEx::x_ParseHex(size_t len)
{
    int x = 0;
    for (size_t n = 0; n < len || !len; n++) {
        if (m_Cur < m_Str.length()) {
            char& c = m_Str[m_Cur];
            if (c >= '0' && c <= '9') {
                x = x * 16 + c - '0';
            }
            else if (c >= 'A' && c <= 'F') {
                x = x * 16 + 10 + c - 'A';
            }
            else if (c >= 'a' && c <= 'f') {
                x = x * 16 + 10 + c - 'a';
            }
            else {
                return n ? x : -1;
            }
            m_Cur++;
        }
        else {
            return n ? x : -1;
        }
    }
    return x;
}


int CRegEx::x_ParseDec(size_t len)
{
    int x = 0;
    for (size_t n = 0; n < len || !len; n++) {
        if (m_Cur < m_Str.length()) {
            char& c = m_Str[m_Cur];
            if (c >= '0' && c <= '9') {
                x = x * 10 + c - '0';
            }
            else {
                return n ? x : -1;
            }
            m_Cur++;
        }
        else {
            return n ? x : -1;
        }
    }
    return x;
}


void CRegEx::x_ParseOptions()   // options after the final slash, e.g.: /***/i
{
    for (; m_Cur < m_Str.length(); m_Cur++) {
        switch (m_Str[m_Cur]) {
        case 'i':
            m_RegX->SetCaseInsensitive();
            break;
        case 'g': // [ignored] global
        case 'm': // [ignored] multi-line
        case 'u': // [ignored] unicode
        case 'y': // [ignored] sticky
            break;
        default:
            x_ThrowUnexpectedCharacter();
        }
    }
}

//  Printing

ostream& operator<<(ostream& ostr, const CRegEx& regex)
{
    regex.x_Print(ostr);
    return ostr;
}


void CRegEx::x_Print(ostream& ostr) const
{
    ostr << "<<RegEx>> " << m_Str << "\n";
    if (m_Err.length()) {
        ostr << "  <ERROR>\t" << m_Err << "\n";
    }
    else {
        m_RegX->Print(ostr, 2);
    }
}


void CRegEx::CRegXChar::Print(ostream& out, size_t off) const
{
    PrintOffset(out, off);
    out << (m_Neg ? "<char>!\t" : "<char>\t");
    if (m_Set.empty()) {
        out << "<empty>";
    }
    for (auto it = m_Set.begin(); it != m_Set.end(); it++) {
        switch (*it) {
        case 0:
            out << "\\0";
            break;
        case '\b':
            out << "\\b";
            break;
        case '\f':
            out << "\\f";
            break;
        case '\n':
            out << "\\n";
            break;
        case '\r':
            out << "\\r";
            break;
        case '\t':
            out << "\\t";
            break;
        case '\v':
            out << "\\v";
            break;
        default:
            out << *it;
        }
    }
    out << "\n";
}


void CRegEx::CRegXTerm::Print(ostream& out, size_t off) const
{
    PrintOffset(out, off);
    out << "<repeat>\t" << m_Min << " : ";
    if (m_Max) {
        out << m_Max;
    }
    else {
        out << "inf";
    }
    out << (m_Lazy ? " : lazy\n" : "\n");
    m_RegX->Print(out, off + 2);
}


void CRegEx::CRegXAssert::Print(ostream& out, size_t off) const
{
    static const string A[] = { "error", "beginning of string", "end of string", "word boundary", "not word boundary", "look ahead", "look ahead negative", "look back", "look back negative" };
    PrintOffset(out, off);
    out << "<assert>\t" << A[m_Assert] << "\n";
    if (m_RegX) {
        m_RegX->Print(out, off + 2);
    }
}


void CRegEx::CRegXChar::SetCaseInsensitive()
{
    for (unsigned char c = 'A'; c <= 'Z'; c++) {
        if (m_Set.find(c) != m_Set.end()) {
            m_Set.insert((unsigned char)(c + 32));
        }
        else if (m_Set.find((unsigned char)(c + 32)) != m_Set.end()) {
            m_Set.insert(c);
        }
    }
}


bool CRegEx::CRegXChar::IsCaseInsensitive() const
{
    for (unsigned char c = 'A'; c <= 'Z'; c++) {
        if ((m_Set.find(c) == m_Set.end()) != (m_Set.find((unsigned char)(c + 32)) == m_Set.end())) {
            return false;
        }
    }
    return true;
}


// Render

void CRegEx::CRegXEmpty::Render(CRegExFSA& fsa, size_t from, size_t to) const
{
    fsa.Short(from, to);
}


void CRegEx::CRegXChar::Render(CRegExFSA& fsa, size_t from, size_t to) const
{
    size_t x = fsa.AddState(eTypeStop | eTypeWord | eTypeNoWord);
    for (unsigned n = 1; n < 256; n++) {
        unsigned char c = (unsigned char)n;
        if ((m_Set.find(c) == m_Set.end()) == m_Neg) {
            fsa.Trans(from, c, x);
            fsa.Short(x, to);
        }
    }
}


void CRegEx::CRegXConcat::Render(CRegExFSA& fsa, size_t from, size_t to) const
{
    if (m_Vec.empty()) {
        fsa.Short(from, to);
        return;
    }
    size_t current = from;
    for (size_t n = 0; n < m_Vec.size(); n++) {
        size_t next = to;
        if (n < m_Vec.size() - 1) {
            next = fsa.AddState();
        }
        m_Vec[n]->Render(fsa, current, next);
        current = next;
    }
}


void CRegEx::CRegXSelect::Render(CRegExFSA& fsa, size_t from, size_t to) const
{
    if (m_Vec.empty()) {
        fsa.Short(from, to);
        return;
    }
    for (size_t n = 0; n < m_Vec.size(); n++) {
        size_t current = fsa.AddState();
        fsa.Short(from, current);
        m_Vec[n]->Render(fsa, current, to);
    }
}


void CRegEx::CRegXTerm::Render(CRegExFSA& fsa, size_t from, size_t to) const
{
    size_t current = from;
    size_t last = current;
    for (size_t n = 0; n < m_Min; n++) {
        size_t next = to;
        if (n + 1 < m_Min || n + 1 < m_Max) {
            next = fsa.AddState();
        }
        m_RegX->Render(fsa, current, next);
        last = current;
        current = next;
    }
    if (!m_Max) {
        if (!m_Min) {
            m_RegX->Render(fsa, from, to);
            fsa.Short(from, to);
        }
        fsa.Short(to, last);
        return;
    }
    for (size_t n = m_Min; n < m_Max; n++) {
        size_t next = to;
        if (n + 1 < m_Max) {
            next = fsa.AddState();
        }
        m_RegX->Render(fsa, current, next);
        fsa.Short(current, to);
        current = next;
    }
}


void CRegEx::CRegX::DummyTrans(CRegExFSA& fsa, size_t x, unsigned char t)
{
    size_t y;
    if (t & CRegEx::eTypeStop) {
        y = fsa.AddState(CRegEx::eTypeStop);
        fsa.Trans(x, 0, y);
    }
    if (t & CRegEx::eTypeWord) {
        y = fsa.AddState(CRegEx::eTypeWord);
        for (unsigned n = 1; n < 256; n++) {
            if (CRegEx::IsWordCharacter((unsigned char)n)) {
                fsa.Trans(x, (unsigned char)n, y);
            }
        }
    }
    if (t & CRegEx::eTypeNoWord) {
        y = fsa.AddState(CRegEx::eTypeNoWord);
        for (unsigned n = 1; n < 256; n++) {
            if (!CRegEx::IsWordCharacter((unsigned char)n)) {
                fsa.Trans(x, (unsigned char)n, y);
            }
        }
    }
}


void CRegEx::CRegXAssert::Render(CRegExFSA& fsa, size_t from, size_t to) const
{
    size_t x;
    switch (m_Assert) {
        case eAssertBegin:    // ^
            x = fsa.AddState(eTypeStart);
            fsa.Short(from, x);
            fsa.Short(x, to);
            return;
        case eAssertEnd:      // $
            x = fsa.AddState(eTypePass | eTypeToStop);
            DummyTrans(fsa, x, eTypeStop);
            fsa.Short(from, x);
            fsa.Short(x, to);
            return;
        case eAssertWord:     // \b
            x = fsa.AddState(eTypeStart | eTypeNoWord | eTypeToWord);
            DummyTrans(fsa, x, eTypeWord);
            fsa.Short(from, x);
            fsa.Short(x, to);
            x = fsa.AddState(eTypeWord | eTypeToNoWord | eTypeToStop);
            DummyTrans(fsa, x, eTypeNoWord);
            DummyTrans(fsa, x, eTypeStop);
            fsa.Short(from, x);
            fsa.Short(x, to);
            return;
        case eAssertWordNeg:  // \B
            x = fsa.AddState(eTypeStart | eTypeNoWord | eTypeToNoWord | eTypeToStop);
            DummyTrans(fsa, x, eTypeNoWord);
            DummyTrans(fsa, x, eTypeStop);
            fsa.Short(from, x);
            fsa.Short(x, to);
            x = fsa.AddState(eTypeWord | eTypeToWord);
            DummyTrans(fsa, x, eTypeWord);
            fsa.Short(from, x);
            fsa.Short(x, to);
            return;
        case eAssertLookAhead:
            throw string("(?=...) - lookahead is not supported");
        case eAssertLookAheadNeg:
            throw string("(?!...) - lookahead is not supported");
        case eAssertLookBack:
            throw string("(?<=...) - lookback is not supported");
        case eAssertLookBackNeg:
            throw string("(?<!...) - lookback is not supported");
        default:
            return;
    }
}


CRegExFSA::CRegExFSA()
{
    AddState();                     // 0: dummy
    AddState(CRegEx::eTypeStart);   // 1: begin of the input
}


void CRegExFSA::Add(const CRegEx& rx)
{
    Create(rx, m_Str.size());
    m_Str.push_back(rx.m_Str);
}


void CRegExFSA::Add(const vector<unique_ptr<CRegEx>>& v)
{
    if (!v.size()) {
        return;
    }
    vector<unique_ptr<CRegExFSA>> w;
    for (auto& rx : v) {
        unique_ptr<CRegExFSA> p(new CRegExFSA);
        p->Create(*rx, m_Str.size());
        m_Str.push_back(rx->m_Str);
        w.push_back(move(p));
    }
    while (w.size() > 1) {
        size_t h = (w.size() + 1) / 2;
        for (size_t i = 0, j = h; j < w.size(); i++, j++) {
            w[i]->Merge(move(w[j]));
        }
        w.resize(h);
    }
    Merge(move(w[0]));
}


void CRegExFSA::Merge(unique_ptr<CRegExFSA> fsa)
{
    size_t shift = m_States.size();
    for (auto& state : fsa->m_States) {
        for (auto& trans : state->m_Trans) {
            trans += shift;
        }
        m_States.push_back(move(state));
    }
    Short(0, shift);
    Short(shift, 0);
    Short(1, shift + 1);
    Short(shift + 1, 1);
    Refine();
}


void CRegExFSA::Create(const CRegEx& rx, size_t emit)
{
    if (!rx.m_RegX) {
        throw "Invalid Regular Expression: " + rx.m_Str + " -- " + rx.m_Err;
    }
    Short(0, AddState(CRegEx::eTypeStop));

    size_t from = AddState();
    size_t to = AddState();
    Emit(to, emit);
    try {
        rx.m_RegX->Render(*this, from, to);
    }
    catch (const string& s) {
        Refine();
        throw "Unsupported Regular Expression: " + rx.m_Str + " -- " + s;
    }
    Short(0, from);
    Refine();
}


void CRegExFSA::Refine()
{
    TStates DSA;
    vector<size_t> Set;
    vector<size_t> Queue;
    TNodeSetMap NM;
    TNodeSetList NL;
    TNodeSet NS;
    TScratch VV, HH;
    DSA.push_back(unique_ptr<CRegExState>(new CRegExState));
    NS.push_back(TNode(0, CRegEx::eTypePass));
    NL.push_back(NS);

    Push(0, VV[0], HH[0]);
    Push(1, VV[0], HH[0]);
    Collect(VV, CRegEx::eTypeStart, m_States, DSA, NM, NL, NS, HH);

    for (size_t n = 1; n < DSA.size(); n++) {
        if (DSA[n]->m_Type != CRegEx::eTypeStop) {
            for (unsigned c = 0; c < 256; c++) {
                Extend(n, (char)c, m_States, DSA, NM, NL, NS, VV, HH);
            }
        }
    }
    m_States.swap(DSA);
}


size_t CRegExFSA::Collect(TScratch& VV, CRegEx::EType ttt, TStates& src, TStates& dest, TNodeSetMap& NM, TNodeSetList& NL, TNodeSet& NS, TScratch& HH)
{
    NS.clear();
    vector<size_t>& V0 = VV[0];
    vector<size_t>& V1 = VV[1];
    vector<size_t>& V2 = VV[2];
    vector<size_t>& V3 = VV[3];
    vector<size_t>& H0 = HH[0];
    vector<size_t>& H1 = HH[1];
    vector<size_t>& H2 = HH[2];
    vector<size_t>& H3 = HH[3];

    for (size_t i = 0; i < V0.size(); ++i) {
        size_t n = V0[i];
        for (size_t s : src[n]->m_Short) {
            if (src[s]->m_Type & ttt) {
                auto t = src[s]->m_Type;
                if (t & CRegEx::eTypeCheck) {
                    if (t & CRegEx::eTypeToNoWord) {
                        Push(s, V1, H1);
                    }
                    if (t & CRegEx::eTypeToWord) {
                        Push(s, V2, H2);
                    }
                    if (t & CRegEx::eTypeToStop) {
                        Push(s, V3, H3);
                    }
                }
                else {
                    Push(s, V0, H0);
                }
            }
        }
    }
    for (size_t i = 0; i < V1.size(); ++i) {
        size_t n = V1[i];
        for (size_t s : src[n]->m_Short) {
            if (src[s]->m_Type & ttt) {
                auto t = src[s]->m_Type;
                if ((t & CRegEx::eTypeToNoWord) || !(t & CRegEx::eTypeCheck)) {
                    if (!In(s, H0)) {
                        Push(s, V1, H1);
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < V2.size(); ++i) {
        size_t n = V2[i];
        for (size_t s : src[n]->m_Short) {
            if (src[s]->m_Type & ttt) {
                auto t = src[s]->m_Type;
                if ((t & CRegEx::eTypeToWord) || !(t & CRegEx::eTypeCheck)) {
                    if (!In(s, H0)) {
                        Push(s, V2, H2);
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < V3.size(); ++i) {
        size_t n = V3[i];
        for (size_t s : src[n]->m_Short) {
            if (src[s]->m_Type & ttt) {
                auto t = src[s]->m_Type;
                if ((t & CRegEx::eTypeToStop) || !(t & CRegEx::eTypeCheck)) {
                    if (!In(s, H0)) {
                        Push(s, V3, H3);
                    }
                }
            }
        }
    }
    for (size_t n : H0) { // these are already ordered by Push()
        NS.push_back(TNode(n, CRegEx::eTypeNone));
    }
    for (size_t n : H1) {
        NS.push_back(TNode(n, CRegEx::eTypeNoWord));
    }
    for (size_t n : H2) {
        NS.push_back(TNode(n, CRegEx::eTypeWord));
    }
    for (size_t n : H3) {
        NS.push_back(TNode(n, CRegEx::eTypeStop));
    }
    auto I = NM.find(NS);
    if (I != NM.end()) {
        dest[I->second]->m_Type |= ttt;
        return I->second;
    }
    size_t x = NL.size();
    NM[NS] = x;
    NL.push_back(NS);
    dest.push_back(unique_ptr<CRegExState>(new CRegExState(ttt)));
    for (size_t n : H0) {
        dest[x]->m_Emit.insert(src[n]->m_Emit.begin(), src[n]->m_Emit.end());
    }
    for (size_t n : H1) {
        dest[x]->m_Forward1.insert(src[n]->m_Emit.begin(), src[n]->m_Emit.end());
    }
    for (size_t n : H2) {
        dest[x]->m_Forward2.insert(src[n]->m_Emit.begin(), src[n]->m_Emit.end());
    }
    for (size_t n : H3) {
        dest[x]->m_Forward3.insert(src[n]->m_Emit.begin(), src[n]->m_Emit.end());
    }
    return x;
}


void CRegExFSA::Extend(size_t x, unsigned char c, TStates& src, TStates& dest, TNodeSetMap& NM, TNodeSetList& NL, TNodeSet& NS, TScratch& VV, TScratch& HH)
{
    for (auto& a : VV) { // that is cheaper than creating new arrays
        a.clear();
    }
    for (auto& a : HH) { // that is cheaper than creating new arrays
        a.clear();
    }
    Push(0, VV[0], HH[0]);
    CRegEx::EType ttt = c ? CRegEx::IsWordCharacter(c) ? CRegEx::eTypeWord : CRegEx::eTypeNoWord : CRegEx::eTypeStop;
    for (auto p : NL[x]) {
        if (!p.second || p.second == ttt) { // if (p.first) - check performance with and without
            Push(src[p.first]->m_Trans[c], VV[0], HH[0]);
        }
    }
    size_t d = Collect(VV, ttt, src, dest, NM, NL, NS, HH);
    dest[x]->m_Trans[c] = d;
    if (ttt == CRegEx::eTypeNoWord) {
        dest[d]->m_Emit.insert(dest[x]->m_Forward1.begin(), dest[x]->m_Forward1.end());
    }
    if (ttt == CRegEx::eTypeWord) {
        dest[d]->m_Emit.insert(dest[x]->m_Forward2.begin(), dest[x]->m_Forward2.end());
    }
    if (ttt == CRegEx::eTypeStop) {
        dest[d]->m_Emit.insert(dest[x]->m_Forward3.begin(), dest[x]->m_Forward3.end());
    }
}


static string QuoteDot(const string& s, bool space = false)
{
    string out;
    for (size_t i = 0; i < s.size(); i++) {
        switch (s[i]) {
        case ' ':
            out += space ? "&#9251;" : " ";
            break;
        case '\"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case 0x7f:  // DEL
            out += "&#9249;";
            break;
        default:
            if ((unsigned char)s[i] < 32) { // non-printable characters
                out += "&#" + to_string(9216 + s[i]) + ";";
            }
            else if ((unsigned char)s[i] > 0x7f) {  // second half of ASCII table
                out += "&#" + to_string(912 + (unsigned char)s[i]) + ";";
            }
            else {
                out += s[i];
            }
        }
    }
    return out;
}


static string Pack(const string& s)
{
    string out;
    char from = 0;
    char to = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if ((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z')) {
            if (!from) {
                from = s[i];
                to = from;
                continue;
            }
            else if (s[i] == to + 1) {
                to++;
                continue;
            }
            else {
                out += from;
                if (to > from + 1) {
                    out += '-';
                }
                if (to > from) {
                    out += to;
                }
                from = 0;
                i--;
                continue;
            }
        }
        if (from) {
            out += from;
            if (to > from + 1) {
                out += '-';
            }
            if (to > from) {
                out += to;
            }
            from = 0;
        }
        out += s[i];
    }
    if (from) {
        out += from;
        if (to > from + 1) {
            out += '-';
        }
        if (to > from) {
            out += to;
        }
        from = 0;
    }
    return out;
}


void CRegExFSA::GenerateDotGraph(ostream& out) const  // Dump in DOT format (http://www.graphviz.org/)
{
    out << "digraph {\n";
    for (size_t n = 1; n < m_States.size(); ++n) {
        string label = to_string(n) + ": ";
        if (CRegEx::eTypePass == (m_States[n]->m_Type & CRegEx::eTypePass)) {
            label += "*";
        }
        else {
            if (m_States[n]->m_Type & CRegEx::eTypeStart) {
                label += "^";
            }
            if (m_States[n]->m_Type & CRegEx::eTypeWord) {
                label += "w";
            }
            if (m_States[n]->m_Type & CRegEx::eTypeNoWord) {
                label += "#";
            }
            if (m_States[n]->m_Type & CRegEx::eTypeStop) {
                label += "$";
            }
        }
        if (m_States[n]->m_Type & CRegEx::eTypeCheck) {
            label += " : ";
            if (m_States[n]->m_Type & CRegEx::eTypeToWord) {
                label += "w";
            }
            if (m_States[n]->m_Type & CRegEx::eTypeToNoWord) {
                label += "#";
            }
            if (m_States[n]->m_Type & CRegEx::eTypeToStop) {
                label += "$";
            }
        }
        if (!m_States[n]->m_Emit.empty()) {
            label += "\\nfound:";
            bool first = true;
            for (size_t e : m_States[n]->m_Emit) {
                if (!first) {
                    label += ",";
                }
                first = false;
                label += " &#8220;" + QuoteDot(m_Str[e]) + "&#8221;";
            }
        }
        out << n << " [label=\"" << label << "\"]\n";
        for (size_t t : m_States[n]->m_Short) {
            out << n << " -> " << t << " [style=dotted]\n";
        }
        if (m_States[n]->m_Type != CRegEx::eTypeStop) {
            map<size_t, string> arrows;
            for (unsigned m = 1; m < 256; m++) {
                arrows[m_States[n]->m_Trans[m]] += char(m);
            }
            arrows[m_States[n]->m_Trans[0]] += "&#9216;";
            size_t max_len = 0;
            size_t other;
            for (auto &A : arrows) {
                A.second = Pack(A.second);
                if (A.second.length() > max_len) {
                    other = A.first;
                    max_len = A.second.length();
                }
            }
            arrows[other] = "...";
            for (auto &A : arrows) {
                out << n << " -> " << A.first << " [label=\"" << QuoteDot(A.second, true) << "\"]\n";
            }
        }
    }
    out << "}\n";
}


void CRegExFSA::GenerateSourceCode(ostream& out) const
{
    out << "// Input from the outer code: const unsigned char* p;\n//\n\n    const unsigned char* _p = p;\n";
    for (size_t n = 1; n < m_States.size(); ++n) {
        if (n > 1) {
            out << "_" << n << ":\n";
        }
        for (auto e : m_States[n]->m_Emit) {
            out << "    if (_FSM_REPORT(" << e << ", p - _p)) return;  // " << m_Str[e] << "\n";
        }
        if (m_States[n]->m_Type & CRegEx::eTypeStop) {
            out << "    return;\n";
            continue;
        }
        if (n > 1) {
            out << "    ++p;\n";
        }
        out << "    switch (*p) {\n";
        map<size_t, string> arrows;
        for (unsigned m = 0; m < 256; m++) {
            arrows[m_States[n]->m_Trans[m]] += char(m);   // it's ok for an std::string to have a null character inside!
        }
        size_t max_len = 0;
        size_t other = 0;
        for (auto A = arrows.begin(); A != arrows.end(); ++A) {
            if (A->second.length() > max_len) {
                other = A->first;
                max_len = A->second.length();
            }
        }
        for (auto A = arrows.begin(); A != arrows.end(); ++A) {
            if (A->first != other) {
                for (auto p = A->second.begin(); p != A->second.end(); ++p) {
                    out << "        case ";
                    if (*p == '\'' || *p == '\"' || *p == '\\') {
                        out << "\'\\" << *p << "\':\n";
                    }
                    else if (*p >= 32 && *p < 127) {
                        out << "\'" << *p << "\':\n";
                    }
                    else {
                        out << (unsigned)*p << ":\n";
                    }
                }
                out << "            goto _" << A->first << ";\n";
            }
        }
        out << "        default:\n";
        out << "            goto _" << other << ";\n";
        out << "    }\n";
    }
}


void CRegExFSA::GenerateArrayMapData(ostream& out) const
{
    out << "_FSM_EMIT = {\n";   // #define _FSM_EMIT static bool emit[]
    for (size_t n = 0; n < m_States.size() - 1; n++) {
        cout << (n ? n % 32 ? ", " : ",\n" : "") << (m_States[n + 1]->m_Emit.size() ? "1" : "0");
    }
    out << "\n};\n";

    out << "_FSM_HITS = {\n";   // #define _FSM_HITS static map<size_t, vector<size_t>> hits
    size_t count = 0;
    for (size_t n = 0; n < m_States.size(); n++) {
        if (m_States[n]->m_Emit.size()) {
            count++;
        }
    }
    for (size_t n = 0; n < m_States.size(); n++) {
        if (m_States[n]->m_Emit.size()) {
            count--;
            out << "{ " << (n - 1) << ", { ";
            size_t i = 0;
            for (auto e : m_States[n]->m_Emit) {
                out << (i ? ", " : "") << e;
                i++;
            }
            out << " }}" << (count ? ",  " : "  ");
            for (auto e : m_States[n]->m_Emit) {
                out << " // " << e << ": " << m_Str[e];
                i++;
            }
            out << "\n";
        }
    }
    out << "};\n";

    out << "_FSM_STATES = {";   // #define _FSM_STATE static size_t[] states
    for (size_t n = 1; n < m_States.size(); n++) {
        out << "\n// " << (n - 1);
        for (size_t i = 0; i < 256; i++) {
            cout << (i % 32 ? ", " : "\n") << (m_States[n]->m_Trans[i] ? m_States[n]->m_Trans[i] - 1 : 0) << (i % 32 == 31 && (i != 255 || n < m_States.size() - 1) ? "," : "");
        }
    }
    out << "\n};\n";
}

END_NCBI_SCOPE
