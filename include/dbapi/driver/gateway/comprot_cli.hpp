#ifndef RDBLIB__COMPROT_CLI__HPP
#define RDBLIB__COMPROT_CLI__HPP

/* $Id$
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
 * Author:  Victor Sapojnikov
 *
 * File Description:
 *   Conveniency shortcut functions for CompactProtocol client,
 *   returning bool/int/void and accepting 0 or 1 parameters.
 *
 */

// prevent WINSOCK.H from being included - we use WINSOCK2.H
#define _WINSOCKAPI_

#include <cli/sssconnection.hpp>
#include <iostream>

using namespace std;

extern CSSSConnection* conn;
void comprot_errmsg();


bool comprot_bool( const char *procName, int object );

template<class T> bool comprot_bool1( const char *procName, int object, T* param )
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call(procName);
  pGate->set_output_arg( "object", &object );

  // cerr << "bool1 " << procName << " object=" << object << "\n";

  pGate->set_output_arg("param", param);
  pGate->send_data();

  int nOk;
  if (pGate->get_input_arg("result", &nOk) != IGate::eGood) {
    comprot_errmsg();
    return false;
  }

  // cerr << "  result=" << nOk << "\n";

  return nOk;
}


int comprot_int( const char *procName, int object );

template<class T> int comprot_int1( const char *procName, int object, T* param )
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call(procName);
  pGate->set_output_arg( "object", &object );

  // cerr << "int1 " << procName << " object=" << object << "\n";

  pGate->set_output_arg("param", param);
  pGate->send_data();

  int res;
  if (pGate->get_input_arg("result", &res) != IGate::eGood) {
    comprot_errmsg();
    return 0;
  }

  // cerr << "  result=" << res << "\n";

  return res;
}

template<class T1, class T2> int comprot_int2(
  const char *procName, int object,
  const char *name1, T1* param1,
  const char *name2, T2* param2)
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call(procName);

  // cerr << "int2 " << procName << " object=" << object << "\n";

  pGate->set_output_arg( "object", &object );
  pGate->set_output_arg(name1, param1);
  pGate->set_output_arg(name2, param2);
  pGate->send_data();

  int res;
  if (pGate->get_input_arg("result", &res) != IGate::eGood) {
    comprot_errmsg();
    return 0;
  }

  // cerr << "  result=" << res << "\n";

  return res;
}


void comprot_void( const char *procName, int object );

template<class T> void comprot_void1( const char *procName, int object, T* param )
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call(procName);
  pGate->set_output_arg( "object", &object );

  // cerr << "void1 " << procName << " object=" << object << "\n";

  pGate->set_output_arg( "param", param );
  pGate->send_data();
}

char* comprot_chars( const char *procName, int object, char* buf, int len );

template<class T> char* comprot_chars1( const char *procName, int object, T* param, char* buf, int len )
{
  IGate* pGate = conn->getProtocol();
  pGate->set_RPC_call(procName);
  pGate->set_output_arg( "object", &object );

  // cerr << "chars1 " << procName << " object=" << object << "\n";

  pGate->set_output_arg("param", param);
  pGate->send_data();

  if (pGate->get_input_arg("result", buf, len) != IGate::eGood) {
    comprot_errmsg();
    return 0;
  }

  // cerr << "  result=" << buf << "\n";

  return buf;
}

#endif  /* RDBLIB__COMPROT_CLI__HPP */

