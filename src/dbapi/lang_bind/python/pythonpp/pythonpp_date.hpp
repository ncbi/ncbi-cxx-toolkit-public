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
* Author: Sergey Sikorskiy
*
* File Description: Tiny Python API wrappers
*
* Status: *Initial*
*
* ===========================================================================
*/

#ifndef PYTHONPP_DATE_H
#define PYTHONPP_DATE_H

#include "pythonpp_object.hpp"
#include <datetime.h>

BEGIN_NCBI_SCOPE

namespace pythonpp
{

class CTimeDelta : public CObject
{
public:
    CTimeDelta(int days, int seconds, int useconds)
    : CObject(PyDelta_FromDSU(days, seconds, useconds), eTakeOwnership)
    {
    }

protected:
    static bool HasSameType(PyObject* obj)
    {
        return PyDelta_Check(obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyDelta_CheckExact(obj);
    }

private:
};

class CTZInfo : public CObject
{
public:
protected:
    static bool HasSameType(PyObject* obj)
    {
        return PyTZInfo_Check(obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyTZInfo_CheckExact(obj);
    }

private:
};

class CTime : public CObject
{
public:
    CTime(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CTime(const CObject& obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
        Set(obj);
    }
    CTime(int hour, int minute, int second, int usecond)
    : CObject(PyTime_FromTime(hour, minute, second, usecond), eTakeOwnership)
    {
    }

public:
    int GetHour(void) const
    {
        return PyDateTime_TIME_GET_HOUR(Get());
    }
    int GetMinute(void) const
    {
        return PyDateTime_TIME_GET_MINUTE(Get());
    }
    int GetSecond(void) const
    {
        return PyDateTime_TIME_GET_SECOND(Get());
    }
    int GetMicroSecond(void) const
    {
        return PyDateTime_TIME_GET_MICROSECOND(Get());
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyTime_Check(obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyTime_CheckExact(obj);
    }

private:
};

class CDate : public CObject
{
// PyObject* PyDate_FromTimestamp( PyObject *args)
public:
    CDate(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CObject(obj, ownership)
    {
    }
    CDate(const CObject& obj)
    : CObject(obj)
    {
    }
    CDate(int year, int month, int day)
    : CObject(PyDate_FromDate(year, month, day), eTakeOwnership)
    {
    }

public:
    int GetYear(void) const
    {
        return PyDateTime_GET_YEAR(Get());
    }
    int GetMonth(void) const
    {
        return PyDateTime_GET_MONTH(Get());
    }
    int GetDay(void) const
    {
        return PyDateTime_GET_DAY(Get());
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyDate_Check(obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyDate_CheckExact(obj);
    }

private:
};

class CDateTime : public CDate
{
// PyObject* PyDateTime_FromTimestamp( PyObject *args)

public:
    CDateTime(PyObject* obj, EOwnership ownership = eAcquireOwnership)
    : CDate(obj, ownership)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
    }
    CDateTime(const CObject& obj)
    : CDate(obj)
    {
        if ( !HasExactSameType(obj) ) {
            throw CTypeError("Invalid conversion");
        }
        Set(obj);
    }
    CDateTime(int year, int month, int day, int hour = 0, int minute = 0, int second = 0, int usecond = 0)
    : CDate(PyDateTime_FromDateAndTime(year, month, day, hour, minute, second, usecond), eTakeOwnership)
    {
    }

public:
    int GetHour(void) const
    {
        return PyDateTime_DATE_GET_HOUR(Get());
    }
    int GetMinute(void) const
    {
        return PyDateTime_DATE_GET_MINUTE(Get());
    }
    int GetSecond(void) const
    {
        return PyDateTime_DATE_GET_SECOND(Get());
    }
    int GetMicroSecond(void) const
    {
        return PyDateTime_DATE_GET_MICROSECOND(Get());
    }

public:
    static bool HasSameType(PyObject* obj)
    {
        return PyDateTime_Check(obj);
    }
    static bool HasExactSameType(PyObject* obj)
    {
        return PyDateTime_CheckExact(obj);
    }

private:
};

}

END_NCBI_SCOPE // namespace pythonpp

#endif                                  // PYTHONPP_DATE_H

