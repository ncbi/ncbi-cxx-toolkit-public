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
#include <corelib/ncbi_safe_static.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;


class CSafeStaticTestApp : public CNcbiApplication
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

    using TData = pair<int, bool>;
    shared_ptr<TData> GetData() const { return m_Data; }

private:
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

struct SFunctions
{
    static SObject* After(int n, SObject* object, CSafeStaticLifeSpan life_span)
    {
        auto shared = GetShared();
        assert(shared);

        cout << "Registering function #" << n - 1 << " to be called after destroying object #" << n << endl;
        CSafeStaticGuard::Register(bind(CheckAfter, n, shared), life_span);
        shared->emplace(n, nullptr);
        return object;
    }

    static SObject* BeforeOrUpdate(int n, SObject* object, CSafeStaticLifeSpan life_span)
    {
        auto shared = GetShared();
        assert(shared);

        assert(object);
        assert(n == object->GetData()->first);

        if (auto [it, added] = shared->insert_or_assign(n, object->GetData()); added) {
            cout << "Registering function #" << n + 1 << " to be called before destroying object #" << n << endl;
            CSafeStaticGuard::Register(bind(CheckBefore, n, shared), life_span);
        }

        return object;
    }

private:
    using TSharedData = map<int, shared_ptr<SObject::TData>>;

    static shared_ptr<TSharedData> GetShared()
    {
        if (auto rv = sm_Shared.lock()) return rv;

        auto rv = make_shared<TSharedData>();
        sm_Shared = rv;
        return rv;
    }

    static void CheckBefore(int n, shared_ptr<TSharedData> shared)
    {
        assert(shared);

        const auto& data = (*shared)[n];
        assert(data);

        if (data->second) {
            cout << "Function #" << n + 1 << " called before destroying object #" << data->first << endl;
        } else {
            cerr << "Function #" << n + 1 << " called before destroying object #" << data->first << " which has already been DESTROYED!" << endl;
            assert(false); // Cannot use _ASSERT as CNcbiDiag may already have been destroyed
        }
    }

    static void CheckAfter(int n, shared_ptr<TSharedData> shared)
    {
        assert(shared);

        const auto& data = (*shared)[n];
        assert(data);

        if (!data->second) {
            cout << "Function #" << n - 1 << " called after destroying object #" << data->first << endl;
        } else {
            cerr << "Function #" << n - 1 << " called after destroying object #" << data->first << " which has not yet been DESTROYED!" << endl;
            assert(false); // Cannot use _ASSERT as CNcbiDiag may already have been destroyed
        }
    }

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

template <int i> auto GetLifespan() { return CSafeStaticLifeSpan(ls_level<i / 100>, ls_span<i / 10 % 10>, ls_adjust<i % 10>); }

template <int n>        CSafeStatic<SObject> global_one = { &SObject::Create<n>, nullptr, GetLifespan<n / 10>() };
template <int n> static CSafeStatic<SObject> static_one = { &SObject::Create<n>, nullptr, GetLifespan<n / 10>() };

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
    static CSafeStatic<SObject> local_one = { &SObject::Create<n>, nullptr, GetLifespan<n / 10>() };
    return local_one->Do(static_one);
}

template <int n>
struct SMemberOne
{
    auto Do(SObject* local_one) { return member_one->Do(local_one); }

