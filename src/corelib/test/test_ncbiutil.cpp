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
 * Author:  Aaron Ucko <ucko@ncbi.nlm.nih.gov>
 *
 * File Description:
 *   Test for the functionality in ncbiutil.hpp
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbiobj.hpp>
#include <set>
#include <functional>
#include <algorithm>

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE


static void TestPEqualTo(void)
{
    int             n1 = 17, n2 = 0, n3 = 17;
    set<const int*> s;

    s.insert(&n1);
    assert(find_if(s.begin(), s.end(), bind2nd(p_equal_to<int>(), &n2))
            == s.end());
    assert(find_if(s.begin(), s.end(), bind2nd(p_equal_to<int>(), &n3))
            != s.end());
    NcbiCout << "p_equal_to works." << NcbiEndl;
}

// Should add more tests here!


END_NCBI_SCOPE

USING_NCBI_SCOPE;

#if 0

#undef DECLARE_SAFE_BOOL_METHOD

#define DECLARE_SAFE_BOOL_METHOD(Expr)                                  \
    struct SSafeBoolTag {                                               \
        void True(SSafeBoolTag*) {}                                     \
        typedef void (SSafeBoolTag::*TBoolType)(SSafeBoolTag*);         \
    };                                                                  \
    operator SSafeBoolTag::TBoolType() const {                          \
        return (Expr)? &SSafeBoolTag::True: 0;                          \
    }                                                                   \
    private:                                                            \
    bool operator==(SSafeBoolTag::TBoolType) const;                     \
    bool operator!=(SSafeBoolTag::TBoolType) const;                     \
    public:                                                             \
    typedef SSafeBoolTag::TBoolType TBoolType

/*
template<class Class>
bool operator==(const Class&, typename Class::TBoolType)
{
    return Class::SafeBoolIsNotComparable();
}
template<class Class>
bool operator!=(const Class&, typename Class::TBoolType)
{
    return Class::SafeBoolIsNotComparable();
}
template<class Stream>
Stream& operator<<(Stream&, void (CSafeBoolTag::*)(CSafeBoolTag*))
{
    return Stream::SafeBoolCannotWritten();
}
*/
#endif


class A
{
public:
    A() : value(0) {}
    explicit A(int v) : value(v) {}

    DECLARE_OPERATOR_BOOL(value != 0);

    int Value() const { return value; }

    bool operator==(const A& v) const { return value == v.value; }
    bool operator!=(const A& v) const { return value != v.value; }
    bool operator<(const A& v) const { return value < v.value; }

protected:
    int value;
};


class B
{
public:
    B() : value(0) {}
    explicit B(const char* v) : value(v) {}

    DECLARE_OPERATOR_BOOL_PTR(value);

    const char* Value() const { return value; }

protected:
    const char* value;
};


class AA : public A
{
public:
    AA() {}
    AA(int v) : A(v) {}
};


class CObjA : public CObject
{
};


class CObjB : public CObject
{
};


