#ifndef _BULK_INSERT_HPP_
#define _BULK_INSERT_HPP_

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
* File Description:  Array template class
*
*
* $Log$
* Revision 1.4  2004/04/08 15:56:58  kholodov
* Multiple bug fixes and optimizations
*
* Revision 1.3  2002/10/03 18:50:00  kholodov
* Added: additional TRACE diagnostics about object deletion
* Fixed: setting parameters in IStatement object is fully supported
* Added: IStatement::ExecuteLast() to execute the last statement with
* different parameters if any
*
* Revision 1.2  2002/09/18 18:49:26  kholodov
* Modified: class declaration and Action method to reflect
* direct inheritance of CActiveObject from IEventListener
*
* Revision 1.1  2002/09/16 19:34:41  kholodov
* Added: bulk insert support
*
*
*
*
*/

#include <dbapi/dbapi.hpp>
#include "active_obj.hpp"
#include <corelib/ncbistre.hpp>
#include "dbexception.hpp"

BEGIN_NCBI_SCOPE

class CBulkInsert : public CActiveObject, 
                    public IBulkInsert
{
public:
    CBulkInsert(const string& table,
                unsigned int cols,
                CConnection* conn);

    virtual ~CBulkInsert();

    virtual void Bind(unsigned int col,
                      CVariant *v);

    virtual void AddRow();
    virtual void StoreBatch();
    virtual void Cancel();
    virtual void Complete();
    virtual void Close();

    // Interface IEventListener implementation
    virtual void Action(const CDbapiEvent& e);

protected:

    CDB_BCPInCmd* GetBCPInCmd() { return m_cmd; }

    void FreeResources();

private:
    int m_nofCols;
    CDB_BCPInCmd* m_cmd;
    CConnection* m_conn;
  
};

//====================================================================
END_NCBI_SCOPE

#endif // _BULK_INSERT_HPP_
