#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbifloat.h>
#include <math.h>

#include <common/test_assert.h>  /* This header must go last */

#if defined(NCBI_OS_MSWIN)
/**
 * Define value of finite (Is Finite).
 *
 * Checks whether given double-precision floating point value is finite
 */
#   define isfinite _finite
#endif

#define finite isfinite

USING_NCBI_SCOPE;

class CTestApplication : public CNcbiApplication
{
public:
    virtual int Run(void);
};


int CTestApplication::Run(void)
{
    _ASSERT(isfinite(0.));
    _ASSERT(isfinite(0.f));
    _ASSERT(isfinite(DBL_MAX));
    _ASSERT(isfinite(FLT_MAX));
    _ASSERT(isfinite(DBL_MIN));
    _ASSERT(isfinite(FLT_MIN));
    double nan = sqrt(-1.);
    _ASSERT(!isfinite(nan));
    _ASSERT(!isfinite(HUGE_VAL));
    _ASSERT(!isfinite(-HUGE_VAL));
    NcbiCout << "Passed" << NcbiEndl;
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN

int main(int argc, const char* argv[]) 
{
    return CTestApplication().AppMain(argc, argv);
}