int TestOperatorBool(void)
{
    string s;
    A a2(2), a1(1), a0(0);
    B b2("2"), b1("1"), b0(0);
    AA aa2(2), aa1(1), aa0(0);

    bool b_a0 = a0;
    bool b_a1 = a1;
    bool b_a2 = a2;
    bool b_b0 = b0;
    bool b_b1 = b1;
    bool b_b2 = b2;
    bool b_av0 = a0.Value();
    bool b_av1 = a1.Value();
    bool b_av2 = a2.Value();
    bool b_bv0 = b0.Value();
    bool b_bv1 = b1.Value();
    bool b_bv2 = b2.Value();

    assert(a2);
    assert(a1);
    assert(!a0);
    assert(b2);
    assert(b1);
    assert(!b0);
#if 0 // compile error ?
    assert(a0==0);
    assert(a1!=0);
    assert(!(a0!=0));
    assert(!(a1==0));
#endif
#if 0 // compile error ?
    assert(a0==false);
    assert(a1!=false);
    assert(a0!=true);
    assert(a1==true);
#endif
#if 0 // should not compile ?
    cout << a0 << endl;
    cout << b0 << endl;
    cout << a2 << endl;
    cout << b2 << endl;
#endif
#if 1 // should compile
    cout << 0 << endl;
    cout << 0.0 << endl;
    cout << '\0' << endl;
    cout << (void*)0 << endl;
    //cout << NULL << endl;
    //cout << nullptr << endl;
#endif

    assert(a2 && a1);
    assert(b1 && a2);
    assert(a1 && !b0);

    assert(a2 == a2);
    assert(a2 != a1);
    assert(a2 != a0);
    assert(a1 != a2);
    assert(a1 == a1);
    assert(a1 != a0);
    assert(a0 != a2);
    assert(a0 != a1);
    assert(a0 == a0);
    assert(a0 < a1);
    assert(a0 < a2);
    assert(a1 < a2);

#if 0 // should not compile
    assert(a2 > a0);
    assert(a2 > a1);
#endif

#if 0 // should not compile
    assert(b2 == b2);
    assert(b2 != b1);
    assert(b2 != b0);
    assert(b1 != b2);
    assert(b1 == b1);
    assert(b1 != b0);
    assert(b0 != b2);
    assert(b0 != b1);
    assert(b0 == b0);
#endif

#if 0 // should not compile
    assert(aa2 != a1);
    assert(aa1 == a1);
    assert(!(aa1 != a1));
    assert(!(aa2 == a1));
#endif
#if 1 // should compile with explicit comparison operator
    assert(a2 != a1);
    assert(a1 == a1);
    assert(!(a1 != a1));
    assert(!(a2 == a1));
    assert(a1 < a2);
    assert(!(a1 < a1));
#endif

    CConstRef<CObject> o0(new CObject);
    CRef<CObjA> oa(new CObjA);
    const CRef<CObjA> coa(oa);
    CConstRef<CObjA> oca(oa);
    CConstRef<CObjB> ob(new CObjB);

    const CObject* o0p = o0;
    const CObjA* oapc = oa;
    CObjA* oap = oa;
    const CObjB* obp = ob;
    const CObject* oap0c = oa;
    CObject* oap0 = oa;
    const CObject* obp0 = ob;

    assert(o0 && oa);
    assert(oa || ob);
    assert(ob || !o0);

    assert(o0 == o0p);
    assert(o0p == o0);
    assert(o0 != oap0c);
    assert(oap0c != o0);
    assert(o0 != obp0);
    assert(obp0 != o0);

    assert(oa == oapc);
    assert(oapc == oa);
    assert(oa == oap);
    assert(oap == oa);
    assert(oa == oap0c);
    assert(oap0c == oa);

    assert(coa == oapc);
    assert(oapc == coa);
#if 1
    assert(coa == oap); // fails on Sun
    assert(oap == coa); // fails on Sun
#endif
    assert(coa == oap0c);
    assert(oap0c == coa);

    assert(oa == coa);
    assert(coa <= oa);
    assert(oa >= coa);
    assert(oa == oca);
    assert(oca <= oa);
    assert(oa == oca);
    assert(coa == oca);
    assert(oca <= coa);
    assert(coa >= oca);

    assert(ob == obp);
    assert(obp == ob);
    assert(ob == obp0);
    assert(obp0 == ob);

    assert(o0 != oa);
    assert(o0 != ob);

    assert(!a0);

    set< CRef<CObject> > obj_set;
    obj_set.insert(Ref(new CObject));
    set< CConstRef<CObject> > cobj_set;
    cobj_set.insert(o0);
    assert(*cobj_set.begin() == o0);

#if 1 // this code produces errors on Sun C++ compiler
    assert(oa != o0);
    assert(ob != o0);
    assert(!(oa == o0));
    assert(!(ob == o0));
    assert(oa == coa);
    assert(oa == oca);
    assert(!(oa != coa));
    assert(!(oa != oca));
#endif

#if 0
    delete oa; // should not compile, but alas :(
    delete ob; // should not compile, but alas :(
#endif
#if 0
    delete a0; // should not compile
    delete b1; // should not compile
#endif
#if 0
    assert(a0 != b0); // should not compile
#endif
#if 0
    assert(a2 == o0); // should not compile
    assert(ob != oa); // should not compile
    assert(o0 == a0); // should not compile
#endif
#if 0
    assert(a0 + 1); // should not compile
    assert(a0 <= a1); // should not compile, but alas :(
    assert(b0 > b2); // should not compile, but alas :(
    assert(a0 < b0); // should not compile
    assert(a2 > o0); // should not compile
    assert(ob <= oa); // should not compile
    assert(o0 >= a0); // should not compile
#endif
#if 0
    assert(b2 - b0); // should not compile, but compiles on WorkShop
#endif
#if 0
    s = oa; // should not compile
    s = ob; // should not compile
    s = a0; // should not compile
    s = a1; // should not compile
    s = b0; // should not compile
    s = b1; // should not compile
#endif
#if 0
    void* pa0 = a0; // should not compile, but alas :(
    const void* pa1 = a1; // should not compile, but alas :(
    void* pb1 = b1; // should not compile, but alas :(
    const void* pb0 = b0; // should not compile, but alas :(
#endif

    return (b_a0+b_a1+b_a2+b_b0+b_b1+b_b2+
            b_av0+b_av1+b_av2+b_bv0+b_bv1+b_bv2) &&
        (o0p && oapc && oap && obp && oap0c && oap0 && obp0);
}


