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
 * Author:  Aleksey Grichenko
 *
 * File Description:
 *   Test for parameters in multithreaded environment
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/test_mt.hpp>
#include <corelib/ncbi_param.hpp>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

const char*    kStrParam_Default    = "StrParam Default";
const bool     kBoolParam_Default   = false;
const unsigned kUIntParam_Default   = 123;
const char*    kStaticStr_Default   = "StaticStringParam";
const double   kDoubleParam_Default = 123.456;

NCBI_PARAM_DECL(int, ParamTest, ThreadIdx);
NCBI_PARAM_DECL(string, ParamTest, StrParam);
NCBI_PARAM_DECL(bool, ParamTest, BoolParam);
NCBI_PARAM_DECL(unsigned int, ParamTest, UIntParam);
NCBI_PARAM_DECL(string, ParamTest, StaticStr);
NCBI_PARAM_DECL(int, ParamTest, NoThreadParam);
NCBI_PARAM_DECL(int, ParamTest, NoLoadParam);
NCBI_PARAM_DECL(double, ParamTest, DoubleParam);

NCBI_PARAM_DEF(int, ParamTest, ThreadIdx, 0);
NCBI_PARAM_DEF(string, ParamTest, StrParam, kStrParam_Default);
NCBI_PARAM_DEF(bool, ParamTest, BoolParam, kBoolParam_Default);
NCBI_PARAM_DEF(unsigned int, ParamTest, UIntParam, kUIntParam_Default);
NCBI_PARAM_DEF(string, ParamTest, StaticStr, kStaticStr_Default);
NCBI_PARAM_DEF_EX(int, ParamTest, NoThreadParam, 0, eParam_NoThread, 0);
NCBI_PARAM_DEF_EX(int, ParamTest, NoLoadParam, 0, eParam_NoLoad, 0);
NCBI_PARAM_DEF(double, ParamTest, DoubleParam, kDoubleParam_Default);


// User-defined type

struct STestStruct
{
    STestStruct(void) : first(0), second(0) {}
    STestStruct(int f, int s) : first(f), second(s) {}
    bool operator==(const STestStruct& val)
    { return first == val.first  &&  second == val.second; }
    int first, second;
};

CNcbiIstream& operator>>(CNcbiIstream& in, STestStruct val)
{
    in >> val.first >> val.second;
    return in;
}

CNcbiOstream& operator<<(CNcbiOstream& out, const STestStruct val)
{
    out << val.first << ' ' << val.second;
    return out;
}

const STestStruct kDefaultPair(123, 456);

NCBI_PARAM_DECL(STestStruct, ParamTest, Struct);
NCBI_PARAM_DEF(STestStruct, ParamTest, Struct, kDefaultPair);

/////////////////////////////////////////////////////////////////////////////
//  Test application

class CTestParamApp : public CThreadedApp
{
public:
    virtual bool Thread_Run(int idx);
protected:
    virtual bool TestApp_Init(void);
    virtual bool TestApp_Exit(void);
private:
    typedef NCBI_PARAM_TYPE(ParamTest, ThreadIdx) TParam_ThreadIdx;
    typedef NCBI_PARAM_TYPE(ParamTest, StrParam) TParam_StrParam;
    typedef NCBI_PARAM_TYPE(ParamTest, BoolParam) TParam_BoolParam;
    typedef NCBI_PARAM_TYPE(ParamTest, UIntParam) TParam_UIntParam;
    typedef NCBI_PARAM_TYPE(ParamTest, Struct) TParam_Struct;
    typedef NCBI_PARAM_TYPE(ParamTest, NoThreadParam) TParam_NoThread;
    typedef NCBI_PARAM_TYPE(ParamTest, NoLoadParam) TParam_NoLoad;
    typedef NCBI_PARAM_TYPE(ParamTest, DoubleParam) TParam_DoubleParam;
};


typedef NCBI_PARAM_TYPE(ParamTest, StaticStr) TParam_StaticStr;
static TParam_StaticStr s_StrParam;

string InitIntParam(void);

NCBI_PARAM_DECL(int, ParamTest, InitFuncParam);
NCBI_PARAM_DEF_WITH_INIT(int, ParamTest, InitFuncParam, 123, InitIntParam);
typedef NCBI_PARAM_TYPE(ParamTest, InitFuncParam) TParam_InitFunc;

