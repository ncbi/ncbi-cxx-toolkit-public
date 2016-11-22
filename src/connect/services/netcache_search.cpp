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
 * Author: Rafael Sadyrov
 *
 */


#include <ncbi_pch.hpp>

#include <corelib/ncbistr.hpp>
#include <corelib/ncbidbg.hpp>
#include <connect/services/netcache_search.hpp>
#include "netcache_api_impl.hpp"

#include <ostream>
#include <list>
#include <limits>


namespace ncbi {
namespace grid {
namespace netcache {
namespace search {

using namespace chrono;

enum ETerm : size_t
{
    eKey,
    eSubkey,
    eCreatedAgo,
    eCreated,
    eExpiresIn,
    eExpires,
    eVersionExpiresIn,
    eVersionExpires,
    eSize,

    eNumberOfTerms
};

enum EComparison : size_t
{
    eGreaterOrEqual,
    eEqual,
    eLessThan,

    eNumberOfComparisons
};

struct SCondition
{
    SCondition(size_t key) : m_Key(key) {}
    virtual ~SCondition() {}

    size_t Key() const { return m_Key; }

    virtual ostream& Output(ostream&) const = 0;
    virtual void Merge(SCondition*) = 0;

    template <ETerm term, EComparison comparison, typename TValue>
    static SCondition* Create(TValue);

private:
    size_t m_Key;
};

ostream& operator<<(ostream& os, const SCondition& c)
{
    return c.Output(os);
}

template <ETerm term, EComparison comparison, typename TValue>
struct SConditionImpl : SCondition
{
    SConditionImpl(TValue value) :
        SCondition(comparison + term * eNumberOfComparisons),
        m_Value(value)
    {
    }

