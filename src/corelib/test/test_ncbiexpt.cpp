#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbithr.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// CExceptSubsystem

class CExceptSubsystem : public CNcbiException
{
public:
    enum EErrCode {
        eType1,
        eType2
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eType1: return "eType1";
        case eType2: return "eType2";
        default:     return CNcbiException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CExceptSubsystem,CNcbiException);
};

/////////////////////////////////////////////////////////////////////////////
// CExceptSupersystem

class CExceptSupersystem : public CExceptSubsystem
{
public:
    enum EErrCode {
        eSuper1,
        eSuper2
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eSuper1: return "eSuper1";
        case eSuper2: return "eSuper2";
        default:      return CNcbiException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CExceptSupersystem, CExceptSubsystem);
};


/////////////////////////////////////////////////////////////////////////////
//  CExceptApplication::

class CExceptApplication : public CNcbiApplication
{
private:
    void f1(void);
    void f2(void);
    void f3(void);
    void f4(void);
private:
    virtual int  Run(void);
};

void CExceptApplication::f1(void)
{
    try {
        f2();
    }
    catch (CNcbiException& e) {  // catch by reference
        // verify error code
        _ASSERT(e.GetErrCode() == CNcbiException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CNcbiException>(e));
        _ASSERT(UppermostCast<CExceptSubsystem>(e));
        _ASSERT(!UppermostCast<CExceptSupersystem>(e));
        // verify error code
        const CExceptSubsystem *pe = UppermostCast<CExceptSubsystem>(e);
        _ASSERT(pe->GetErrCode() == CExceptSubsystem::eType2);

        NCBI_RETHROW_SAME(e,"calling f2 from f1");
    }
}

void CExceptApplication::f2(void)
{
    try {
        f3();
    }
    catch (CExceptSubsystem e) {  // catch by value
/*
    catching exception by value results in copying
    CExceptSupersystem into CExceptSubsystem and
    loosing all meaning of the "original" exception
    i.e. only location and message info is preserved
    while err.code becomes invalid
*/
        // verify error code
        _ASSERT((int)e.GetErrCode() == (int)CNcbiException::eInvalid);

        NCBI_RETHROW(e,CExceptSubsystem,eType2,"calling f3 from f2");
    }
}

void CExceptApplication::f3(void)
{
    try {
        f4();
    }
    catch (CExceptSubsystem& e) {  // catch by reference
        // verify error code
        _ASSERT((int)(e.GetErrCode()) == (int)CNcbiException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CNcbiException>(e));
        _ASSERT(!UppermostCast<CExceptSubsystem>(e));
        _ASSERT(UppermostCast<CExceptSupersystem>(e));
        // verify error code
        const CExceptSupersystem *pe = UppermostCast<CExceptSupersystem>(e);
        _ASSERT(pe->GetErrCode() == CExceptSupersystem::eSuper2);

        // error code string is always correct
        _ASSERT( strcmp(e.GetErrCodeString(),
                        pe->GetErrCodeString())==0);

        NCBI_RETHROW_SAME(e,"calling f4 from f3");
    }
}

void CExceptApplication::f4(void)
{
    NCBI_THROW(CExceptSupersystem,eSuper2,"from f4");
}


int CExceptApplication::Run(void)
{
    try {
        f1();
    }
    catch (CNcbiException& e) {

        ERR_POST(e);

// Attributes
        // verify error code
        _ASSERT(e.GetErrCode() == CNcbiException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CNcbiException>(e));
        _ASSERT(UppermostCast<CExceptSubsystem>(e));
        _ASSERT(!UppermostCast<CExceptSupersystem>(e));
        // verify error code
        _ASSERT(UppermostCast<CExceptSubsystem>(e)->GetErrCode() ==
                CExceptSubsystem::eType2);


        // verify predecessors
        const CNcbiException* pred;


        pred = e.GetPredecessor();
        _ASSERT(pred);
        _ASSERT(pred->GetErrCode() == CNcbiException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CNcbiException>(*pred));
        _ASSERT(UppermostCast<CExceptSubsystem>(*pred));
        _ASSERT(!UppermostCast<CExceptSupersystem>(*pred));
        // verify error code
        _ASSERT(UppermostCast<CExceptSubsystem>(*pred)->GetErrCode() ==
                CExceptSubsystem::eType2);


        pred = pred->GetPredecessor();
        _ASSERT(pred);
        _ASSERT(pred->GetErrCode() == CNcbiException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CNcbiException>(*pred));
        _ASSERT(UppermostCast<CExceptSubsystem>(*pred));
        _ASSERT(!UppermostCast<CExceptSupersystem>(*pred));
        // verify error code
        _ASSERT((int)UppermostCast<CExceptSubsystem>(*pred)->GetErrCode() ==
                (int)CNcbiException::eInvalid);


        pred = pred->GetPredecessor();
        _ASSERT(pred);
        _ASSERT(pred->GetErrCode() == CNcbiException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CNcbiException>(*pred));
        _ASSERT(!UppermostCast<CExceptSubsystem>(*pred));
        _ASSERT(UppermostCast<CExceptSupersystem>(*pred));
        // verify error code
        _ASSERT(UppermostCast<CExceptSupersystem>(*pred)->GetErrCode() ==
                CExceptSupersystem::eSuper2);



// Reporting
        cerr << endl
            << "****** ERR_POST ******"
            << endl;
        ERR_POST("err_post begins " << e << "err_post ends");

        cerr << endl << "****** e.ReportAll() ******" << endl;
        cerr << e.ReportAll();

        CExceptionReporterStream reporter(cerr);
        CExceptionReporter::SetDefault(&reporter);
        CExceptionReporter::EnableDefault(false);

        cerr << endl << "****** stream reporter ******" << endl;
        e.Report(__FILE__, __LINE__,&reporter);
        cerr << endl << "****** default reporter (stream, disabled) ******" << endl;
        REPORT_NCBI_EXCEPTION(e);

        CExceptionReporter::EnableDefault(true);
        cerr << endl << "****** default reporter (stream) ******" << endl;
        REPORT_NCBI_EXCEPTION(e);

        CExceptionReporter::SetDefault(0);
        cerr << endl << "****** default reporter (diag) ******" << endl;
        REPORT_NCBI_EXCEPTION(e);
    }
    catch (exception& /*e*/) {
        _ASSERT(0);
    }
    return 0;
}
END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    // Execute main application function
    SetDiagPostFlag(eDPF_Trace);
    return CExceptApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
