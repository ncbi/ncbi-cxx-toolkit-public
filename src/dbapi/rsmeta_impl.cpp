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
* File Name:  $Id$
*
* Author:  Michael Kholodov
*   
* File Description:  Resultset metadata implementation
*
*
* $Log$
* Revision 1.7  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.6  2003/11/04 21:39:03  vakatov
* Minor style fixes;  also explicitly put into the NCBI_SCOPE.
*
* Revision 1.5  2002/11/25 15:32:06  kholodov
* Modified: using STL vector istead of CDynamicArray.
*
* Revision 1.4  2002/10/03 18:50:00  kholodov
* Added: additional TRACE diagnostics about object deletion
* Fixed: setting parameters in IStatement object is fully supported
* Added: IStatement::ExecuteLast() to execute the last statement with
* different parameters if any
*
* Revision 1.3  2002/09/18 18:49:27  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.2  2002/09/09 20:48:57  kholodov
* Added: Additional trace output about object life cycle
* Added: CStatement::Failed() method to check command status
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*/

#include <ncbi_pch.hpp>
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/public.hpp>

#include "rsmeta_impl.hpp"
#include "rs_impl.hpp"


BEGIN_NCBI_SCOPE


CResultSetMetaData::CResultSetMetaData(CDB_Result *rs)
  : m_totalColumns(rs->NofItems())
{
    SetIdent("CResultSetMetaData");

  // Fill out column metadata
  for(unsigned int i = 0; i < m_totalColumns; ++i ) {
  
    SColMetaData md(rs->ItemName(i) == 0 ? "" : rs->ItemName(i),
		    rs->ItemDataType(i),
		    rs->ItemMaxSize(i));

    m_colInfo.push_back(md);
    
  }
}

CResultSetMetaData::~CResultSetMetaData()
{
  Notify(CDbapiDeletedEvent(this));
  _TRACE(GetIdent() << " " << (void*)this << " deleted."); 
}

void CResultSetMetaData::Action(const CDbapiEvent& e) 
{
    _TRACE(GetIdent() << " " << (void*)this << ": '" << e.GetName() 
           << "' from " << e.GetSource()->GetIdent());

  if(dynamic_cast<const CDbapiDeletedEvent*>(&e) != 0 ) {
    RemoveListener(e.GetSource());
    if(dynamic_cast<CResultSet*>(e.GetSource()) != 0 ) {
        _TRACE("Deleting " << GetIdent() << " " << (void*)this); 
      delete this;
    }
  }
}


END_NCBI_SCOPE
