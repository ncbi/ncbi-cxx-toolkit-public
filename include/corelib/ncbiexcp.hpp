#ifndef NCBIEXCP__HPP
#define NCBIEXCP__HPP

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
* Author: 
*	Vsevolod Sandomirskiy
*        Denis Vakatov
*
* File Description:
*   NCBI OS-independent C++ exceptions
*   To be used to catch UNIX sync signals or Win32 SEH exceptions
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  1998/11/05 21:45:13  sandomir
* std:: deleted
*
* Revision 1.1  1998/11/02 15:31:53  sandomir
* Portable exception classes
*
* ===========================================================================
*/

#include <ncbistd.hpp>

#include <string>
#include <stdexcept>

// Portable generic exception
class CNcbiOSException : public exception
{
public:
    
    CNcbiOSException( const string& what ) throw()
        : m_what( what ) {}
    
    virtual const char *what() const throw()
        { return m_what.c_str(); }
    
    // NOTE: set_unexpected() does not work with VisualC++ 5.0
    static void UnexpectedHandler() throw( bad_exception );
    
    // OS depenedent initialization
    static void SetDefHandler() throw( runtime_error );
    
protected:
    
    string m_what;
};

// Portable memory-related exception
class CNcbiMemException : public CNcbiOSException
{
public:
    CNcbiMemException( const string& what ) throw()
        : CNcbiOSException( what ) {}
};

// Portable FPE-related exception
class CNcbiFPEException : public CNcbiOSException
{
public:
    CNcbiFPEException( const string& what ) throw()
        : CNcbiOSException( what ) {}
};

// Portable other exception
class CNcbiSystemException : public CNcbiOSException
{
public:
    CNcbiSystemException( const string& what ) throw()
        : CNcbiOSException( what ) {}
};

//
// diag
//

// Standard handling of "exception"-derived exceptions
#define STD_CATCH(message) \
catch (exception& e) \
{ \
      CNcbiDiag diag; \
      diag << Error << "[" << message << "]" << "Exception: " << e.what(); \
}

// Standard handling of "exception"-derived and all other exceptions
#define STD_CATCH_ALL(message) \
STD_CATCH(message) \
    catch (...) \
{ \
      CNcbiDiag diag; \
      diag << Error << "[" << message << "]" << "Unknown exception"; \
}

#endif  /* NCBIEXCP__HPP */