    ostream& Output(ostream& os) const override;
    void Merge(SCondition*) override;

private:
    TValue m_Value;
};

const char* s_Term(ETerm term)
{
    switch (term) {
    case eKey:              return "key";
    case eSubkey:           return "subkey";
    case eCreated:          return "fcr_epoch";
    case eCreatedAgo:       return "fcr_ago";
    case eExpires:          return "fexp_epoch";
    case eExpiresIn:        return "fexp_now";
    case eVersionExpires:   return "fvexp_epoch";
    case eVersionExpiresIn: return "fvexp_now";
    case eSize:             return "fsize";
    default:                return nullptr;
    }
}

const char* s_Comparison(EComparison comparison)
{
    switch (comparison) {
    case eLessThan:         return "_lt";
    case eEqual:            return "";
    case eGreaterOrEqual:   return "_ge";
    default:                return nullptr;
    }
}

template <typename TValue>
string s_Value(TValue value)
{
    return to_string(value);
}

template <>
string s_Value(string value)
{
    return value;
}

template <ETerm term, EComparison comparison, typename TValue>
ostream& SConditionImpl<term, comparison, TValue>::Output(ostream& os) const
{
    return os << s_Term(term) << s_Comparison(comparison) << "=" << s_Value(m_Value);
}

template <ETerm term, EComparison comparison, typename TValue>
struct SMerge;

template <ETerm term, typename TValue>
struct SMerge<term, eLessThan, TValue>
{
    SMerge(TValue& left, const TValue& right)
    {
        if (left > right) left = right;
    }
};

template <ETerm term, typename TValue>
struct SMerge<term, eGreaterOrEqual, TValue>
{
    SMerge(TValue& left, const TValue& right)
    {
        if (left < right) left = right;
    }
};

template <>
struct SMerge<eKey, eEqual, string>
{
    SMerge(string&, const string&)
    {
        NCBI_THROW_FMT(CNetCacheException, eNotImplemented,
            "Field '" << s_Term(eKey) << "' cannot be specified more than once");
    }
};

template <ETerm term, EComparison comparison, typename TValue>
void SConditionImpl<term, comparison, TValue>::Merge(SCondition* o)
{
    auto other = dynamic_cast<decltype(this)>(o);
    _ASSERT(other);

    SMerge<term, comparison, TValue>(m_Value, other->m_Value);
}

template <ETerm term, EComparison comparison, typename TValue>
SCondition* SCondition::Create(TValue value)
{
    return new SConditionImpl<term, comparison, TValue>(value);
}

struct SExpressionImpl
{
    list<shared_ptr<SCondition>> conditions;
};

SExpression::~SExpression()
{
}

template <ETerm term, EComparison comparison, typename TValue>
SExpression s_CreateBase(TValue value)
{
    auto condition = SCondition::Create<term, comparison>(value);

    SExpression result;
    result.impl.reset(new SExpressionImpl);
    result.impl->conditions.emplace_back(condition);
    return result;
}

template <ETerm term, EComparison comparison, typename TValue>
CExpression s_Create(TValue value)
{
    CExpression result;
    result.base = s_CreateBase<term, comparison>(value);
    return result;
}

chrono::seconds::rep s_GetSeconds(duration d)
{
    return duration_cast<chrono::seconds>(d).count();
}

chrono::seconds::rep s_GetSeconds(time_point p)
{
    return s_GetSeconds(p.time_since_epoch());
}

CExpression operator==(KEY, string v)
{
    return s_Create<eKey, eEqual>(v);
}

CExpression operator>=(CREATED, time_point v)
{
    return s_Create<eCreated, eGreaterOrEqual>(s_GetSeconds(v));
}

CExpression operator< (CREATED, time_point v)
{
    return s_Create<eCreated, eLessThan>(s_GetSeconds(v));
}

CExpression operator>=(CREATED, duration v)
{
    return s_Create<eCreatedAgo, eGreaterOrEqual>(s_GetSeconds(v));
}

CExpression operator< (CREATED, duration v)
{
    return s_Create<eCreatedAgo, eLessThan>(s_GetSeconds(v));
}

CExpression operator>=(EXPIRES, time_point v)
{
    return s_Create<eExpires, eGreaterOrEqual>(s_GetSeconds(v));
}

CExpression operator< (EXPIRES, time_point v)
{
    return s_Create<eExpires, eLessThan>(s_GetSeconds(v));
}

CExpression operator>=(EXPIRES, duration v)
{
    return s_Create<eExpiresIn, eGreaterOrEqual>(s_GetSeconds(v));
}

CExpression operator< (EXPIRES, duration v)
{
    return s_Create<eExpiresIn, eLessThan>(s_GetSeconds(v));
}

CExpression operator>=(VERSION_EXPIRES, time_point v)
{
    return s_Create<eVersionExpires, eGreaterOrEqual>(s_GetSeconds(v));
}

CExpression operator< (VERSION_EXPIRES, time_point v)
{
    return s_Create<eVersionExpires, eLessThan>(s_GetSeconds(v));
}

CExpression operator>=(VERSION_EXPIRES, duration v)
{
    return s_Create<eVersionExpiresIn, eGreaterOrEqual>(s_GetSeconds(v));
}

CExpression operator< (VERSION_EXPIRES, duration v)
{
    return s_Create<eVersionExpiresIn, eLessThan>(s_GetSeconds(v));
}

CExpression operator>=(SIZE, size_t v)
{
    return s_Create<eSize, eGreaterOrEqual>(v);
}

CExpression operator< (SIZE, size_t v)
{
    return s_Create<eSize, eLessThan>(v);
}

void s_Merge(SExpression& l, SExpression& r)
{
    if (!l.impl) { l = r; return; }
    if (!r.impl) return;

    auto& lc = l.impl->conditions;
    auto& rc = r.impl->conditions;
    auto li = lc.begin();
    auto ri = rc.begin();

    while (li != lc.end() && ri != rc.end()) {
        auto& lp = *li;
        auto& rp = *ri;

        if (lp->Key() < rp->Key()) {
            ++li;
        } else if (lp->Key() > rp->Key()) {
            auto old_ri = ri;
            lc.splice(li, rc, old_ri, ++ri);
        } else {
            lp->Merge(rp.get());
            ++li;
            ++ri;
        }
    }

    if (ri != rc.end()) {
        lc.splice(lc.end(), rc, ri, rc.end());
    }
}

CExpression operator&&(CExpression l, CExpression r)
{
    s_Merge(l.base, r.base);
    return l;
}

CExpression operator+(CExpression l, CFields r)
{
    s_Merge(l.base, r.base);
    return l;
}

ostream& operator<<(ostream& os, CExpression expression)
{
    if (!expression.base.impl) return os;

    for (auto& condition : expression.base.impl->conditions) {
        os << " " << *condition;
    }

    return os;
}

// Mirrored versions
CExpression operator==(string v, KEY                 t) { return t == v; }
CExpression operator<=(time_point v, CREATED         t) { return t >= v; }
CExpression operator> (time_point v, CREATED         t) { return t <  v; }
CExpression operator<=(duration   v, CREATED         t) { return t >= v; }
CExpression operator> (duration   v, CREATED         t) { return t <  v; }
CExpression operator<=(time_point v, EXPIRES         t) { return t >= v; }
CExpression operator> (time_point v, EXPIRES         t) { return t <  v; }
CExpression operator<=(duration   v, EXPIRES         t) { return t >= v; }
CExpression operator> (duration   v, EXPIRES         t) { return t <  v; }
CExpression operator<=(time_point v, VERSION_EXPIRES t) { return t >= v; }
CExpression operator> (time_point v, VERSION_EXPIRES t) { return t <  v; }
CExpression operator<=(duration   v, VERSION_EXPIRES t) { return t >= v; }
CExpression operator> (duration   v, VERSION_EXPIRES t) { return t <  v; }
CExpression operator<=(size_t v, SIZE                t) { return t >= v; }
CExpression operator> (size_t v, SIZE                t) { return t <  v; }

// Cannot use zero, as corresponding condition would be ignored then
const chrono::seconds::rep kSmallestTimePoint = 1;
const size_t kLargestSize = numeric_limits<size_t>::max();

CFields::CFields()
{
}

CFields::CFields(CREATED)
    : base(s_CreateBase<eCreated, eGreaterOrEqual>(kSmallestTimePoint))
{
}

CFields::CFields(EXPIRES)
    : base(s_CreateBase<eExpires, eGreaterOrEqual>(kSmallestTimePoint))
{
}

CFields::CFields(VERSION_EXPIRES)
    : base(s_CreateBase<eVersionExpires, eGreaterOrEqual>(kSmallestTimePoint))
{
}

CFields::CFields(SIZE)
    : base(s_CreateBase<eSize, eLessThan>(kLargestSize))
{
}

CFields operator|(CFields l, CFields r)
{
    s_Merge(l.base, r.base);
    return l;
}

struct SBlobInfoImpl
{
    string key;
    string subkey;

