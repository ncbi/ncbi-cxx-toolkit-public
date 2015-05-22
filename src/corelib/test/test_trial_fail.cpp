//#define NCBI_TEST_APPLICATION
#include <ncbi_pch.hpp>
#include <corelib/ncbicfg.h>
#include <corelib/error_codes.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_safe_static.hpp>

#if !(defined(NCBI_STRICT_CTYPE_ARGS) && (defined(isalpha) || defined(NCBI_STRICT_CTYPE_ARGS_ACTIVE)))

#include <cctype>

#define NCBI_DEFINE_CTYPE_FUNC(ncbi_name, name)                         \
    inline int name(Uchar c) { return name(int(c)); }                   \
    inline int name(char c) { return name(Uchar(c)); }                  \
    template<class C> inline int name(C c)                              \
    { return See_the_standard_on_proper_argument_type_for_ctype_functions(c); }

NCBI_DEFINE_CTYPE_FUNC(NCBI_isalpha, isalpha)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isalnum, isalnum)
NCBI_DEFINE_CTYPE_FUNC(NCBI_iscntrl, iscntrl)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isdigit, isdigit)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isgraph, isgraph)
NCBI_DEFINE_CTYPE_FUNC(NCBI_islower, islower)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isprint, isprint)
NCBI_DEFINE_CTYPE_FUNC(NCBI_ispunct, ispunct)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isspace, isspace)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isupper, isupper)
NCBI_DEFINE_CTYPE_FUNC(NCBI_isxdigit, isxdigit)
NCBI_DEFINE_CTYPE_FUNC(NCBI_tolower, tolower)
NCBI_DEFINE_CTYPE_FUNC(NCBI_toupper, toupper)
//NCBI_DEFINE_CTYPE_FUNC(NCBI_isblank, isblank)
//NCBI_DEFINE_CTYPE_FUNC(NCBI_isascii, isascii)
//NCBI_DEFINE_CTYPE_FUNC(NCBI_toascii, toascii)

#undef NCBI_DEFINE_CTYPE_FUNC

#endif


#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    NcbiCout << "isupper(int('A')) = " << isupper(int('A')) << NcbiEndl;
    NcbiCout << "isupper(Uchar('A')) = " << isupper(Uchar('A')) << NcbiEndl;
    NcbiCout << "isupper('A') = " << isupper('A') << NcbiEndl;
    _ASSERT(isupper(int('A')));
    _ASSERT(isupper(Uchar('A')));
    _ASSERT(isupper('A'));
    _ASSERT(std::isupper(int('A')));
    _ASSERT(std::isupper(Uchar('A')));
    _ASSERT(std::isupper('A'));
    _ASSERT(::isupper(int('A')));
    _ASSERT(::isupper(Uchar('A')));
    _ASSERT(::isupper('A'));
    _ASSERT(toupper('A') == 'A');
    _ASSERT(std::toupper('A') == 'A');
    _ASSERT(::toupper('A') == 'A');
#if 1 // should fail
# if defined(NCBI_STRICT_GI) && defined(NCBI_INT8_GI)
    // test only if CStrictGi is on
    TGi gi;
    //gi = 2;
    const TIntId& id = gi;
    gi = GI_CONST(id);
    NcbiCout << "GI = " << gi << NcbiEndl;
# else
    _ASSERT(toupper('A'=='A'));
# endif
#endif
    NcbiCout << "Passed" << NcbiEndl;
    return 1;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv);
}
