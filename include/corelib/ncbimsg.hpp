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
* Revision 1.5  1998/09/29 20:26:01  vakatov
* Redesigning #1
*
* Revision 1.4  1998/09/25 22:58:01  vakatov
* *** empty log message ***
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


    class CErr {
    public:
        CErr(EErrSeverity sev=eE_Error, const char* message=0,
             bool flush=false);
        ~CErr(void);

        template<class X> CErr& operator << (X& x);  // formatted output

        // write the error diagnostics to output stream "os"
        static void f_SetStream(ostream& os=cerr);
        // do not post messages which severity is less than "min_sev"
        static void f_SetPostLevel(EErrSeverity min_sev=eE_Error);
        // abrupt the application if severity is >= "max_sev"
        static void f_SetDieLevel(EErrSeverity max_sev=eE_Fatal);

        // (for the error stream manipulators)
        CErr& operator << (CErr& (*f)(CErr&)) { return f(*this); }
        
        EErrSeverity f_GetSeverity(void);

    private:
        EErrSeverity m_Severity;  // severity level for the current message
        CErrBuffer&  m_Buffer;    // this thread's error message buffer
    };

    // Set of output manipulators for CErr
    //
    inline CErr& Reset  (CErr& e);  // reset the content of current message
    inline CErr& EndMess(CErr& e);  // write out current message, start new one

    inline CErr& Info   (CErr& e);  ///  these 4 manipulators first do a flush 
    inline CErr& Warning(CErr& e);  ///  and then set severity for the next
    inline CErr& Error  (CErr& e);  ///  message
    inline CErr& Fatal  (CErr& e);  ///


    //////////////////////////////////////////////////////////////////
    //
    class CErrBuffer {
        friend class CErr;
    public:
        CErrBuffer(void);
        CErrBuffer(EErrSeverity sev, const char* message, bool flush=true);
        ~CErrBuffer(void);

        template<class X> CErrBuffer& operator << (X& x);  // formatted output
        void f_Clear(void);  // reset current message
        void f_Flush(void);  // flush out current message
        // flush curr. message;  then start new one with the specified severity
        void f_Severity(EErrSeverity sev);

        // write the error diagnostics to output stream "os"
        extern void f_SetStream(ostream& os=cerr);
        // do not post messages which severity is less than "min_sev"
        extern void f_SetPostLevel(EErrSeverity min_sev=eE_Error);
        // abrupt the application if severity is >= "max_sev"
        extern void f_SetDieLevel(EErrSeverity max_sev=eE_Fatal);

        // (for the error stream manipulators)
        CErrBuffer& operator << (CErrBuffer& (*f)(CErrBuffer&)) { return f(*this); }

    private:
        const CErr* m_Err;     // 
        ostrstream  m_Stream;  // content of the current message
    };




    ////////////////////////////////////////////////////////////////////
    // THE API BELOW IS MOSTLY FOR INTERNAL USE -- OR FOR ANYBODY WHO
    // WANT TO PROVIDE HIS OWN ERROR-HANDLE CALLBACKS
    ////////////////////////////////////////////////////////////////////


    // Type of the callback for "CErrBuffer::f_Flush()"
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
    // method gets called for any instance of "CErrBuffer" in the current
    // thread
    // NOTE:  "f_Done()" will be called for the replaced hook(if any)
    extern void g_SetFlushHook(const SFlushHook& hook);



    ///////////////////////////////////////////////////////
    // All inline function implementations are in this file
#include <ncbierr.inl>

}  // namespace ncbi_err

#endif  /* NCBIERR__HPP */