    SBlobInfoImpl(string key, string subkey, string data);
    time_point operator[](CREATED);
    time_point operator[](EXPIRES);
    time_point operator[](VERSION_EXPIRES);
    size_t operator[](SIZE);

    static SBlobInfoImpl* Create(string data);

private:
    void Parse();

    const string m_Data;
    bool m_Parsed;
    CNullable<time_point> m_Created;
    CNullable<time_point> m_Expires;
    CNullable<time_point> m_VersionExpires;
    CNullable<size_t> m_Size;
};

SBlobInfoImpl::SBlobInfoImpl(string k, string sk, string data)
    : key(k),
      subkey(sk),
      m_Data(data),
      m_Parsed(false)
{
}

time_point SBlobInfoImpl::operator[](CREATED)
{
    if (!m_Parsed) Parse();
    return m_Created.GetValue();
}

time_point SBlobInfoImpl::operator[](EXPIRES)
{
    if (!m_Parsed) Parse();
    return m_Expires.GetValue();
}

time_point SBlobInfoImpl::operator[](VERSION_EXPIRES)
{
    if (!m_Parsed) Parse();
    return m_VersionExpires.GetValue();
}

size_t SBlobInfoImpl::operator[](SIZE)
{
    if (!m_Parsed) Parse();
    return m_Size.GetValue();
}

const string kSeparator = "\t";

void SBlobInfoImpl::Parse()
{
    vector<CTempString> fields;

    NStr::Split(m_Data, kSeparator, fields);

    for (auto& field : fields) {
        string name, value;
        NStr::SplitInTwo(field, "=", name, value);
        if (name == "cr_time") {
            m_Created = time_point(seconds(stoll(value)));
        } else if (name == "exp") {
            m_Expires = time_point(seconds(stoll(value)));
        } else if (name == "ver_dead") {
            m_VersionExpires = time_point(seconds(stoll(value)));
        } else if (name == "size") {
            m_Size = stoull(value);
        } else {
            NCBI_THROW_FMT(CNetCacheException, eInvalidServerResponse,
                "Unknown field '" << name << "' in response '" << m_Data << "'");
        }
    }

    m_Parsed = true;
}

void operator<<(CBlobInfo& blob_info, string data)
{
    string cache, key, subkey, remaining;
    NStr::SplitInTwo(data, kSeparator, cache, remaining);
    NStr::SplitInTwo(remaining, kSeparator, key, remaining);
    NStr::SplitInTwo(remaining, kSeparator, subkey, remaining);
    blob_info.base.impl.reset(new SBlobInfoImpl(key, subkey, remaining));
}

SBlobInfo::~SBlobInfo()
{
}

string CBlobInfo::operator[](KEY) const
{
    if (!base.impl) return string();
    return base.impl->key;
}

string CBlobInfo::operator[](SUBKEY) const
{
    if (!base.impl) return string();
    return base.impl->subkey;
}

time_point CBlobInfo::operator[](CREATED created) const
{
    if (!base.impl) return time_point();
    return (*base.impl)[created];
}

time_point CBlobInfo::operator[](EXPIRES expires) const
{
    if (!base.impl) return time_point();
    return (*base.impl)[expires];
}

time_point CBlobInfo::operator[](VERSION_EXPIRES version_expires) const
{
    if (!base.impl) return time_point();
    return (*base.impl)[version_expires];
}

size_t CBlobInfo::operator[](SIZE size) const
{
    if (!base.impl) return size_t();
    return (*base.impl)[size];
}

}
}
}
}
