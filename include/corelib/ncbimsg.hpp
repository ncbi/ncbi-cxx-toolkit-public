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
* Revision 1.6  1998/09/29 21:45:55  vakatov
* *** empty log message ***
*
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


    //////////////////////////////////////////////////////////////////
    //
    class CErr {
    public:
        CErr(EErrSeverity sev=eE_Error, const char* message=0,
             bool flush=false);
        ~CErr(void);

        template<class X> CErr& operator << (X& x) const;  // formatted output
        CErr& operator << (CErr& (*f)(CErr&));       // manipulators

        // write the error diagnostics to output stream "os"
        static void f_SetStream(ostream& os=cerr);
        // do not post messages which severity is less than "min_sev"
        static void f_SetPostLevel(EErrSeverity min_sev=eE_Error);
        // abrupt the application if severity is >= "max_sev"
        static void f_SetDieLevel(EErrSeverity max_sev=eE_Fatal);

        EErrSeverity f_GetSeverity(void) const;

    private:
        EErrSeverity m_Severity;  // severity level for the current message
        CErrBuffer&  m_Buffer;    // this thread's error message buffer
    };

    // Set of output manipulators for CErr
    //
    inline CErr& Reset  (const CErr& err); // reset content of curr. message
    inline CErr& EndMess(const CErr& err); // flush curr.message, start new one

    inline CErr& Info   (CErr& err);  ///  these 4 manipulators first do a
    inline CErr& Warning(CErr& err);  ///  flush;  then they set severity
    inline CErr& Error  (CErr& err);  ///  for the next message
    inline CErr& Fatal  (CErr& err);  ///



    //////////////////////////////////////////////////////////////////
    //
    class CErrBuffer {
        friend class CErr;
        friend CErrBuffer& g_GetErrBuffer(void);
    private:
        const CErr* m_Err;     // present user
        ostrstream  m_Stream;  // content of the current message

        CErrBuffer(void);
        ~CErrBuffer(void);

        // formatted output
        template<class X> void f_Put(const CErr& err, X& x);

        void f_Flush  (void);
        void f_Reset  (const CErr& err);  // reset current message
        void f_EndMess(const CErr& err);  // flush out current message

        // flush & detach the current user
        void f_Detach(const CErr* err);
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
