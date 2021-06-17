#ifndef TESTSERIAL_HPP
#define TESTSERIAL_HPP

#include <corelib/ncbistd.hpp>
#include <corelib/ncbiapp.hpp>

#include "serialobject.hpp"
#include <serial/serial.hpp>
#include <serial/serialasn.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objectio.hpp>
#include <serial/iterator.hpp>
#include <serial/objhook.hpp>
#include "cppwebenv.hpp"
#include <serial/serialimpl.hpp>
#include <serial/streamiter.hpp>

#ifdef HAVE_NCBI_C
# include <asn.h>
# include "twebenv.h"
#else
# include <serial/test/Web_Env.hpp>
#endif

#include <corelib/ncbifile.hpp>
#include <corelib/test_boost.hpp>

/////////////////////////////////////////////////////////////////////////////
// TestHooks

void InitializeTestObject(
#ifdef HAVE_NCBI_C
    WebEnv*& env,
#else
    CRef<CWeb_Env>& env,
#endif
    CTestSerialObject& obj, CTestSerialObject2& write1);

class CWriteSerialObjectHook : public CWriteObjectHook
{
public:
    CWriteSerialObjectHook(CTestSerialObject* hooked)
        : m_HookedObject(hooked) {}
    virtual void
    WriteObject(CObjectOStream& out, const CConstObjectInfo& object);
private:
    CTestSerialObject* m_HookedObject;
};

class CReadSerialObjectHook : public CReadObjectHook
{
public:
    CReadSerialObjectHook(CTestSerialObject* hooked)
        : m_HookedObject(hooked) {}
    virtual void
    ReadObject(CObjectIStream& in, const CObjectInfo& object);
private:
    CTestSerialObject* m_HookedObject;
};

class CWriteSerialObject_NameHook : public CWriteClassMemberHook
{
public:
    virtual void
    WriteClassMember(CObjectOStream& out, const CConstObjectInfoMI& member);
};

class CReadSerialObject_NameHook : public CReadClassMemberHook
{
public:
    virtual void
    ReadClassMember(CObjectIStream& in, const CObjectInfoMI& member);
};

class CTestSerialObjectHook : public CSerial_FilterObjectsHook<CTestSerialObject>
{
public:
    virtual void Process(const CTestSerialObject& obj);
};

class CStringObjectHook : public CSerial_FilterObjectsHook<string>
{
public:
    virtual void Process(const string& obj);
};

/////////////////////////////////////////////////////////////////////////////
// TestPrintAsn

void PrintAsn(CNcbiOstream& out, const CConstObjectInfo& object);

#endif
