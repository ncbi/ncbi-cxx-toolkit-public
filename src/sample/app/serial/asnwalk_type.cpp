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

    void Dispatch(int depth, size_t offset, const CObjectTypeInfo& info);
    void WalkPrimitive(int depth, size_t offset, const CObjectTypeInfo& info);
    void WalkClass(int depth, size_t offset, const CObjectTypeInfo& info);
    void WalkChoice(int depth, size_t offset, const CObjectTypeInfo& info);
    void WalkContainer(int depth, size_t offset, const CObjectTypeInfo& info);
    void WalkPointer(int depth, size_t offset, const CObjectTypeInfo& info);

    int m_MaxDepth;
};


int CDemoApp::Run(void)
{
    // Set this arbitrarily for demo purposes.
    m_MaxDepth = 5;

    // Get the type info and walk the object hierarchy.
    CObjectTypeInfo info(CSeq_entry::GetTypeInfo());
    Dispatch(0, 0, info);

    return 0;
}


void CDemoApp::Dispatch(int depth, size_t offset, const CObjectTypeInfo& info)
{
    if (depth > m_MaxDepth) {
        cout << "..." << endl;
        return;
    }
    ++depth;
    offset += 2;
    switch (info.GetTypeFamily()) {
    default:
        cout << "ERROR: unknown type family" << endl;
        break;
    case eTypeFamilyPrimitive:
        WalkPrimitive(depth, offset, info);
        break;
    case eTypeFamilyClass:
        WalkClass(depth, offset, info);
        break;
    case eTypeFamilyChoice:
        WalkChoice(depth, offset, info);
        break;
    case eTypeFamilyContainer:
        WalkContainer(depth, offset, info);
        break;
    case eTypeFamilyPointer:
        WalkPointer(depth, offset, info);
        break;
    }
    --depth;
}
void CDemoApp::WalkPrimitive(int depth, size_t offset, const CObjectTypeInfo& info)
{
    string off(offset,' ');
    cout << off << "primitive: ";
    EPrimitiveValueType type = info.GetPrimitiveValueType();

    switch (type) {
    case ePrimitiveValueSpecial: cout << "special"; break;
    case ePrimitiveValueBool:
        cout << "boolean";
        break;
    case ePrimitiveValueChar:
        cout << "char";
        break;
    case ePrimitiveValueInteger:
        cout << (info.IsPrimitiveValueSigned() ? "signed" : "unsigned")
             << " integer";
        break;
    case ePrimitiveValueReal:
        cout << "float";
        break;
    case ePrimitiveValueString:
        cout << "string";
        break;
    case ePrimitiveValueEnum:
        cout << "enum: ";
        {
            const CEnumeratedTypeValues& values = info.GetEnumeratedTypeValues();
            if (values.IsInteger()) {
                cout << " integer: ";
            }
            CEnumeratedTypeValues::TValues all = values.GetValues();
            cout << "{" << endl;
            ITERATE( CEnumeratedTypeValues::TValues, v, all) {
                cout << off << " " << v->first << " (" << v->second << ")" << endl;
            }
            cout << off << "}";
        }
        break;
    case ePrimitiveValueOctetString:
        cout << "octetstring";
        break;
    case ePrimitiveValueBitString:
        cout << "bitstring";
        break;
    case ePrimitiveValueAny:
        cout << "any";
        break;
    case ePrimitiveValueOther:
        cout << "other";
        break;
    }
    cout << endl;
}

void CDemoApp::WalkClass(int depth, size_t offset, const CObjectTypeInfo& info)
{
    string off(offset,' ');
    cout << off;

    const CClassTypeInfo* info_class = info.GetClassTypeInfo();
    if (info_class->RandomOrder()) {
        cout << "random ";
    } else if (info_class->Implicit()) {
        cout << "implicit ";
    } else {
        cout << "sequential ";
    }
    cout << "class: " << info.GetName() << " {" << endl;
    CObjectTypeInfoMI mem = info.BeginMembers();
    for ( ; mem.Valid(); ++mem) {
        cout << off << " [" << mem.GetMemberIndex() << "]: "
                    << mem.GetMemberInfo()->GetId().GetName()
                    << " {" << endl;
        Dispatch(depth, offset, mem.GetMemberType());
        cout << off << " }";
        if (mem.GetMemberInfo()->Optional()) {
            cout << " optional";
        }
        if (mem.GetMemberInfo()->GetDefault()) {
            cout << " with default: {" << endl;
            CConstObjectInfo def(mem.GetMemberInfo()->GetDefault(),
                                 mem.GetMemberInfo()->GetTypeInfo());
            Dispatch(depth, offset, def);
            cout << off << " }" << endl;
        }
        cout << endl;
    }
    cout << off << "} end of " << info.GetName() << endl;
}

void CDemoApp::WalkChoice(int depth, size_t offset, const CObjectTypeInfo& info)
{
    string off(offset,' ');
    cout << off << "choice: " << info.GetName() << " {" << endl;
    CObjectTypeInfoVI var = info.BeginVariants();
    for (; var.Valid(); ++var) {
        cout << off << " [" << var.GetVariantIndex() << "]: "
                    << var.GetVariantInfo()->GetId().GetName()
                    << " {" << endl;
        Dispatch(depth, offset, var.GetVariantType());
        cout << off << " }" << endl;
    }
    cout << off << "} end of " << info.GetName() << endl;
}
void CDemoApp::WalkContainer(int depth, size_t offset, const CObjectTypeInfo& info)
{
    string off(offset,' ');
    cout << off << "container of: " << info.GetName() << " {" << endl;
    Dispatch(depth, offset, info.GetElementType());
    cout << off << "} end of " << info.GetName() << endl;
}
void CDemoApp::WalkPointer(int depth, size_t offset, const CObjectTypeInfo& info)
{
    string off(offset,' ');
    Dispatch(depth, offset,info.GetPointedType());
}


int main(int argc, const char* argv[])
{
    return CDemoApp().AppMain(argc, argv);
}

