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
 * Authors:  Andrei Gourianov,
 *
 * File Description:   Test CNcbiException and based classes
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbithr.hpp>
#include <errno.h>

#include <common/test_assert.h>  /* This header must go last */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// CExceptionSubsystem

#if defined(EXCEPTION_BUG_WORKAROUND)
class CSubsystemException : public CException
#else
class CSubsystemException : virtual public CException
#endif
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
        default:     return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CSubsystemException,CException);
};

/////////////////////////////////////////////////////////////////////////////
// CSupersystemException

class CSupersystemException : public CSubsystemException
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
        default:      return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CSupersystemException, CSubsystemException);
};

/////////////////////////////////////////////////////////////////////////////
// CIncompletelyImplementedException

class CIncompletelyImplementedException : public CSupersystemException
{
public:
    CIncompletelyImplementedException(const CSupersystemException& e)
        : CSupersystemException(e)
        { }
};


class CErrnoMoreException : public CErrnoTemplException<CCoreException>
{
public:
    enum EErrCode {
        eMore1,
        eMore2
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eMore1: return "eMore1";
        case eMore2: return "eMore2";
        default:     return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CErrnoMoreException,CErrnoTemplException<CCoreException>);
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

    void s1(void);
    void s2(void);

    void t1(void);
    void m1(void);

    void tp1(void);
private:
    virtual int  Run(void);
};

//---------------------------------------------------------------------------
void CExceptApplication::f1(void)
{
    try {
        f2();
    }
    catch (CException& e) {  // catch by reference
        // verify error code
        assert(e.GetErrCode() == CException::eInvalid);
        // verify exception class
        assert(!UppermostCast<CException>(e));
        assert(UppermostCast<CSubsystemException>(e));
        assert(!UppermostCast<CSupersystemException>(e));
        // verify error code
        const CSubsystemException *pe = UppermostCast<CSubsystemException>(e);
        assert(pe->GetErrCode() == CSubsystemException::eType2);

        NCBI_RETHROW_SAME(e,"calling f2 from f1");
    }
}

void CExceptApplication::f2(void)
{
    try {
        f3();
    }
    catch (CSubsystemException e) {  // catch by value
/*
    catching exception by value results in copying
    CSupersystemException into CSubsystemException and
    loosing all meaning of the "original" exception
    i.e. only location and message info is preserved
    while err.code becomes invalid
*/
        // verify error code
        assert((int)e.GetErrCode() == (int)CException::eInvalid);

        NCBI_RETHROW(e,CSubsystemException,eType2,"calling f3 from f2");
    }
}

void CExceptApplication::f3(void)
{
    try {
        f4();
    }
    catch (CSubsystemException& e) {  // catch by reference
        // verify error code
        assert((int)(e.GetErrCode()) == (int)CException::eInvalid);
        // verify exception class
        assert(!UppermostCast<CException>(e));
        assert(!UppermostCast<CSubsystemException>(e));
        assert(UppermostCast<CSupersystemException>(e));
        // verify error code
        const CSupersystemException *pe = UppermostCast<CSupersystemException>(e);
        assert(pe->GetErrCode() == CSupersystemException::eSuper2);

        // error code string is always correct
        assert( strcmp(e.GetErrCodeString(),
                        pe->GetErrCodeString())==0);

        // NCBI_RETHROW_SAME(e,"calling f4 from f3");
        e.AddBacklog(DIAG_COMPILE_INFO, "calling f4 from f3", e.GetSeverity());
        e.Throw();
    }
}

void CExceptApplication::f4(void)
{
//    NCBI_THROW(CSupersystemException,eSuper2,"from f4");

    NCBI_EXCEPTION_VAR(f4_ex, CSupersystemException, eSuper2, "from f4");
    f4_ex.SetSeverity(eDiag_Critical);
    try {
        CIncompletelyImplementedException f4_ex2(f4_ex);
        f4_ex2.Throw(); // slices (and warns about doing so)
    } catch (CSupersystemException& e) {
        assert(UppermostCast<CSupersystemException>(e));
        // ErrCode trashed, though it would have been safe to keep in
        // this case.
        assert((int)e.GetErrCode() == (int)CException::eInvalid);
    } catch (...) {
        assert(0);
    }
    NCBI_EXCEPTION_THROW(f4_ex);

//    throw CSupersystemException(DIAG_COMPILE_INFO, 0, CSupersystemException::eSuper2, "from f4", eDiag_Warning);
}

void CExceptApplication::t1(void)
{
    NCBI_THROW(CErrnoTemplException<CCoreException>,eErrno,"from t1");
}

void CExceptApplication::m1(void)
{
    NCBI_THROW(CErrnoMoreException,eMore2,"from m1");
}

void CExceptApplication::tp1(void)
{
    NCBI_THROW2(CParseTemplException<CCoreException>,eErr,"from tp1",123);
}

//---------------------------------------------------------------------------

