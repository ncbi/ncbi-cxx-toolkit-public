#ifndef DBAPI_DRIVER_IMPL___DBAPI_IMPL_RESULT__HPP
#define DBAPI_DRIVER_IMPL___DBAPI_IMPL_RESULT__HPP

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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *
 */

#include <dbapi/driver/impl/dbapi_driver_utils.hpp>
#include <dbapi/driver/util/parameters.hpp>


BEGIN_NCBI_SCOPE

namespace impl {

/////////////////////////////////////////////////////////////////////////////
///
///  CResult::
///

class NCBI_DBAPIDRIVER_EXPORT CResult
{
    friend class ncbi::CDB_Result; // Because of Release().

public:
    CResult(void);
    virtual ~CResult(void);

public:
    /// Get type of the result
    virtual EDB_ResType ResultType(void) const = 0;

    virtual const CDBParams& GetDefineParams(void) const;

    /// Fetch next row
    virtual bool Fetch(void) = 0;

    /// Return current item number we can retrieve (0,1,...)
    /// Return "-1" if no more items left (or available) to read.
    virtual int CurrentItemNo(void) const = 0;

    /// Return number of columns in the recordset.
    virtual int GetColumnNum(void) const = 0;

    /// Get a result item (you can use either GetItem or ReadItem).
    /// If "item_buf" is not NULL, then use "*item_buf" (its type should be
    /// compatible with the type of retrieved item!) to retrieve the item to;
    /// otherwise allocate new "CDB_Object".
    virtual CDB_Object* GetItem(CDB_Object* item_buf = 0, 
					I_Result::EGetItem policy = I_Result::eAppendLOB) = 0;

    /// Read a result item body (for text/image mostly).
    /// Return number of successfully read bytes.
    /// Set "*is_null" to TRUE if the item is <NULL>.
    /// Throw an exception on any error.
    virtual size_t ReadItem(void* buffer, size_t buffer_size,
                            bool* is_null = 0) = 0;

    /// Get a descriptor for text/image column (for SendData).
    /// Return NULL if this result doesn't (or can't) have img/text descriptor.
    /// NOTE: you need to call ReadItem (maybe even with buffer_size == 0)
    ///       before calling this method!
    virtual I_ITDescriptor* GetImageOrTextDescriptor(void) = 0;

    /// Skip result item
    virtual bool SkipItem(void) = 0;

    void AttachTo(CDB_Result* interface)
    {
        m_Interface = interface;
    }

    const CDB_Params& GetDefineParamsImpl(void) const
    {
        return m_DefineParams;
    }
    CDB_Params& GetDefineParamsImpl(void)
    {
        return m_DefineParams;
    }

protected:
    CDB_Params     m_DefineParams;
    CCachedRowInfo m_CachedRowInfo;

private:
    void Release(void)
    {
//         delete this;
        DetachInterface();
    }

    void DetachInterface(void)
    {
        m_Interface.DetachInterface();
    }

    CInterfaceHook<CDB_Result> m_Interface;

};


} // namespace impl

END_NCBI_SCOPE

#endif  /* DBAPI_DRIVER_IMPL___DBAPI_IMPL_RESULT__HPP */