    inline static CSafeStatic<SObject> member_one = { &SObject::Create<n>, nullptr, GetLifespan<n / 10>() };
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

template <int n>
auto FuncAfter(SObject* object = nullptr)
{
    return SFunctions::After(n, object, GetLifespan<n / 10>());
}

template <int n>
auto FuncUpdate(SObject* object)
{
    return SFunctions::BeforeOrUpdate(n, object, GetLifespan<n / 10>());
}

template <int n>
auto FuncBefore(SObject* object)
{
    return SFunctions::BeforeOrUpdate(n, object, GetLifespan<n / 10>());
}

int CSafeStaticTestApp::Run(void)
{
    FuncBefore<1114>(MemberOne<1114>(LocalOne<1113>(StaticOne<1112>(FuncUpdate<1111>(GlobalOne<1111>(FuncAfter<1111>()))))));
    FuncBefore<1124>(MemberOne<1124>(LocalOne<1123>(StaticOne<1122>(FuncUpdate<1121>(GlobalOne<1121>(FuncAfter<1121>(MemberOne<1114>())))))));
    FuncBefore<1134>(MemberOne<1134>(LocalOne<1133>(StaticOne<1132>(FuncUpdate<1131>(GlobalOne<1131>(FuncAfter<1131>(MemberOne<1124>())))))));
    FuncBefore<1214>(MemberOne<1214>(LocalOne<1213>(StaticOne<1212>(FuncUpdate<1211>(GlobalOne<1211>(FuncAfter<1211>(MemberOne<1134>())))))));
    FuncBefore<1224>(MemberOne<1224>(LocalOne<1223>(StaticOne<1222>(FuncUpdate<1221>(GlobalOne<1221>(FuncAfter<1221>(MemberOne<1214>())))))));
    FuncBefore<1234>(MemberOne<1234>(LocalOne<1233>(StaticOne<1232>(FuncUpdate<1231>(GlobalOne<1231>(FuncAfter<1231>(MemberOne<1224>())))))));
    FuncBefore<1314>(MemberOne<1314>(LocalOne<1313>(StaticOne<1312>(FuncUpdate<1311>(GlobalOne<1311>(FuncAfter<1311>(MemberOne<1234>())))))));
    FuncBefore<1324>(MemberOne<1324>(LocalOne<1323>(StaticOne<1322>(FuncUpdate<1321>(GlobalOne<1321>(FuncAfter<1321>(MemberOne<1314>())))))));
    FuncBefore<1334>(MemberOne<1334>(LocalOne<1333>(StaticOne<1332>(FuncUpdate<1331>(GlobalOne<1331>(FuncAfter<1331>(MemberOne<1324>())))))));
    FuncBefore<1414>(MemberOne<1414>(LocalOne<1413>(StaticOne<1412>(FuncUpdate<1411>(GlobalOne<1411>(FuncAfter<1411>(MemberOne<1334>())))))));
    FuncBefore<1424>(MemberOne<1424>(LocalOne<1423>(StaticOne<1422>(FuncUpdate<1421>(GlobalOne<1421>(FuncAfter<1421>(MemberOne<1414>())))))));
    FuncBefore<1434>(MemberOne<1434>(LocalOne<1433>(StaticOne<1432>(FuncUpdate<1431>(GlobalOne<1431>(FuncAfter<1431>(MemberOne<1424>())))))));
    FuncBefore<1514>(MemberOne<1514>(LocalOne<1513>(StaticOne<1512>(FuncUpdate<1511>(GlobalOne<1511>(FuncAfter<1511>(MemberOne<1434>())))))));
    FuncBefore<1524>(MemberOne<1524>(LocalOne<1523>(StaticOne<1522>(FuncUpdate<1521>(GlobalOne<1521>(FuncAfter<1521>(MemberOne<1514>())))))));
    FuncBefore<1534>(MemberOne<1534>(LocalOne<1533>(StaticOne<1532>(FuncUpdate<1531>(GlobalOne<1531>(FuncAfter<1531>(MemberOne<1524>())))))));
    FuncBefore<2114>(MemberOne<2114>(LocalOne<2113>(StaticOne<2112>(FuncUpdate<2111>(GlobalOne<2111>(FuncAfter<2111>(MemberOne<1534>())))))));
    FuncBefore<2124>(MemberOne<2124>(LocalOne<2123>(StaticOne<2122>(FuncUpdate<2121>(GlobalOne<2121>(FuncAfter<2121>(MemberOne<2114>())))))));
    FuncBefore<2134>(MemberOne<2134>(LocalOne<2133>(StaticOne<2132>(FuncUpdate<2131>(GlobalOne<2131>(FuncAfter<2131>(MemberOne<2124>())))))));
    FuncBefore<2214>(MemberOne<2214>(LocalOne<2213>(StaticOne<2212>(FuncUpdate<2211>(GlobalOne<2211>(FuncAfter<2211>(MemberOne<2134>())))))));
    FuncBefore<2224>(MemberOne<2224>(LocalOne<2223>(StaticOne<2222>(FuncUpdate<2221>(GlobalOne<2221>(FuncAfter<2221>(MemberOne<2214>())))))));
    FuncBefore<2234>(MemberOne<2234>(LocalOne<2233>(StaticOne<2232>(FuncUpdate<2231>(GlobalOne<2231>(FuncAfter<2231>(MemberOne<2224>())))))));
    FuncBefore<2314>(MemberOne<2314>(LocalOne<2313>(StaticOne<2312>(FuncUpdate<2311>(GlobalOne<2311>(FuncAfter<2311>(MemberOne<2234>())))))));
    FuncBefore<2324>(MemberOne<2324>(LocalOne<2323>(StaticOne<2322>(FuncUpdate<2321>(GlobalOne<2321>(FuncAfter<2321>(MemberOne<2314>())))))));
    FuncBefore<2334>(MemberOne<2334>(LocalOne<2333>(StaticOne<2332>(FuncUpdate<2331>(GlobalOne<2331>(FuncAfter<2331>(MemberOne<2324>())))))));
    FuncBefore<2414>(MemberOne<2414>(LocalOne<2413>(StaticOne<2412>(FuncUpdate<2411>(GlobalOne<2411>(FuncAfter<2411>(MemberOne<2334>())))))));
    FuncBefore<2424>(MemberOne<2424>(LocalOne<2423>(StaticOne<2422>(FuncUpdate<2421>(GlobalOne<2421>(FuncAfter<2421>(MemberOne<2414>())))))));
    FuncBefore<2434>(MemberOne<2434>(LocalOne<2433>(StaticOne<2432>(FuncUpdate<2431>(GlobalOne<2431>(FuncAfter<2431>(MemberOne<2424>())))))));
    FuncBefore<2514>(MemberOne<2514>(LocalOne<2513>(StaticOne<2512>(FuncUpdate<2511>(GlobalOne<2511>(FuncAfter<2511>(MemberOne<2434>())))))));
    FuncBefore<2524>(MemberOne<2524>(LocalOne<2523>(StaticOne<2522>(FuncUpdate<2521>(GlobalOne<2521>(FuncAfter<2521>(MemberOne<2514>())))))));
    FuncBefore<2534>(MemberOne<2534>(LocalOne<2533>(StaticOne<2532>(FuncUpdate<2531>(GlobalOne<2531>(FuncAfter<2531>(MemberOne<2524>())))))));
    FuncBefore<2624>(MemberOne<2624>(LocalOne<2623>(StaticOne<2622>(FuncUpdate<2621>(GlobalOne<2621>(FuncAfter<2621>(MemberOne<2534>())))))));

    return 0;
}

int NcbiSys_main(int argc, const ncbi::TXChar* argv[])
{
    return CSafeStaticTestApp().AppMain(argc, argv);
}
