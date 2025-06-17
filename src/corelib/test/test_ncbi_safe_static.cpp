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
 * Authors: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_safe_static.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


class CSampleBasicApplication : public CNcbiApplication
{
private:
    virtual int  Run(void);
};

struct SObject
{
    SObject() : SObject('?') {}

    SObject(int n) : m_Data(x_CreateData(n))                     { Report("Creating object   #"sv); }
    ~SObject()                           { m_Data->second = false; Report("Destroying object #"sv); }
    auto Do(SObject* p = nullptr) { if (p) m_PrevData = p->m_Data; Report("Using object      #"sv); return this; }

    template <int n> static SObject* Create() { return new SObject(n); }

private:
    using TData = pair<int, bool>;
    using TSharedData = deque<TData>;

    auto x_GetShared()
    {
        if (auto rv = sm_Shared.lock()) return rv;

        auto rv = make_shared<TSharedData>();
        sm_Shared = rv;
        return rv;
    }

    shared_ptr<TData> x_CreateData(int n)
    {
        auto shared = x_GetShared();
        auto data = &shared->emplace_back(n, true);
        return { shared, data };
    }

    void Report(string_view what)
    {
        if (!m_PrevData) {
            cout << what << m_Data->first << endl;
        } else if (m_PrevData->second) {
            cout << what << m_Data->first << ", using object #" << m_PrevData->first << endl;
        } else {
            cerr << what << m_Data->first << ", using object #" << m_PrevData->first << " which has already been DESTROYED!" << endl;
            assert(false); // Cannot use _ASSERT as CNcbiDiag may already have been destroyed
        }
    }

    shared_ptr<TData> m_Data;
    shared_ptr<TData> m_PrevData;
    inline static weak_ptr<TSharedData> sm_Shared;
};

template <int> constexpr auto ls_level    = CSafeStaticLifeSpan::eLifeLevel_Default;
template <>    constexpr auto ls_level<2> = CSafeStaticLifeSpan::eLifeLevel_AppMain;

template <int> constexpr auto ls_span    = CSafeStaticLifeSpan::eLifeSpan_Longest;
template <>    constexpr auto ls_span<2> = CSafeStaticLifeSpan::eLifeSpan_Long;
template <>    constexpr auto ls_span<3> = CSafeStaticLifeSpan::eLifeSpan_Normal;
template <>    constexpr auto ls_span<4> = CSafeStaticLifeSpan::eLifeSpan_Short;
template <>    constexpr auto ls_span<5> = CSafeStaticLifeSpan::eLifeSpan_Shortest;
template <>    constexpr auto ls_span<6> = CSafeStaticLifeSpan::eLifeSpan_Min;

template <int> constexpr auto ls_adjust    =  42;
template <>    constexpr auto ls_adjust<2> =   0;
template <>    constexpr auto ls_adjust<3> = -13;

template <int i> const auto lifespan = CSafeStaticLifeSpan(ls_level<i / 100>, ls_span<i / 10 % 10>, ls_adjust<i % 10>);

template <int n>        CSafeStatic<SObject> global_one = { &SObject::Create<n>, nullptr, lifespan<n / 10> };
template <int n> static CSafeStatic<SObject> static_one = { &SObject::Create<n>, nullptr, lifespan<n / 10> };

template <int n>
auto GlobalOne(SObject* member_one = nullptr)
{
    return global_one<n>->Do(member_one);
}

template <int n>
auto StaticOne(SObject* global_one)
{
    return static_one<n>->Do(global_one);
}

template <int n>
auto LocalOne(SObject* static_one)
{
    static CSafeStatic<SObject> local_one = { &SObject::Create<n>, nullptr, lifespan<n / 10> };
    return local_one->Do(static_one);
}

template <int n>
struct SMemberOne
{
    auto Do(SObject* local_one) { return member_one->Do(local_one); }

    inline static CSafeStatic<SObject> member_one = { &SObject::Create<n>, nullptr, lifespan<n / 10> };
};

template <int n>
auto MemberOne()
{
    return &*SMemberOne<n>::member_one;
}

template <int n>
auto MemberOne(SObject* local_one)
{
    SMemberOne<n> member_one;
    return member_one.Do(local_one);
}