string InitIntParam(void)
{
    return "456";
}


bool CTestParamApp::Thread_Run(int idx)
{
    // Check params initialized by a function
    _ASSERT(TParam_InitFunc::GetDefault() == 456);

    // Set thread default value
    TParam_ThreadIdx::SetThreadDefault(idx);
    string str_idx = NStr::IntToString(idx);

    _ASSERT(TParam_StrParam::GetDefault() == kStrParam_Default);
    _ASSERT(s_StrParam.Get() == kStaticStr_Default);

    // Non-initializable param must always have the same value
    _ASSERT(TParam_NoLoad::GetDefault() == 0);
    // No-thread param has no thread local value
    bool error = false;
    try {
        TParam_NoThread::SetThreadDefault(idx + 1000);
    }
    catch (CParamException&) {
        error = true;
    }
    _ASSERT(error);
    TParam_NoThread::SetDefault(idx);
    _ASSERT(TParam_NoThread::GetThreadDefault() != idx + 1000);

    // Initialize parameter with global default
    TParam_StrParam str_param1;
    // Set new thread default
    TParam_StrParam::SetThreadDefault(kStrParam_Default + str_idx);
    // Initialize parameter with thread default
    TParam_StrParam str_param2;

    // Parameters should keep the value set during initialization
    _ASSERT(str_param1.Get() == kStrParam_Default);
    _ASSERT(str_param2.Get() == kStrParam_Default + str_idx);
    // Test local reset
    str_param2.Reset();
    _ASSERT(str_param2.Get() == TParam_StrParam::GetThreadDefault());
    // Test thread reset
    TParam_StrParam::ResetThreadDefault();
    _ASSERT(TParam_StrParam::GetThreadDefault() == kStrParam_Default);
    str_param2.Reset();
    _ASSERT(str_param2.Get() == str_param1.Get());
    // Restore thread default to global default for testing in ST mode
    TParam_StrParam::SetThreadDefault(kStrParam_Default);

    // Set thread default value
    bool odd = idx % 2 != 0;
    TParam_BoolParam::SetThreadDefault(odd);
    TParam_BoolParam bool_param1;
    // Check if the value is correct
    _ASSERT(bool_param1.Get() == odd);
    // Change global default not to match the thread default
    TParam_BoolParam::SetDefault(!bool_param1.Get());
    // The parameter should use thread default rather than global default
    TParam_BoolParam bool_param2;
    _ASSERT(bool_param2.Get() == odd);

    TParam_Struct struct_param;
    _ASSERT(struct_param.Get() == kDefaultPair);

    // Thread default value should not change
    _ASSERT(TParam_ThreadIdx::GetThreadDefault() == idx);

    double dbl = TParam_DoubleParam::GetDefault();
    _ASSERT(dbl == kDoubleParam_Default);
    return true;
}


bool CTestParamApp::TestApp_Init(void)
{
    NcbiCout << NcbiEndl
             << "Testing parameters with "
             << NStr::IntToString(s_NumThreads)
             << " threads..."
             << NcbiEndl;

    return true;
}

bool CTestParamApp::TestApp_Exit(void)
{
    // Test resets in ST mode
    TParam_StrParam::SetDefault(string(kStrParam_Default) + "-test1");
    TParam_StrParam::ResetThreadDefault();
    _ASSERT(TParam_StrParam::GetThreadDefault() ==
        TParam_StrParam::GetDefault());
    TParam_StrParam::SetDefault(string(kStrParam_Default) + "-test2");
    // the global value must be used
    _ASSERT(TParam_StrParam::GetThreadDefault() ==
        TParam_StrParam::GetDefault());
    // Global reset
    TParam_StrParam::ResetDefault();
    _ASSERT(TParam_StrParam::GetDefault() == kStrParam_Default);

    NcbiCout << "Test completed successfully!"
             << NcbiEndl << NcbiEndl;
    return true;
}



///////////////////////////////////
// MAIN
//

int main(int argc, const char* argv[]) 
{
    return CTestParamApp().AppMain(argc, argv);
}
