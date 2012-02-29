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

    void SetDispatch(int depth, size_t offset, CObjectInfo& info);
    void SetPrimitive(int depth, size_t offset, CObjectInfo& info);
    void SetClass(int depth, size_t offset, CObjectInfo& info);
    void SetChoice(int depth, size_t offset, CObjectInfo& info);
    void SetContainer(int depth, size_t offset, CObjectInfo& info);
    void SetPointer(int depth, size_t offset, CObjectInfo& info);

    int m_MaxDepth;
};


int CDemoApp::Run(void)
{
    // Set this arbitrarily for demo purposes.
    m_MaxDepth = 5;

    // Get a Seq-entry object.
    CSeq_entry obj;

    // Get the object info and walk the object hierarchy.
    CObjectInfo info(ObjectInfo(obj));
    SetDispatch(0,0,info);
    cout << MSerial_AsnText << obj;

    return 0;
}


void CDemoApp::SetDispatch(int depth, size_t offset, CObjectInfo& info)
{
    ++depth;
    offset += 2;
    switch (info.GetTypeFamily()) {
    default:
        cout << "ERROR: unknown type family" << endl;
        break;
    case eTypeFamilyPrimitive:
        SetPrimitive(depth, offset, info);
        break;
    case eTypeFamilyClass:
        SetClass(depth, offset, info);
        break;
    case eTypeFamilyChoice:
        SetChoice(depth, offset, info);
        break;
    case eTypeFamilyContainer:
        SetContainer(depth, offset, info);
        break;
    case eTypeFamilyPointer:
        SetPointer(depth, offset, info);
        break;
    }
    --depth;
}

void CDemoApp::SetPrimitive(int depth, size_t offset, CObjectInfo& info)
{
    EPrimitiveValueType type = info.GetPrimitiveValueType();
    switch (type) {
    case ePrimitiveValueSpecial:
        cout << "special" << endl;
        break;
    case ePrimitiveValueBool:
        info.SetPrimitiveValueBool(true);
        break;
    case ePrimitiveValueChar:
        info.SetPrimitiveValueChar('X');
        break;
    case ePrimitiveValueInteger:
        info.SetPrimitiveValueInt(123 + depth);
        break;
    case ePrimitiveValueReal:
        info.SetPrimitiveValueDouble(123.456 + depth);
        break;
    case ePrimitiveValueString:
        info.SetPrimitiveValueString("xxx");
        break;
    case ePrimitiveValueEnum:
        {
            const CEnumeratedTypeValues& values = info.GetEnumeratedTypeValues();
            CEnumeratedTypeValues::TValues all = values.GetValues();
            CEnumeratedTypeValues::TValues::const_iterator v = all.begin();
            if (all.size() > 1) {
                ++v;
            }
            if (all.size() > 2) {
                ++v;
            }
            info.SetPrimitiveValueInt(v->second);
        }
        break;
    case ePrimitiveValueOctetString:
        cout << "octet string" << endl;
        break;
    case ePrimitiveValueBitString:
        cout << "bitstring" << endl;
        break;
    case ePrimitiveValueAny:
        cout << "any" << endl;
        break;
    case ePrimitiveValueOther:
        cout << "other" << endl;
        break;
    }
}
void CDemoApp::SetClass(int depth, size_t offset, CObjectInfo& info)
{
    string off(offset,' ');
    cout << off << "class: " << info.GetName() << " {" << endl;
    CObjectInfoMI mem = info.BeginMembers();
    for ( ; mem.Valid(); ++mem) {
        if (depth > m_MaxDepth && mem.GetMemberInfo()->Optional()) {
            // set some optional ones, but not too many of them
            continue;
        }
        cout << off << " [" << mem.GetMemberIndex() << "]: "
                    << mem.GetMemberInfo()->GetId().GetName()
                    << endl;
        CObjectInfo tmp_info(info.SetClassMember(mem.GetMemberIndex()));
        SetDispatch(depth, offset, tmp_info);
    }
    cout << off << "} end of " << info.GetName() << endl;
}
void CDemoApp::SetChoice(int depth, size_t offset, CObjectInfo& info)
{
// select choice #2
    TMemberIndex ind = 2;
    if (depth > m_MaxDepth) {
        // or choice #1
        ind = 1;
    }

    string off(offset,' ');
    cout << off << "choice: " << info.GetName() << " {" << endl;
    cout << off << " [" << ind << "]: "
                << info.GetVariantIterator(ind).GetVariantInfo()->GetId().GetName()
                << " {" << endl;
    CObjectInfo tmp_info(info.SetChoiceVariant(ind));
    SetDispatch(depth,offset,tmp_info);
    cout << off << "} end of " << info.GetName() << endl;
}
void CDemoApp::SetContainer(int depth, size_t offset, CObjectInfo& info)
{
    string off(offset,' ');
    cout << off << "container of: " << info.GetName() << " {" << endl;
    for (int i=0; i<2; ++i) {
        if (info.GetElementType().GetTypeFamily() == eTypeFamilyPointer) {
            CObjectInfo tmp_info(info.AddNewPointedElement());
            SetDispatch(depth,offset,tmp_info);
        } else {
            CObjectInfo tmp_info(info.AddNewElement());
            SetDispatch(depth,offset,tmp_info);
        }
    }
    cout << off << "} end of " << info.GetName() << endl;
}
void CDemoApp::SetPointer(int depth, size_t offset, CObjectInfo& info)
{
    CObjectInfo tmp_info(info.SetPointedObject());
    SetDispatch(depth,offset,tmp_info);
}


int main(int argc, const char* argv[])
{
    return CDemoApp().AppMain(argc, argv);
}

