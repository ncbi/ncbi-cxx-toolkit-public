#ifndef NCBIAPP__HPP
#define NCBIAPP__HPP

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
*
* File Description:
*   CNcbiApplication -- a generic NCBI application class
*   CCgiApplication  -- a NCBI CGI-application class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  1998/12/07 23:46:52  vakatov
* Merged with "cgiapp.hpp";  minor fixes
*
* Revision 1.5  1998/12/07 22:31:12  vakatov
* minor fixes
*
* Revision 1.4  1998/12/03 21:24:21  sandomir
* NcbiApplication and CgiApplication updated
*
* Revision 1.3  1998/12/01 19:12:36  lewisg
* added CCgiApplication
*
* Revision 1.2  1998/11/05 21:45:13  sandomir
* std:: deleted
*
* Revision 1.1  1998/11/02 22:10:12  sandomir
* CNcbiApplication added; netest sample updated
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <ncbicgi.hpp>


BEGIN_NCBI_SCOPE

///////////////////////////////////////////////////////
// CNcbiApplication
//

class CNcbiApplication {
public:
    static CNcbiApplication* Instance(void); // Singleton method

    // (throw exception if not-only instance)
    CNcbiApplication(int argc=0, char** argv=0);
    virtual ~CNcbiApplication(void);

    virtual void Init(void); // initialization
    virtual void Exit(void); // cleanup
    virtual int Run(void) = 0; // main loop
  
protected: 
    static CNcbiApplication* m_Instance;
    int    m_Argc;
    char** m_Argv;
};


///////////////////////////////////////////////////////
// CCgiApplication
//

class CCgiApplication: public CNcbiApplication {
public:
  static CCgiApplication* Instance(void); // Singleton method

  CCgiApplication(int argc=0, char** argv=0,
                  CNcbiIstream* istr=0, bool indexes_as_entries=true);
  virtual ~CCgiApplication(void);

  virtual void Init(void); // initialization
  virtual void Exit(void); // cleanup

  const CCgiRequest* GetRequest(void) const  { return m_CgiRequest; }

protected:
  CCgiRequest*  m_CgiRequest;
  CNcbiIstream* m_Istr;
  bool          m_Iase; // indexes_as_entries
};

END_NCBI_SCOPE

#endif // NCBIAPP__HPP


