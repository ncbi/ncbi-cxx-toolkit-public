#ifndef SERIALIZABLE__HPP
#define SERIALIZABLE__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/04/12 17:01:04  kholodov
* General serializable interface for different output formats
*
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <serial/exception.hpp>

BEGIN_NCBI_SCOPE

class ISerializable {

public:

  enum EOutputType { eAsFasta, eAsAsnText, eAsAsnBinary, eAsXML };

  virtual void WriteAsFasta(ostream& out) {
    throw CSerialNotImplemented("ISerializable::WriteAsFasta() not implemented");
  }
  virtual void WriteAsAsnText(ostream& out) {
    throw CSerialNotImplemented("ISerializable::WriteAsAsnText() not implemented");
  }
  virtual void WriteAsAsnBinary(ostream& out) {
    throw CSerialNotImplemented("ISerializable::WriteAsAsnBinary() not implemented");
  }
  virtual void WriteAsXML(ostream& out) {
    throw CSerialNotImplemented("ISerializable::WriteAsXML() not implemented");
  }
  
  virtual EOutputType GetOutputType() = 0;

};

ostream& operator << (ostream& out, ISerializable& src); 

END_NCBI_SCOPE

#endif  /* SERIALIZABLE__HPP */
