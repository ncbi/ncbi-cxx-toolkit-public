#ifndef CONNECT_SERVICES__NETSCHEDULE_STORAGE_HPP
#define CONNECT_SERVICES__NETSCHEDULE_STORAGE_HPP

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
 * Authors:  Maxim Didneko, Anatoliy Kuznetsov
 *
 * File Description:  NetSchedule shared storage interface
 *
 */

#include <connect/connect_export.h>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbiexpt.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup NetScheduleClient
 *
 * @{
 */


/// NetSchedule Storage interface
///
/// This interface is used by Worker Node and Worker 
/// Node client to retrive and store data to/from external storage
//
class NCBI_XCONNECT_EXPORT INetScheduleStorage
{
public:
    virtual CNcbiIstream& GetIStream(const string& data_id, 
                                     size_t* blob_size = 0) = 0;
    virtual CNcbiOstream& CreateOStream(string& data_id) = 0;
    virtual void Reset() = 0;
};

class NCBI_XCONNECT_EXPORT CNetScheduleStorageException : public CException
{
public:
    enum EErrCode {
        eReader,
        eWriter
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eReader: return "eReaderError";
        case eWriter: return "eWriterError";
        default:      return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetScheduleStorageException, CException);
};

class NCBI_XCONNECT_EXPORT CNullStorage : public INetScheduleStorage
{
public:
    CNcbiIstream& GetIStream(const string&,
                             size_t* blob_size = 0)
    {
        if (blob_size) *blob_size = 0;
        NCBI_THROW(CNetScheduleStorageException,
                   eReader, "Empty Storage reader.");
   }
    CNcbiOstream& CreateOStream(string&)
    {
        NCBI_THROW(CNetScheduleStorageException,
                   eWriter, "Empty Storage writer.");
    }
    void Reset() {};
};


/* @} */

END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/03/23 13:10:32  kuznets
 * documented and doxygenized
 *
 * Revision 1.2  2005/03/22 21:42:50  didenko
 * Got rid of warnning on Sun WorkShop
 *
 * Revision 1.1  2005/03/22 20:17:55  didenko
 * Initial version
 *
 * ===========================================================================
 */


#endif // CONNECT_SERVICES__NETSCHEDULE_STORAGE_HPP