int CExceptApplication::Run(void)
{

    try {
        f1();
    }
    catch (CException& e) {

// Attributes
        // verify error code
        assert(e.GetErrCode() == CException::eInvalid);
        // verify exception class
        assert(!UppermostCast<CException>(e));
        assert(UppermostCast<CSubsystemException>(e));
        assert(!UppermostCast<CSupersystemException>(e));
        // verify error code
        assert(UppermostCast<CSubsystemException>(e)->GetErrCode() ==
                CSubsystemException::eType2);


        // verify predecessors
        const CException* pred;


        pred = e.GetPredecessor();
        assert(pred);
        assert(pred->GetErrCode() == CException::eInvalid);
        // verify exception class
        assert(!UppermostCast<CException>(*pred));
        assert(UppermostCast<CSubsystemException>(*pred));
        assert(!UppermostCast<CSupersystemException>(*pred));
        // verify error code
        assert(UppermostCast<CSubsystemException>(*pred)->GetErrCode() ==
                CSubsystemException::eType2);


        pred = pred->GetPredecessor();
        assert(pred);
        assert(pred->GetErrCode() == CException::eInvalid);
        // verify exception class
        assert(!UppermostCast<CException>(*pred));
        assert(UppermostCast<CSubsystemException>(*pred));
        assert(!UppermostCast<CSupersystemException>(*pred));
        // verify error code
        assert((int)UppermostCast<CSubsystemException>(*pred)->GetErrCode() ==
                (int)CException::eInvalid);


        pred = pred->GetPredecessor();
        assert(pred);
        assert(pred->GetErrCode() == CException::eInvalid);
        // verify exception class
        assert(!UppermostCast<CException>(*pred));
        assert(!UppermostCast<CSubsystemException>(*pred));
        assert(UppermostCast<CSupersystemException>(*pred));
        // verify error code
        assert(UppermostCast<CSupersystemException>(*pred)->GetErrCode() ==
                CSupersystemException::eSuper2);



// Reporting
        cerr << endl;
        ERR_POST("****** ERR_POST ******" << e << "err_post ends");

        cerr << endl << "****** e.ReportAll() ******" << endl;
        cerr << e.ReportAll();

        cerr << endl << "****** e.what() ******" << endl;
        cerr << e.what();

        cerr << endl << "****** e.ReportThis() ******" << endl;
        cerr << e.ReportThis() << endl;

        CExceptionReporterStream reporter(cerr);
        CExceptionReporter::SetDefault(&reporter);
        CExceptionReporter::EnableDefault(false);

        cerr << endl;
        e.Report(DIAG_COMPILE_INFO,
            "****** stream reporter ******", &reporter, eDPF_All);
        cerr << endl;
        NCBI_REPORT_EXCEPTION(
            "****** default reporter (stream, disabled) ******",e);

        CExceptionReporter::EnableDefault(true);
        cerr << endl;
        NCBI_REPORT_EXCEPTION(
            "****** default reporter (stream) ******",e);

        CExceptionReporter::SetDefault(0);
        cerr << endl;
        NCBI_REPORT_EXCEPTION(
            "****** default reporter (diag) ******",e);
    }
    catch (exception& /*e*/) {
        assert(0);
    }


    try {
        t1();
    } catch (CErrnoTemplException<CCoreException>& e) {
        NCBI_REPORT_EXCEPTION("caught as CErrnoTemplException<CCoreException>", e);
        assert(e.GetErrCode() == CErrnoTemplException<CCoreException>::eErrno);
    } catch (CCoreException& e) {
        NCBI_REPORT_EXCEPTION("caught as CCoreException", e);
        const CErrnoTemplException<CCoreException>* pe = UppermostCast< CErrnoTemplException<CCoreException> > (e);
        assert(pe->GetErrCode() == CErrnoTemplException<CCoreException>::eErrno);
    } catch (exception&) {
        assert(0);
    }

    try {
        m1();
    } catch (CErrnoTemplException<CCoreException>& e) {
        NCBI_REPORT_EXCEPTION("caught as CErrnoTemplException<CCoreException>", e);
        assert((int)e.GetErrCode() == (int)CException::eInvalid);
    } catch (CCoreException e) {
        NCBI_REPORT_EXCEPTION("caught as CCoreException", e);
        assert((int)e.GetErrCode() == (int)CException::eInvalid);
    } catch (exception&) {
        assert(0);
    }

    try {
        tp1();
    } catch (CParseTemplException<CCoreException>& e) {
        NCBI_REPORT_EXCEPTION("caught as CParseTemplException<CCoreException>", e);
        assert(e.GetErrCode() == CParseTemplException<CCoreException>::eErr);
    } catch (exception&) {
        assert(0);
    }

    LOG_POST("Test completed");
    return 0;
}

END_NCBI_SCOPE



/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CExceptApplication().AppMain(argc, argv);
}
