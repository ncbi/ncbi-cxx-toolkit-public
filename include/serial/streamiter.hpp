#ifndef STREAMITER__HPP
#define STREAMITER__HPP

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
* Author: Andrei Gourianov
*
* File Description:
*   Input stream iterators
* Please note:
*   This API requires multi-threading
*/

#include <corelib/ncbistd.hpp>
#include <corelib/ncbithr.hpp>
#include <serial/objistr.hpp>


/** @addtogroup ObjStreamSupport
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
// Iterate over objects in input stream
// IMPORTANT: the following API requires multi-threading
#if defined(NCBI_THREADS)

template<typename TRoot, typename TObject>
class CIStreamIteratorThread_Base;

// Helper hook class
template<typename TRoot, typename TObject>
class CIStreamObjectHook : public CSerial_FilterObjectsHook<TObject>
{
public:
    CIStreamObjectHook(CIStreamIteratorThread_Base<TRoot,TObject>& thr)
        : m_Reader(thr)
    {
    }
    virtual void Process(const TObject& obj);
private:
    CIStreamIteratorThread_Base<TRoot,TObject>& m_Reader;
};

// Helper thread class
template<typename TRoot, typename TObject>
class CIStreamIteratorThread_Base : public CThread
{
public:
    CIStreamIteratorThread_Base(CObjectIStream& in, EOwnership deleteInStream)
        : m_In(in), m_Resume(0,1), m_Ready(0,1), m_Obj(0),
          m_Ownership(deleteInStream), m_Stop(false), m_Failed(false)
    {
    }
    // Resume thread, wait for the next object
    void Next(void)
    {
        m_Obj = 0;
        if (!m_Stop && !m_In.EndOfData()) {
            m_Resume.Post();
            m_Ready.Wait();
            if (m_Failed) {
                NCBI_THROW(CSerialException,eFail,
                             "invalid data object received");
            }
        }
    }
    // Request stop: thread is no longer needed
    void Stop(void)
    {
        m_Stop = true;
        m_Resume.Post();
        Join(0);
    }
    void Fail(void)
    {
        m_Failed = true;
        SetObject(0);
    }
    // Object is ready: suspend thread
    void SetObject(const TObject* obj)
    {
        m_Obj = obj;
        m_Ready.Post();
        m_Resume.Wait();
        if (m_Stop) {
            Exit(0);
        }
    }
    const TObject* GetObject(void) const
    {
        return m_Obj;
    }
protected:
    ~CIStreamIteratorThread_Base(void)
    {
        if (m_Ownership == eTakeOwnership) {
            delete &m_In;
        }
    }
    virtual void* Main(void)
    {
        return 0;
    }
protected:
    CObjectIStream&      m_In;
    CSemaphore           m_Resume;
    CSemaphore           m_Ready;
    const TObject*       m_Obj;
    EOwnership           m_Ownership;
    bool                 m_Stop;
    bool                 m_Failed;
};

// Reading thread for serial objects
template<typename TRoot, typename TObject>
class CIStreamObjectIteratorThread
    : public CIStreamIteratorThread_Base< TRoot,TObject >
{
public:
    CIStreamObjectIteratorThread(CObjectIStream& in, EOwnership deleteInStream)
        : CIStreamIteratorThread_Base< TRoot,TObject >(in, deleteInStream)
    {
    }
protected:
    ~CIStreamObjectIteratorThread(void)
    {
    }
    virtual void* Main(void)
    {
        this->m_Resume.Wait();
        // Enumerate objects of requested type
        try {
            Serial_FilterObjects< TRoot >( this->m_In,
                new CIStreamObjectHook< TRoot, TObject >(*this));
            this->SetObject(0);
        } catch (CException& e) {
            NCBI_REPORT_EXCEPTION("In CIStreamObjectIteratorThread",e);
            this->Fail();
        }
        return 0;
    }
};

// Reading thread for std objects
template<typename TRoot, typename TObject>
class CIStreamStdIteratorThread
    : public CIStreamIteratorThread_Base< TRoot,TObject >
{
public:
    CIStreamStdIteratorThread(CObjectIStream& in, EOwnership deleteInStream)
        : CIStreamIteratorThread_Base< TRoot,TObject >(in, deleteInStream)
    {
    }
protected:
    ~CIStreamStdIteratorThread(void)
    {
    }
    virtual void* Main(void)
    {
        this->m_Resume.Wait();
        // Enumerate objects of requested type
        try {
            Serial_FilterStdObjects< TRoot >( this->m_In,
                new CIStreamObjectHook< TRoot, TObject >(*this));
            this->SetObject(0);
        } catch (CException& e) {
            NCBI_REPORT_EXCEPTION("In CIStreamStdIteratorThread",e);
            this->Fail();
        }
        return 0;
    }
};

template<typename TRoot, typename TObject>
inline
void CIStreamObjectHook<TRoot,TObject>::Process(const TObject& obj)
{
    m_Reader.SetObject(&obj);
}

// Stream iterator base class
template<typename TRoot, typename TObject>
class CIStreamIterator_Base
{
public:
    void operator++()
    {
        m_Reader->Next();
    }
    void operator++(int)
    {
        m_Reader->Next();
    }
    const TObject& operator* (void) const
    {
        return *(m_Reader->GetObject());
    }
    const TObject* operator-> (void) const
    {
        return m_Reader->GetObject();
    }
    bool IsValid(void) const
    {
        return m_Reader->GetObject() != 0;
    }
protected:
    CIStreamIterator_Base()
        : m_Reader(nullptr)
    {
    }
    ~CIStreamIterator_Base(void)
    {
        if (m_Reader) {
            m_Reader->Stop();
        }
    }
private:
    // prohibit copy
    CIStreamIterator_Base(const CIStreamIterator_Base<TRoot,TObject>& v);
    // prohibit assignment
    CIStreamIterator_Base<TRoot,TObject>& operator=(
        const CIStreamIterator_Base<TRoot,TObject>& v);

protected:
    CIStreamIteratorThread_Base< TRoot, TObject > *m_Reader;
};

/// Stream iterator for serial objects
///
/// Usage:
///    CObjectIStream* is = CObjectIStream::Open(...);
///    CIStreamObjectIterator<CRootClass,CObjectClass> i(*is);
///    for ( ; i.IsValid(); ++i) {
///        const CObjectClass& obj = *i;
///        ...
///    }
/// IMPORTANT:
///     This API requires multi-threading!

template<typename TRoot, typename TObject>
class CIStreamObjectIterator
    : public CIStreamIterator_Base< TRoot, TObject>
{
public:
    CIStreamObjectIterator(CObjectIStream& in, EOwnership deleteInStream = eNoOwnership)
    {
        // Create reading thread, wait until it finds the next object
        this->m_Reader =
            new CIStreamObjectIteratorThread< TRoot, TObject >(in, deleteInStream);
        this->m_Reader->Run();
        this->m_Reader->Next();
    }
    ~CIStreamObjectIterator(void)
    {
    }
};

/// Stream iterator for standard type objects
///
/// Usage:
///    CObjectIStream* is = CObjectIStream::Open(...);
///    CIStreamStdIterator<CRootClass,string> i(*is);
///    for ( ; i.IsValid(); ++i) {
///        const string& obj = *i;
///        ...
///    }
/// IMPORTANT:
///     This API requires multi-threading!

template<typename TRoot, typename TObject>
class CIStreamStdIterator
    : public CIStreamIterator_Base< TRoot, TObject>
{
public:
    CIStreamStdIterator(CObjectIStream& in, EOwnership deleteInStream = eNoOwnership)
    {
        // Create reading thread, wait until it finds the next object
        this->m_Reader =
            new CIStreamStdIteratorThread< TRoot, TObject >(in,deleteInStream);
        this->m_Reader->Run();
        this->m_Reader->Next();
    }
    ~CIStreamStdIterator(void)
    {
    }
};

#endif // _MT


/* @} */

END_NCBI_SCOPE

#endif  /* STREAMITER__HPP */
