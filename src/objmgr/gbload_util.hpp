#ifndef GBLOADER_UTIL_HPP_INCLUDED
#define GBLOADER_UTIL_HPP_INCLUDED

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
*  ===========================================================================
*
*  Author: Michael Kimelman
*
*  File Description:
*   GB loader Utilities
*
* ===========================================================================
*/

#include <objects/objmgr/gbloader.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CGBLGuard
{
public:
  enum EState
    {
      eNone,
      eMain,
      eBoth,
      eLocal,
      eNoneToMain,
      eLast
    };
  typedef CGBDataLoader::SLeveledMutex TLMutex;

  CGBLGuard(TLMutex& lm,EState orig,const char *loc="",int select=-1); // just accept start position
  CGBLGuard(TLMutex& lm,const char *loc=""); // assume orig=eNone, switch to e.Main in constructor
  CGBLGuard(CGBLGuard &g,const char *loc);   // inherit state from g1 - for SubGuards

  ~CGBLGuard();
  void Switch(EState newstate);
  template<class A> void Switch(EState newstate,A *a)
  {
    Select(m_Locks->m_Pool.Select(a));
    Switch(newstate);
  }
  template<class A> void Lock(A *a)   { Switch(eBoth,a); }
  template<class A> void Unlock(A *a) { Switch(eMain,a); }
  void Lock()   { Switch(eMain);};
  void Unlock() { Switch(eNone);};
  void Local()  { Switch(eLocal);};
  
private:
  TLMutex      *m_Locks;
  const char   *m_Loc;
  EState        m_orig;
  EState        m_current;
  int           m_select;
  
  void MLock();
  void MUnlock();
  void PLock();
  void PUnlock();
  void Select(int s);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif

/* ---------------------------------------------------------------------------
 *
 * $Log$
 * Revision 1.2  2002/07/22 23:10:04  kimelman
 * make sunWS happy
 *
 * Revision 1.1  2002/07/22 22:53:24  kimelman
 * exception handling fixed: 2level mutexing moved to Guard class + added
 * handling of confidential data.
 *
 */
