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
* Author:  Andrei Gourianov
*
* File Description:
*   Simple program demonstrating how to walk a serial object.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/objistr.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);


class CDemoApp : public CNcbiApplication
{
    virtual int  Run(void);

    void Dispatch(size_t offset, const CConstObjectInfo& info);
    void WalkPrimitive(size_t offset, const CConstObjectInfo& info);
    void WalkClass(size_t offset, const CConstObjectInfo& info);
    void WalkChoice(size_t offset, const CConstObjectInfo& info);
    void WalkContainer(size_t offset, const CConstObjectInfo& info);
    void WalkPointer(size_t offset, const CConstObjectInfo& info);
};


int CDemoApp::Run(void)
{
    // Get a Seq-entry object.
    CSeq_entry entry;
    CNcbiIfstream in("sample.asn");
    in >> MSerial_AsnText >> entry;

    // Get the type info and walk the object hierarchy.
    CConstObjectInfo info(&entry, entry.GetThisTypeInfo());
    Dispatch(0,info);

    return 0;
}


void CDemoApp::Dispatch(size_t offset, const CConstObjectInfo& info)
{
    offset += 2;
    switch (info.GetTypeFamily()) {
    default:
        cout << "ERROR: unknown type family" << endl;
        break;
    case eTypeFamilyPrimitive:
        WalkPrimitive(offset, info);
        break;
    case eTypeFamilyClass:
        WalkClass(offset, info);
        break;
    case eTypeFamilyChoice:
        WalkChoice(offset, info);
        break;
    case eTypeFamilyContainer:
        WalkContainer(offset, info);
        break;
    case eTypeFamilyPointer:
        WalkPointer(offset, info);
        break;
    }
}

void CDemoApp::WalkPrimitive(size_t offset, const CConstObjectInfo& info)
{
    string off(offset,' ');
    cout << off << "primitive: ";
    EPrimitiveValueType type = info.GetPrimitiveValueType();

    switch (type) {
    case ePrimitiveValueSpecial: cout << "special"; break;
    case ePrimitiveValueBool:
        cout << "boolean: " << info.GetPrimitiveValueBool();
        break;
    case ePrimitiveValueChar:
        cout << "char: " << info.GetPrimitiveValueChar();
        break;
    case ePrimitiveValueInteger:
        cout << (info.IsPrimitiveValueSigned() ? "signed" : "unsigned")
             << " integer: "
             << info.GetPrimitiveValueInt8();
        break;
    case ePrimitiveValueReal:
        cout << "float: " << info.GetPrimitiveValueDouble();
        break;
    case ePrimitiveValueString:
        cout << "string: " << info.GetPrimitiveValueString();
        break;
    case ePrimitiveValueEnum:
        cout << "enum: ";
        {
            const CEnumeratedTypeValues& values = info.GetEnumeratedTypeValues();
            if (values.IsInteger()) {
                cout << " integer: ";
            }
            cout << values.FindName( info.GetPrimitiveValueInt(), true);
        }
        break;
    case ePrimitiveValueOctetString:
        cout << "octetstring: ??";
        break;
    case ePrimitiveValueBitString:
        cout << "bitstring: ??";
        break;
    case ePrimitiveValueAny:
        cout << "any: ??";
        break;
    case ePrimitiveValueOther:
        cout << "other: ??";
        break;
    }
    cout << endl;
}

void CDemoApp::WalkClass(size_t offset, const CConstObjectInfo& info)
{
    string off(offset,' ');
    cout << off << "class: " << info.GetName() << " {" << endl;
    CConstObjectInfoMI mem = info.BeginMembers();
    while (mem.Valid()) {
        cout << off << " [" << mem.GetMemberIndex() << "]: "
                    << mem.GetItemInfo()->GetId().GetName();
        if (mem.IsSet()) {
            cout << endl;
            Dispatch(offset, mem.GetMember());
        } else {
            cout << ": not set" << endl;
        }
        ++mem;
    }
    cout << off << "} end of class: " << info.GetName() << endl;
}

void CDemoApp::WalkChoice(size_t offset, const CConstObjectInfo& info)
{
    string off(offset,' ');
    cout << off << "choice: " << info.GetName() << " {" << endl;
    CConstObjectInfoCV var = info.GetCurrentChoiceVariant();
    cout << off << " [" << info.GetCurrentChoiceVariantIndex() << "]: "
                << var.GetVariantInfo()->GetId().GetName()
                << endl;
    Dispatch(offset,var.GetVariant());
    cout << off << "} end of choice: " << info.GetName() << endl;
}

void CDemoApp::WalkContainer(size_t offset, const CConstObjectInfo& info)
{
    string off(offset,' ');
    cout << off << "container: " << info.GetName() << " {" << endl;
    CConstObjectInfoEI e = info.BeginElements();
    while (e.Valid()) {
        cout << off << " [" << e.GetIndex() << "]: " << endl;
        Dispatch(offset, e.GetElement());
        ++e;
    }
    cout << off << "} end of container: " << info.GetName() << endl;
}

void CDemoApp::WalkPointer(size_t offset, const CConstObjectInfo& info)
{
    string off(offset,' ');
    cout << off << "pointer: " << info.GetName() << endl;
    Dispatch(offset,info.GetPointedObject());
}


int main(int argc, const char* argv[])
{
    return CDemoApp().AppMain(argc, argv);
}

