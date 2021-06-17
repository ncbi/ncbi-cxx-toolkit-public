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
 * Author:  Vladimir Ivanov
 *
 * File Description:
 *      System functions
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbi_system.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbi_limits.hpp>
#include <math.h>
#include <memory>
#include <stdlib.h>

#include <common/test_assert.h>  /* This header must go last */


USING_NCBI_SCOPE;


/////////////////////////////////////////////////////////////////////////////
//
// General testsSystem information
//

static void Test_General(void)
{
    #define PRINT(msg, value, res)                        \
       cout << "    " << left << setw(35) << msg << ": "; \
       cout << right << setw(12);                         \
       if (res) {                                         \
           cout << value;                                 \
       } else {                                           \
           cout << "n/a";                                 \
       }                                                  \
       cout << endl

    LOG_POST("\nGeneral test");

    size_t mem_limit_soft = GetVirtualMemoryLimitSoft();
    size_t mem_limit_hard = GetVirtualMemoryLimitHard();

    cout << "\nSystem information:" << endl;

    PRINT("Number of CPUs total",               CSystemInfo::GetCpuCount(),                true);
    PRINT("Number of CPUs allowed to use",      CSystemInfo::GetCpuCountAllowed(),         true);
    PRINT("Total physical memory size (bytes)", CSystemInfo::GetTotalPhysicalMemorySize(), true);
    PRINT("Avail physical memory size (bytes)", CSystemInfo::GetAvailPhysicalMemorySize(), true);
    PRINT("Virtual memory limit soft (bytes)",  ((mem_limit_soft > 0) ? std::to_string(mem_limit_soft): "unlimited"), true);
    PRINT("Virtual memory limit hard (bytes)",  ((mem_limit_hard > 0) ? std::to_string(mem_limit_hard): "unlimited"), true);
    PRINT("Virtual memory page size (bytes)",   CSystemInfo::GetVirtualMemoryPageSize(),   true);
     
    cout << "\nCurrent process information:" << endl;

    CProcess::SMemoryUsage mem_usage;
    bool res = CCurrentProcess::GetMemoryUsage(mem_usage);
    
    PRINT("Memory usage (bytes)", " ", res);
    if (res) {
        PRINT("   Total memory usage",        mem_usage.total,         mem_usage.total         > 0);
        PRINT("   Total memory usage (peak)", mem_usage.total_peak,    mem_usage.total_peak    > 0);
        PRINT("   Resident set size",         mem_usage.resident,      mem_usage.resident      > 0);
        PRINT("   Resident set size (peak)",  mem_usage.resident_peak, mem_usage.resident_peak > 0);
        PRINT("   Shared memory usage",       mem_usage.shared,        mem_usage.shared        > 0);
        PRINT("   Data segment size",         mem_usage.data,          mem_usage.data          > 0);
        PRINT("   Stack data size",           mem_usage.stack,         mem_usage.stack         > 0);
        PRINT("   Text segment size",         mem_usage.text,          mem_usage.text          > 0);
        PRINT("   Shared library code size",  mem_usage.lib,           mem_usage.lib           > 0);
    }

    int proc_thread_count = CCurrentProcess::GetThreadCount();
    PRINT("Number of threads", proc_thread_count, proc_thread_count != -1);

    int soft_fd_limit, hard_fd_limit, proc_fd_count;
    proc_fd_count = CCurrentProcess::GetFileDescriptorsCount(&soft_fd_limit, &hard_fd_limit);
    PRINT("Used file descriptors",       proc_fd_count, proc_fd_count != -1);
    PRINT("File descriptors soft limit", soft_fd_limit, soft_fd_limit != -1);
    PRINT("File descriptors hard limit", hard_fd_limit, hard_fd_limit != -1);

    cout << endl;
}



/////////////////////////////////////////////////////////////////////////////
//
// User defined dump print handler
//

int s_PrintParameter = 0;

// This handler is not async-safe, and is good only for test purposes.
//
static void PrintHandler(ELimitsExitCode code, size_t limit,
                         CTime& time, TLimitsPrintParameter param) 
{
    cout << "Type         : " << 
        (code == eLEC_Memory ? "Memory limit" : "CPU limit") << endl;
    cout << "Limit value  : " << limit << endl;
    cout << "Set time     : " << time.AsString() << endl;
    if (param) {
        cout << "Our parameter: " << *(int*) param << endl;
    } else {
        cout << "Current time : " << CTime(CTime::eCurrent).AsString() << endl;
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// Memory limits
//

static void Test_MemLimit(void)
{
    LOG_POST("\nMemory limit test\n");

    const size_t kMemLimit = 500*1024;
    assert( SetMemoryLimit(kMemLimit, PrintHandler, &s_PrintParameter) );
    
    for (size_t i = 0;  i <= kMemLimit/1024;  i++) {
        s_PrintParameter++;
        int* pi = new int[1024];
        assert(pi);
    }
    _ASSERT(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// CPU time limits
//

static void Test_CpuLimit(void)
{
    LOG_POST("\nCPU time limit test\n");

    // Use our own handler
    // To use default handler, use NULL instead of PrintHandler
    assert( SetCpuTimeLimit(1, 5, PrintHandler, NULL) );

    double a = 0.0;
    while (a <= get_limits(a).max()) {
        a = sin(rand() + a);
    }
    _ASSERT(0);
}


/////////////////////////////////////////////////////////////////////////////
//
// Test application
//

class CTestApplication : public CNcbiApplication
{
public:
    virtual void Init(void);
    virtual int  Run (void);
};


void CTestApplication::Init(void)
{
    // Set error posting and tracing on maximum
    //SetDiagTrace(eDT_Enable);
    SetDiagPostFlag(eDPF_All);
    SetDiagPostLevel(eDiag_Info);

    // Create command-line argument descriptions class
    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Test some system-specific functions");

    // Describe the expected command-line arguments
    arg_desc->AddPositional("feature", "Platform-specific feature to test",
                            CArgDescriptions::eString);
    arg_desc->SetConstraint("feature", &(*new CArgAllow_Strings, "general", "mem", "cpu"));

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}


int CTestApplication::Run(void)
{
    const CArgs& args = GetArgs();

    // Specific tests
    if (args["feature"].AsString() == "general") {
        Test_General();
    }
    else if (args["feature"].AsString() == "mem") {
        Test_MemLimit();
    }
    else if (args["feature"].AsString() == "cpu") {
        Test_CpuLimit();
    }
    else {
        _TROUBLE;
    }

    return 0;
}

  
int main(int argc, const char* argv[])
{
    return CTestApplication().AppMain(argc, argv);
}