int CSampleBasicApplication::Run(void)
{
    MemberOne<1114>(LocalOne<1113>(StaticOne<1112>(GlobalOne<1111>())));
    MemberOne<1124>(LocalOne<1123>(StaticOne<1122>(GlobalOne<1121>(MemberOne<1114>()))));
    MemberOne<1134>(LocalOne<1133>(StaticOne<1132>(GlobalOne<1131>(MemberOne<1124>()))));
    MemberOne<1214>(LocalOne<1213>(StaticOne<1212>(GlobalOne<1211>(MemberOne<1134>()))));
    MemberOne<1224>(LocalOne<1223>(StaticOne<1222>(GlobalOne<1221>(MemberOne<1214>()))));
    MemberOne<1234>(LocalOne<1233>(StaticOne<1232>(GlobalOne<1231>(MemberOne<1224>()))));
    MemberOne<1314>(LocalOne<1313>(StaticOne<1312>(GlobalOne<1311>(MemberOne<1234>()))));
    MemberOne<1324>(LocalOne<1323>(StaticOne<1322>(GlobalOne<1321>(MemberOne<1314>()))));
    MemberOne<1334>(LocalOne<1333>(StaticOne<1332>(GlobalOne<1331>(MemberOne<1324>()))));
    MemberOne<1414>(LocalOne<1413>(StaticOne<1412>(GlobalOne<1411>(MemberOne<1334>()))));
    MemberOne<1424>(LocalOne<1423>(StaticOne<1422>(GlobalOne<1421>(MemberOne<1414>()))));
    MemberOne<1434>(LocalOne<1433>(StaticOne<1432>(GlobalOne<1431>(MemberOne<1424>()))));
    MemberOne<1514>(LocalOne<1513>(StaticOne<1512>(GlobalOne<1511>(MemberOne<1434>()))));
    MemberOne<1524>(LocalOne<1523>(StaticOne<1522>(GlobalOne<1521>(MemberOne<1514>()))));
    MemberOne<1534>(LocalOne<1533>(StaticOne<1532>(GlobalOne<1531>(MemberOne<1524>()))));
    MemberOne<2114>(LocalOne<2113>(StaticOne<2112>(GlobalOne<2111>(MemberOne<1534>()))));
    MemberOne<2124>(LocalOne<2123>(StaticOne<2122>(GlobalOne<2121>(MemberOne<2114>()))));
    MemberOne<2134>(LocalOne<2133>(StaticOne<2132>(GlobalOne<2131>(MemberOne<2124>()))));
    MemberOne<2214>(LocalOne<2213>(StaticOne<2212>(GlobalOne<2211>(MemberOne<2134>()))));
    MemberOne<2224>(LocalOne<2223>(StaticOne<2222>(GlobalOne<2221>(MemberOne<2214>()))));
    MemberOne<2234>(LocalOne<2233>(StaticOne<2232>(GlobalOne<2231>(MemberOne<2224>()))));
    MemberOne<2314>(LocalOne<2313>(StaticOne<2312>(GlobalOne<2311>(MemberOne<2234>()))));
    MemberOne<2324>(LocalOne<2323>(StaticOne<2322>(GlobalOne<2321>(MemberOne<2314>()))));
    MemberOne<2334>(LocalOne<2333>(StaticOne<2332>(GlobalOne<2331>(MemberOne<2324>()))));
    MemberOne<2414>(LocalOne<2413>(StaticOne<2412>(GlobalOne<2411>(MemberOne<2334>()))));
    MemberOne<2424>(LocalOne<2423>(StaticOne<2422>(GlobalOne<2421>(MemberOne<2414>()))));
    MemberOne<2434>(LocalOne<2433>(StaticOne<2432>(GlobalOne<2431>(MemberOne<2424>()))));
    MemberOne<2514>(LocalOne<2513>(StaticOne<2512>(GlobalOne<2511>(MemberOne<2434>()))));
    MemberOne<2524>(LocalOne<2523>(StaticOne<2522>(GlobalOne<2521>(MemberOne<2514>()))));
    MemberOne<2534>(LocalOne<2533>(StaticOne<2532>(GlobalOne<2531>(MemberOne<2524>()))));
    MemberOne<2624>(LocalOne<2623>(StaticOne<2622>(GlobalOne<2621>(MemberOne<2534>()))));

	return 0;
}

int main(int argc, ncbi::TXChar* argv[])
{
    return CSampleBasicApplication().AppMain(argc, argv);
}