namespace {
    struct IntDeleter
    {
        explicit IntDeleter(int value)
            : m_Value(value)
            {
            }

        void Delete(int* ptr)
            {
                assert(*ptr == m_Value);
                delete ptr;
            }

        int m_Value;
    };
}

int TestSwap(void)
{
    {{
        CRef<CObjectFor<int> > r1(new CObjectFor<int>(1));
        CRef<CObjectFor<int> > r2(new CObjectFor<int>(2));
        assert(r1 != r2);
        assert(*r1 == 1 && *r2 == 2);
        swap(r1, r2);
        assert(r1 != r2);
        assert(r1->GetData() == 2 && r2->GetData() == 1);
        std::swap(r1, r2);
        assert(r1 != r2);
        assert(*r1 == 1 && *r2 == 2);
    }}

    {{
        CRef<CObjectFor<int> > r1(new CObjectFor<int>(1));
        CRef<CObjectFor<int> > r2;
        assert(r1 != r2);
        assert(*r1 == 1 && !r2);
        swap(r1, r2);
        assert(r1 != r2);
        assert(!r1 && r2->GetData() == 1);
        std::swap(r1, r2);
        assert(r1 != r2);
        assert(*r1 == 1 && !r2);
    }}

    {{
        AutoPtr<int> a1(new int(1));
        AutoPtr<int> a2(new int(2));
        assert(a1.get() != a2.get());
        assert(*a1 == 1 && *a2 == 2);
        swap(a1, a2);
        assert(a1.get() != a2.get());
        assert(*a1 == 2 && *a2 == 1);
        std::swap(a1, a2);
        assert(a1.get() != a2.get());
        assert(*a1 == 1 && *a2 == 2);
    }}

    {{
        AutoPtr<int, IntDeleter> a1(new int(1), IntDeleter(1));
        AutoPtr<int, IntDeleter> a2(new int(2), IntDeleter(2));
        assert(a1.get() != a2.get());
        assert(*a1 == 1 && *a2 == 2);
        swap(a1, a2);
        assert(a1.get() != a2.get());
        assert(*a1 == 2 && *a2 == 1);
    }}

    return 1;
}


int TestAutoArray(void)
{
    AutoArray<int> a1(100);
    for (int i = 0; i < 100; ++i) {
        a1[i] = i;
    }
    for (int i = 0; i < 100; ++i) {
        assert (i == a1[i]);
    }

    return 1;
}

int main(int, char **) {
    TestPEqualTo();
    TestOperatorBool();
    TestSwap();
    TestAutoArray();
    return 0;
}
