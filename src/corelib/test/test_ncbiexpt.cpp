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
 * File Description:
 *   test CNcbiException
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbithr.hpp>
#include <errno.h>


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
        _ASSERT(e.GetErrCode() == CException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CException>(e));
        _ASSERT(UppermostCast<CSubsystemException>(e));
        _ASSERT(!UppermostCast<CSupersystemException>(e));
        // verify error code
        const CSubsystemException *pe = UppermostCast<CSubsystemException>(e);
        _ASSERT(pe->GetErrCode() == CSubsystemException::eType2);

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
        _ASSERT((int)e.GetErrCode() == (int)CException::eInvalid);

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
        _ASSERT((int)(e.GetErrCode()) == (int)CException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CException>(e));
        _ASSERT(!UppermostCast<CSubsystemException>(e));
        _ASSERT(UppermostCast<CSupersystemException>(e));
        // verify error code
        const CSupersystemException *pe = UppermostCast<CSupersystemException>(e);
        _ASSERT(pe->GetErrCode() == CSupersystemException::eSuper2);

        // error code string is always correct
        _ASSERT( strcmp(e.GetErrCodeString(),
                        pe->GetErrCodeString())==0);

        NCBI_RETHROW_SAME(e,"calling f4 from f3");
    }
}

void CExceptApplication::f4(void)
{
    NCBI_THROW(CSupersystemException,eSuper2,"from f4");
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
        _ASSERT(e.GetErrCode() == CException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CException>(e));
        _ASSERT(UppermostCast<CSubsystemException>(e));
        _ASSERT(!UppermostCast<CSupersystemException>(e));
        // verify error code
        _ASSERT(UppermostCast<CSubsystemException>(e)->GetErrCode() ==
                CSubsystemException::eType2);


        // verify predecessors
        const CException* pred;


        pred = e.GetPredecessor();
        _ASSERT(pred);
        _ASSERT(pred->GetErrCode() == CException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CException>(*pred));
        _ASSERT(UppermostCast<CSubsystemException>(*pred));
        _ASSERT(!UppermostCast<CSupersystemException>(*pred));
        // verify error code
        _ASSERT(UppermostCast<CSubsystemException>(*pred)->GetErrCode() ==
                CSubsystemException::eType2);


        pred = pred->GetPredecessor();
        _ASSERT(pred);
        _ASSERT(pred->GetErrCode() == CException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CException>(*pred));
        _ASSERT(UppermostCast<CSubsystemException>(*pred));
        _ASSERT(!UppermostCast<CSupersystemException>(*pred));
        // verify error code
        _ASSERT((int)UppermostCast<CSubsystemException>(*pred)->GetErrCode() ==
                (int)CException::eInvalid);


        pred = pred->GetPredecessor();
        _ASSERT(pred);
        _ASSERT(pred->GetErrCode() == CException::eInvalid);
        // verify exception class
        _ASSERT(!UppermostCast<CException>(*pred));
        _ASSERT(!UppermostCast<CSubsystemException>(*pred));
        _ASSERT(UppermostCast<CSupersystemException>(*pred));
        // verify error code
        _ASSERT(UppermostCast<CSupersystemException>(*pred)->GetErrCode() ==
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
        e.Report(__FILE__, __LINE__,
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
        _ASSERT(0);
    }


    try {
        t1();
    } catch (CErrnoTemplException<CCoreException>& e) {
        NCBI_REPORT_EXCEPTION("caught as CErrnoTemplException<CCoreException>", e);
        _ASSERT(e.GetErrCode() == CErrnoTemplException<CCoreException>::eErrno);
    } catch (CCoreException& e) {
        NCBI_REPORT_EXCEPTION("caught as CCoreException", e);
        const CErrnoTemplException<CCoreException>* pe = UppermostCast< CErrnoTemplException<CCoreException> > (e);
        _ASSERT(pe->GetErrCode() == CErrnoTemplException<CCoreException>::eErrno);
    } catch (exception&) {
        _ASSERT(0);
    }

    try {
        m1();
    } catch (CErrnoTemplException<CCoreException>& e) {
        NCBI_REPORT_EXCEPTION("caught as CErrnoTemplException<CCoreException>", e);
        _ASSERT((int)e.GetErrCode() == (int)CException::eInvalid);
    } catch (CCoreException e) {
        NCBI_REPORT_EXCEPTION("caught as CCoreException", e);
        _ASSERT((int)e.GetErrCode() == (int)CException::eInvalid);
    } catch (exception&) {
        _ASSERT(0);
    }

    try {
        tp1();
    } catch (CParseTemplException<CCoreException>& e) {
        NCBI_REPORT_EXCEPTION("caught as CParseTemplException<CCoreException>", e);
        _ASSERT(e.GetErrCode() == CParseTemplException<CCoreException>::eErr);
    } catch (exception&) {
        _ASSERT(0);
    }

    cout << "Test completed" << endl;
    return 0;
}
END_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//  MAIN

USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    // Execute main application function
//    SetDiagPostFlag(eDPF_Trace);
    return CExceptApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}

/*
 * ===========================================================================
 * $Log$
 * Revision 6.12  2004/05/14 13:59:51  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 6.11  2003/02/24 19:56:51  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * Revision 6.10  2003/02/14 19:32:49  gouriano
 * added testing of template-based errno and parse exceptions
 *
 * Revision 6.9  2003/01/31 14:51:00  gouriano
 * corrected template-based exception definition
 *
 * Revision 6.8  2002/08/20 19:09:07  gouriano
 * more tests
 *
 * Revision 6.7  2002/07/31 18:34:14  gouriano
 * added test for virtual base class
 *
 * Revision 6.6  2002/07/29 19:30:10  gouriano
 * changes to allow multiple inheritance in CException classes
 *
 * Revision 6.5  2002/07/15 18:17:26  gouriano
 * renamed CNcbiException and its descendents
 *
 * Revision 6.4  2002/07/11 14:18:29  gouriano
 * exceptions replaced by CNcbiException-type ones
 *
 * Revision 6.3  2002/06/27 18:56:16  gouriano
 * added "title" parameter to report functions
 *
 * Revision 6.2  2002/06/27 13:53:40  gouriano
 * added standard NCBI info
 *
 *
 * ===========================================================================
 */
