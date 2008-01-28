#ifndef DBAPI_DRIVER_IMPL___DBAPI_DRIVER_UTILS__HPP
#define DBAPI_DRIVER_IMPL___DBAPI_DRIVER_UTILS__HPP

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
 * File Description:  Small utility classes common to all drivers.
 *
 */

#include <dbapi/driver/public.hpp>

#include <corelib/ncbi_safe_static.hpp>


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
class CDBHandlerStack;

/////////////////////////////////////////////////////////////////////////////
class NCBI_DBAPIDRIVER_EXPORT CDBExceptionStorage
{
private:
    friend class CSafeStaticPtr<CDBExceptionStorage>;

    CDBExceptionStorage(void);
    ~CDBExceptionStorage(void) throw();

public:
    void Accept(const CDB_Exception& e);
    void Handle(CDBHandlerStack& handler);
    void Handle(CDBHandlerStack& handler, const string& msg);

private:
    CFastMutex                      m_Mutex;
    CDB_UserHandler::TExceptions    m_Exceptions;
};

template <class I>
class CInterfaceHook
{
public:
    CInterfaceHook(I* interface = NULL) :
        m_Interface(NULL)
    {
        AttachTo(interface);
    }
    ~CInterfaceHook(void)
    {
        DetachInterface();
    }

    CInterfaceHook& operator=(I* interface)
    {
        AttachTo(interface);
        return *this;
    }

public:
    void AttachTo(I* interface)
    {
        DetachInterface();
        m_Interface = interface;
    }
    void DetachInterface(void)
    {
        if (m_Interface) {
            m_Interface->ReleaseImpl();
            m_Interface = NULL;
        }
    }

private:
    CInterfaceHook(const CInterfaceHook&);
    CInterfaceHook& operator=(const CInterfaceHook&);

    I* m_Interface;
};


/////////////////////////////////////////////////////////////////////////////
namespace impl {

class CDB_Params;

class NCBI_DBAPIDRIVER_EXPORT CDBBindedParams : public CDBParams
{
public:
    CDBBindedParams(impl::CDB_Params& bindings);

public:
    virtual unsigned int GetNum(void) const;
    virtual string GetName(
            const CDBParamVariant& param, 
            CDBParamVariant::ENameFormat format = 
                CDBParamVariant::eSQLServerName) const;
    virtual unsigned int GetIndex(const CDBParamVariant& param) const;
    virtual size_t GetMaxSize(const CDBParamVariant& param) const;
    virtual EDB_Type GetDataType(const CDBParamVariant& param) const;
    virtual EDirection GetDirection(const CDBParamVariant& param) const;

    /// This method stores pointer to data.
    virtual CDBParams& Bind(
        const CDBParamVariant& param, 
        CDB_Object* value, 
        bool out_param = false
        );
    /// This method stores copy of data.
    virtual CDBParams& Set(
        const CDBParamVariant& param, 
        CDB_Object* value, 
        bool out_param = false
        );

private:
    impl::CDB_Params* m_Bindings;
};

class NCBI_DBAPIDRIVER_EXPORT CCachedRowInfo : public CDBBindedParams
{
public:
    CCachedRowInfo(impl::CDB_Params& bindings);
    virtual ~CCachedRowInfo(void);

public:
    virtual unsigned int GetNum(void) const;

    virtual string GetName(
            const CDBParamVariant& param, 
            CDBParamVariant::ENameFormat format = 
                CDBParamVariant::eSQLServerName) const;
    virtual unsigned int GetIndex(const CDBParamVariant& param) const;
    
    virtual size_t GetMaxSize(const CDBParamVariant& param) const;
    virtual EDB_Type GetDataType(const CDBParamVariant& param) const;
    virtual EDirection GetDirection(const CDBParamVariant& param) const;

    inline
    void Add(const string& name,
            size_t max_size,
            EDB_Type data_type = eDB_UnsupportedType,
            EDirection direction = eOut
            );

private:
    // Inline version of virtual function GetNum() ...
    unsigned int GetNumInternal(void) const
    {
        return m_Info.size();
    }

private:
    struct SInfo
    {
        SInfo(void);
        SInfo(const string& name,
                size_t max_size,
                EDB_Type data_type = eDB_UnsupportedType,
                EDirection direction = eOut
                );

        string m_Name;
        size_t m_MaxSize;
        EDB_Type m_DataType;
        EDirection m_Direction;
    };

    vector<SInfo> m_Info;
};


class NCBI_DBAPIDRIVER_EXPORT CRowInfo_SP_SQL_Server : public CCachedRowInfo
{
public:
    CRowInfo_SP_SQL_Server(
            const string& name,
            impl::CConnection& conn, 
            impl::CDB_Params& bindings
            );
    virtual ~CRowInfo_SP_SQL_Server(void);
};

inline
void
CCachedRowInfo::Add(const string& name,
        size_t max_size,
        EDB_Type data_type,
        EDirection direction
        )
{
    m_Info.push_back(SInfo(name, max_size, data_type, direction));
}

} // namespace impl


namespace impl 
{

NCBI_DBAPIDRIVER_EXPORT
string ConvertN2A(Uint4 host);

}


END_NCBI_SCOPE


#endif // DBAPI_DRIVER_IMPL___DBAPI_DRIVER_UTILS__HPP


