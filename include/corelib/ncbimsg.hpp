#ifndef NCBIERR__HPP
#define NCBIERR__HPP

/*  $RCSfile$  $Revision$  $Date$
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
* Author:  Denis Vakatov
*
* File Description:
*   NCBI Error Handling API for C++(using STL)
*
* --------------------------------------------------------------------------
* $Log$
* Revision 1.3  1998/09/25 22:38:10  vakatov
* *** empty log message ***
*
* Revision 1.2  1998/09/25 19:35:39  vakatov
* *** empty log message ***
*
* Revision 1.1  1998/09/24 22:10:48  vakatov
* Initial revision
*
* ==========================================================================
*/

#include <iostream>
#include <strstream>
#include <string>

using namespace std;

namespace ncbi_err {

    typedef enum {
        eE_Info = 0,
        eE_Warning,
        eE_Error,
        eE_Fatal   /* guarantees to exit(or abort) */ 
    } EErrSeverity;


    class CError {
    public:
        CError(void);
        CError(EErrSeverity sev, const char* message=0, bool flush=true);
        ~CError(void) { f_Flush(); }

        template<class X> CError& operator << (X& x);  // formatted output
        CError& f_Clear(void);  // reset the current message
        CError& f_Flush(void);  // flush out the current message
        // flush curr. message;  then start new one with the specified severity
        CError& f_Severity(EErrSeverity severity);

        // Write the error diagnostics to output stream "os"
        extern void f_SetStream(ostream& os=cerr);
        // Do not post messages which severity is less than "severity"
        extern void f_SetPostLevel(EErrSeverity severity=eE_Error);
        // Abrupt the application if severity is >= "severity"
        extern void f_SetDieLevel(EErrSeverity severity=eE_Fatal);

        // (for the error stream manipulators)
        CError& operator << (CError& (*f)(CError&)) { return f(*this); }

    private:
        EErrSeverity m_Severity;
        ostrstream   m_Buffer;
    };

    // Set of output manipulators for CError
    //
    extern CError& Flush  (CError& err) { return err.f_Flush(err); }
    extern CError& Info   (CError& err) { return err.f_Severity(eE_Info); }
    extern CError& Warning(CError& err) { return err.f_Severity(eE_Warning); }
    extern CError& Error  (CError& err) { return err.f_Severity(eE_Error); }
    extern CError& Fatal  (CError& err) { return err.f_Severity(eE_Fatal); }




    ////////////////////////////////////////////////////////////////////
    // THE BELOW IS MOSTLY FOR INTERNAL USE -- OR FOR ANYBODY WHO
    // WANT TO PROVIDE HIS OWN ERROR-HANDLE CALLBACKS
    ////////////////////////////////////////////////////////////////////


    // Type of the callback for "CError::f_Flush()"
    // Return "true" if succeeded
    typedef bool (*FFlushHook)(EErrSeverity severity, const char* message,
                               void* data);

    // The function to call when the hook is reset
    typedef void (*FFlushDone)(void* data);

    // The flush-hook callback and its data
    class CFlushHook {
    public:
        FFlushHook f_Hook;
        void*      m_Data;
        FFlushDone f_Done;
        CFlushHook(FFlushHook hook, void* data=0, FFlushDone done=0)
            { f_Hook = hook;  m_Data = data;  f_Done = done; }
    private:
        CFlushHook(void) { NO_GOOD; }        
    };

    // Set new hook function&data to be called whenever the "f_Flush()"
    // method gets called for any instance of "CError" in the current
    // thread
    // NOTE:  "f_Done()" will be called for the replaced hook(if any)
    extern void g_SetFlushHook(const SFlushHook& hook);



    ///////////////////////////////////////////////////////
    // All inline function implementations are in this file
#include <ncbierr.inl>

}  // namespace ncbi_err

#endif  /* NCBIERR__HPP */
