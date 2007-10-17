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
* Author:  Aaron Ucko
*
* File Description:
*   Converts between User-objects and arbitrary other serializable objects.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objects/general/uoconv.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/User_object.hpp>
#include <objects/error_codes.hpp>

#include <serial/objectiter.hpp>

#if BITSTRING_AS_VECTOR
#  include <util/resize_iter.hpp>
#else
#  include <util/bitset/bmserial.h>
#endif


#define NCBI_USE_ERRCODE_X   Objects_UOConv

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

typedef CUser_field::TData TUFData;
typedef TUFData::TOs       TUFDOs;

static CRef<CUser_field> s_PackAsUserField(CConstObjectInfo obj,
                                           const string* label = 0);

static void s_SetOSFromBS(TUFDOs& os, CBitString& bs)
{
#if BITSTRING_AS_VECTOR
    os.resize((bs.size() + CHAR_BIT - 1) / CHAR_BIT + 1);
    // data.SetNum(bs.size());
    os[0] = static_cast<unsigned char>(bs.size());
    Int8 i = 1;
    for (CConstResizingIterator<CBitString, char> it(bs, CHAR_BIT);
         !it.AtEnd();  ++it) {
        os[++i] = *it;
    }
#else
    CBitString::statistics st;
    bs.calc_stat(&st);
    os.resize(st.max_serialize_mem);
    SIZE_TYPE n = bm::serialize(bs, reinterpret_cast<unsigned char*>(&os[0]));
    os.resize(n);
#endif
}

static void s_SetBSFromOS(CBitString& bs, const TUFDOs& os)
{
#if BITSTRING_AS_VECTOR
    bs.resize(os.size() * CHAR_BIT);
    Int8 i = 1;
    for (CResizingIterator<CBitString, char> it(bs, CHAR_BIT);
         !it.AtEnd();  ++it) {
        *it = os[++i];
    }
    Int8 count = (os.size() - ((os[0] % CHAR_BIT) ? 2 : 1)) * CHAR_BIT;
    _ASSERT( !(static_cast<unsigned char>(count) & ~os[0]) );
    count |= os[0];
    bs.resize(count);
#else
    bm::deserialize(bs, reinterpret_cast<const unsigned char*>(&os[0]));
#endif
}

static void s_SetFieldsFromAnyContent(CUser_field& parent,
                                      const CAnyContentObject& obj)
{
    parent.SetNum(obj.GetAttributes().size() + 4);

    parent.AddField("name",      obj.GetName());
    parent.AddField("value",     obj.GetValue());
    parent.AddField("ns_name",   obj.GetNamespaceName());
    parent.AddField("ns_prefix", obj.GetNamespacePrefix());
    
    ITERATE (vector<CSerialAttribInfoItem>, it, obj.GetAttributes()) {
        parent.AddField(it->GetNamespaceName() + ":" + it->GetName(),
                        it->GetValue());
    }
}

static void s_SetAnyContentFromFields(CAnyContentObject& obj,
                                      const TUFData::TFields& fields)
{
    ITERATE (TUFData::TFields, it, fields) {
        const string& name  = (*it)->GetLabel().GetStr();
        const string& value = (*it)->GetData().GetStr();
        SIZE_TYPE     colon = name.find(':');
        if (colon != NPOS) {
            obj.AddAttribute(name.substr(colon + 1), name.substr(0, colon - 1),
                             value);
        } else if (name == "name") {
            obj.SetName(value);
        } else if (name == "value") {
            obj.SetValue(value);
        } else if (name == "ns_name") {
            obj.SetNamespaceName(value);
        } else if (name == "ns_prefix") {
            obj.SetNamespacePrefix(value);
        } else {
            NCBI_THROW(CSerialException, eInvalidData,
                       "Bad User-object encoding.");
        }
    }
}

static void s_SetPrimitiveData(CUser_field& field, CConstObjectInfo obj)
{
    TUFData& data = field.SetData();
    switch (obj.GetPrimitiveValueType()) {
    case ePrimitiveValueSpecial:
        data.SetBool(true);
        break;

    case ePrimitiveValueBool:
        data.SetBool(obj.GetPrimitiveValueBool());
        break;

    case ePrimitiveValueChar:
        data.SetStr(string(1, obj.GetPrimitiveValueChar()));
        break;

    case ePrimitiveValueInteger:
        if (obj.IsPrimitiveValueSigned()) {
            data.SetInt(obj.GetPrimitiveValueInt());
        } else {
            data.SetInt(static_cast<int>(obj.GetPrimitiveValueUInt()));
        }
        break;

    case ePrimitiveValueReal:
        data.SetReal(obj.GetPrimitiveValueDouble());
        break;

    case ePrimitiveValueString:
        obj.GetPrimitiveValueString(data.SetStr());
        break;

    case ePrimitiveValueEnum:
        try {
            obj.GetPrimitiveValueString(data.SetStr());
        } catch (CSerialException&) {
            data.SetInt(obj.GetPrimitiveValueInt());
        }
        break;

    case ePrimitiveValueOctetString:
        obj.GetPrimitiveValueOctetString(data.SetOs());
        break;

    case ePrimitiveValueBitString:
    {
        CBitString bs;
        obj.GetPrimitiveValueBitString(bs);
        s_SetOSFromBS(data.SetOs(), bs);
        break;
    }

    case ePrimitiveValueAny:
    {
        CAnyContentObject obj2;
        obj.GetPrimitiveValueAnyContent(obj2);
        s_SetFieldsFromAnyContent(field, obj2);
        break;
    }

    case ePrimitiveValueOther:
        ERR_POST_X(1, Warning
                   << "s_SetPrimitiveData: ignoring ePrimitiveValueOther");
        break;
    }
}

static CUser_field::TNum s_SetContainerData(TUFData& data,
                                            CConstObjectInfo obj)
{
    CUser_field::TNum count;
    try {
        count = obj.GetContainerTypeInfo()
            ->GetElementCount(obj.GetObjectPtr());
    } catch (...) {
        count = 0;
    }

    if (count > 0) {
        switch (obj.GetElementType().GetTypeFamily()) {
        case eTypeFamilyPrimitive:
            switch (obj.GetElementType().GetPrimitiveValueType()) {
            case ePrimitiveValueSpecial: // can this happen?!
            case ePrimitiveValueBool:
            case ePrimitiveValueInteger:
                data.SetInts().reserve(count);
                break;
            case ePrimitiveValueChar:
            case ePrimitiveValueString:
            case ePrimitiveValueEnum:
                data.SetStrs().reserve(count);
                break;
            case ePrimitiveValueReal:
                data.SetReals().reserve(count);
                break;
            case ePrimitiveValueOctetString:
            case ePrimitiveValueBitString:
                data.SetOss().reserve(count);
                break;
            case ePrimitiveValueAny:
                data.SetFields().reserve(count);
                break;
            case ePrimitiveValueOther:
                ERR_POST_X(2, Warning << "s_SetContainerData:"
                           " ignoring ePrimitiveValueOther");
                break;
            }
        default:
            data.SetFields().reserve(count);
        }
    }

    count = 0;
    for (CConstObjectInfo::CElementIterator it = obj.BeginElements();
         it;  ++it) {
        CConstObjectInfo obj2(*it);
        switch (obj2.GetTypeFamily()) {
        case eTypeFamilyPrimitive:
            switch (obj2.GetPrimitiveValueType()) {
            case ePrimitiveValueSpecial: // can this happen?!
                data.SetInts().push_back(1);
                break;
            case ePrimitiveValueBool:
                data.SetInts().push_back(obj2.GetPrimitiveValueBool());
                break;
            case ePrimitiveValueChar:
                data.SetStrs()
                    .push_back(string(1, obj2.GetPrimitiveValueChar()));
                break;
            case ePrimitiveValueInteger:
                if (obj2.IsPrimitiveValueSigned()) {
                    data.SetInts().push_back(obj2.GetPrimitiveValueInt());
                } else {
                    data.SetInts().push_back(static_cast<int>
                                             (obj2.GetPrimitiveValueUInt()));
                }
                break;
            case ePrimitiveValueReal:
                data.SetReals().push_back(obj2.GetPrimitiveValueDouble());
                break;
            case ePrimitiveValueString:
                data.SetStrs().push_back(obj2.GetPrimitiveValueString());
                break;
            case ePrimitiveValueEnum:
            {
                string s;
                try {
                    obj2.GetPrimitiveValueString(s);
                } catch (CSerialException&) {
                    s = NStr::IntToString(obj.GetPrimitiveValueInt());
                }
                data.SetStrs().push_back(s);
                break;
            }
            case ePrimitiveValueOctetString:
            {
                TUFData::TOs* os = new TUFData::TOs;
                obj.GetPrimitiveValueOctetString(*os);
                data.SetOss().push_back(os);
                break;
            }
            case ePrimitiveValueBitString:
            {
                TUFData::TOs* os = new TUFData::TOs;
                CBitString bs;
                obj.GetPrimitiveValueBitString(bs);
                s_SetOSFromBS(*os, bs);
                data.SetOss().push_back(os);
                break;
            }
            case ePrimitiveValueAny:
            {
                CRef<CUser_field> field(new CUser_field);
                CAnyContentObject aco;
                obj.GetPrimitiveValueAnyContent(aco);
                s_SetFieldsFromAnyContent(*field, aco);
                data.SetFields().push_back(field);
                break;
            }
            case ePrimitiveValueOther:
                ERR_POST_X(3, Warning << "s_SetContainerData:"
                           " ignoring ePrimitiveValueOther");
                break;
            }
            break;

        case eTypeFamilyPointer:
        {
            CConstObjectInfo obj3 = obj2.GetPointedObject();
            data.SetFields().push_back(s_PackAsUserField(obj3 ? obj3 : obj2));
            break;
        }

        default:
            data.SetFields().push_back(s_PackAsUserField(obj2));
        }
        ++count;
    }
    return count;
}

CRef<CUser_field> s_PackAsUserField(CConstObjectInfo obj, const string* label)
{
    CRef<CUser_field> field(new CUser_field);
    if (label) {
        field->SetLabel().SetStr(*label);
    } else {
        field->SetLabel().SetId(0);
    }

    TUFData& data = field->SetData();
    switch (obj.GetTypeFamily()) {
    case eTypeFamilyPrimitive:
        s_SetPrimitiveData(*field, obj);
        break;

    case eTypeFamilyContainer:
        field->SetNum(s_SetContainerData(data, obj));
        break;

    case eTypeFamilyClass:
        for (CConstObjectInfo::CMemberIterator it = obj.BeginMembers();
             it;  ++it) {
            if (it.IsSet()) {
                if (it.GetAlias().empty()  &&
                    obj.GetClassTypeInfo()->GetItems().LastIndex() == 1) {
                    // just a wrapper
                    return s_PackAsUserField(*it, label);
                } else {
                    data.SetFields().push_back(s_PackAsUserField
                                               (*it, &it.GetAlias()));
                }
            }
        }
        field->SetNum(data.GetFields().size());
        break;

    case eTypeFamilyChoice:
    {
        CConstObjectInfo::CChoiceVariant var = obj.GetCurrentChoiceVariant();
        field->SetNum(1);
        data.SetFields().push_back(s_PackAsUserField(*var, &var.GetAlias()));
        break;
    }

    case eTypeFamilyPointer:
    {
        CConstObjectInfo obj2 = obj.GetPointedObject();
        if (obj2) {
            return s_PackAsUserField(obj2);
        } else { // how to represent?
            field->SetNum(0);
            data.SetFields().clear();
        }
        break;
    }
    }

    return field;
}

CRef<CUser_object> PackAsUserObject(CConstObjectInfo obj)
{
    CRef<CUser_object> uo(new CUser_object);
    uo->SetClass(obj.GetTypeInfo()->GetModuleName());
    uo->SetType().SetStr(obj.GetTypeInfo()->GetName());
    uo->SetData().push_back(s_PackAsUserField(obj));
    return uo;
}


static void s_UnpackUserField(const CUser_field& uo, CObjectInfo obj);

static void s_UnpackPrimitiveField(const TUFData& data, CObjectInfo obj)
{
    switch (obj.GetPrimitiveValueType()) {
    case ePrimitiveValueSpecial:
        break;

    case ePrimitiveValueBool:
        obj.SetPrimitiveValueBool(data.GetBool());
        break;

    case ePrimitiveValueChar:
        obj.SetPrimitiveValueChar(data.GetStr()[0]);
        break;

    case ePrimitiveValueInteger:
        if (obj.IsPrimitiveValueSigned()) {
            obj.SetPrimitiveValueInt(data.GetInt());
        } else {
            obj.SetPrimitiveValueUInt(static_cast<unsigned int>
                                      (data.GetInt()));
        }
        break;

    case ePrimitiveValueReal:
        obj.SetPrimitiveValueDouble(data.GetReal());
        break;

    case ePrimitiveValueString:
        obj.SetPrimitiveValueString(data.GetStr());
        break;

    case ePrimitiveValueEnum:
        switch (data.Which()) {
        case TUFData::e_Int:
            obj.SetPrimitiveValueInt(data.GetInt());
            break;
        case TUFData::e_Str:
            obj.SetPrimitiveValueString(data.GetStr());
            break;
        default:
            NCBI_THROW(CSerialException, eInvalidData,
                       "Bad User-object encoding.");
        }

    case ePrimitiveValueOctetString:
        obj.SetPrimitiveValueOctetString(data.GetOs());

    case ePrimitiveValueBitString:
    {
        CBitString bs;
        s_SetBSFromOS(bs, data.GetOs());
        obj.SetPrimitiveValueBitString(bs);
    }

    case ePrimitiveValueAny:
    {
        CAnyContentObject aco;
        s_SetAnyContentFromFields(aco, data.GetFields());
        obj.SetPrimitiveValueAnyContent(aco);
    }

    case ePrimitiveValueOther:
        ERR_POST_X(4, Warning << "s_UnpackPrimitiveField:"
                   " ignoring ePrimitiveValueOther");
        break;
    }
}

static void s_UnpackContainerField(const TUFData& data, CObjectInfo obj)
{
    const CContainerTypeInfo* continfo = obj.GetContainerTypeInfo();
    CObjectTypeInfo           elt_oti  = obj.GetElementType();
    TTypeInfo                 elt_ti   = elt_oti.GetTypeInfo();
    TObjectPtr                objp     = obj.GetObjectPtr();
    switch (data.Which()) {
    case TUFData::e_Strs:
        if (elt_oti.GetTypeFamily() != eTypeFamilyPrimitive) {
            NCBI_THROW(CSerialException, eInvalidData,
                       "Bad User-object encoding.");
        }
        try {
            continfo->ReserveElements(objp, data.GetStrs().size());
        } catch (CSerialException&) {
            // ignore; not implemented by all containers
        }
        ITERATE (TUFData::TStrs, it, data.GetStrs()) {
            TObjectPtr p = elt_ti->Create();
            CObjectInfo obj2(p, elt_ti, CObjectInfo::eNonCObject);
            switch (elt_oti.GetPrimitiveValueType()) {
            case ePrimitiveValueChar:
                obj2.SetPrimitiveValueChar((*it)[0]);
                break;
            case ePrimitiveValueString:
                obj2.SetPrimitiveValueString(*it);
            case ePrimitiveValueEnum:
                try {
                    obj2.SetPrimitiveValueInt(NStr::StringToInt(*it));
                } catch (CStringException&) {
                    obj2.SetPrimitiveValueString(*it);
                }
            default:
                NCBI_THROW(CSerialException, eInvalidData,
                           "Bad User-object encoding.");
            }
            elt_ti->Delete(p);
        }
        break;

    case TUFData::e_Ints:
        if (elt_oti.GetTypeFamily() != eTypeFamilyPrimitive) {
            NCBI_THROW(CSerialException, eInvalidData,
                       "Bad User-object encoding.");
        }
        try {
            continfo->ReserveElements(objp, data.GetInts().size());
        } catch (CSerialException&) {
            // ignore; not implemented by all containers
        }
        ITERATE (TUFData::TInts, it, data.GetInts()) {
            TObjectPtr p = elt_ti->Create();
            CObjectInfo obj2(p, elt_ti, CObjectInfo::eNonCObject);
            switch (elt_oti.GetPrimitiveValueType()) {
            case ePrimitiveValueSpecial:
                break;
            case ePrimitiveValueBool:
                obj2.SetPrimitiveValueBool(*it ? true : false);
                break;
            case ePrimitiveValueInteger:
            case ePrimitiveValueEnum:
                if (obj2.IsPrimitiveValueSigned()) {
                    obj2.SetPrimitiveValueInt(*it);
                } else {
                    obj2.SetPrimitiveValueUInt(static_cast<unsigned int>(*it));
                }
                break;
            default:
                NCBI_THROW(CSerialException, eInvalidData,
                           "Bad User-object encoding.");
            }
            elt_ti->Delete(p);
        }
        break;

    case TUFData::e_Reals:
        if (elt_oti.GetTypeFamily() != eTypeFamilyPrimitive
            ||  elt_oti.GetPrimitiveValueType() != ePrimitiveValueReal) {
            NCBI_THROW(CSerialException, eInvalidData,
                       "Bad User-object encoding.");
        }
        try {
            continfo->ReserveElements(objp, data.GetReals().size());
        } catch (CSerialException&) {
            // ignore; not implemented by all containers
        }
        ITERATE (TUFData::TReals, it, data.GetReals()) {
            TObjectPtr p = elt_ti->Create();
            CObjectInfo obj2(p, elt_ti, CObjectInfo::eNonCObject);
            obj2.SetPrimitiveValueDouble(*it);
            elt_ti->Delete(p);
        }
        break;

    case TUFData::e_Oss:
        if (elt_oti.GetTypeFamily() != eTypeFamilyPrimitive) {
            NCBI_THROW(CSerialException, eInvalidData,
                       "Bad User-object encoding.");
        }
        try {
            continfo->ReserveElements(objp, data.GetOss().size());
        } catch (CSerialException&) {
            // ignore; not implemented by all containers
        }
        ITERATE (TUFData::TOss, it, data.GetOss()) {
            TObjectPtr p = elt_ti->Create();
            CObjectInfo obj2(p, elt_ti, CObjectInfo::eNonCObject);
            switch (elt_oti.GetPrimitiveValueType()) {
            case ePrimitiveValueOctetString:
                obj2.SetPrimitiveValueOctetString(**it);
                break;

            case ePrimitiveValueBitString:
            {
                CBitString bs;
                s_SetBSFromOS(bs, **it);
                obj.SetPrimitiveValueBitString(bs);
                break;
            }

            default:
                NCBI_THROW(CSerialException, eInvalidData,
                           "Bad User-object encoding.");
            }
            elt_ti->Delete(p);
        }
        break;

    case TUFData::e_Fields:
        try {
            continfo->ReserveElements(objp, data.GetFields().size());
        } catch (CSerialException&) {
            // ignore; not implemented by all containers
        }
        ITERATE (TUFData::TFields, it, data.GetFields()) {
            TObjectPtr p = elt_ti->Create();
            CObjectInfo obj2(p, elt_ti);
            s_UnpackUserField(**it, obj2);
            if ( !elt_ti->IsCObject() ) {
                elt_ti->Delete(p);
            }
        }
        break;

    default:
        break;
    }
}

static void s_UnpackUserField(const CUser_field& field, CObjectInfo obj)
{
    _ASSERT(obj  &&  obj.GetObjectPtr());
    const TUFData& data = field.GetData();
    switch (obj.GetTypeFamily()) {
    case eTypeFamilyPrimitive:
        s_UnpackPrimitiveField(data, obj);
        break;

    case eTypeFamilyContainer:
        s_UnpackContainerField(data, obj);
        break;

    case eTypeFamilyClass:
        ITERATE (TUFData::TFields, it, data.GetFields()) {
            if ((*it)->GetLabel().IsStr()) {
                string name = (*it)->GetLabel().GetStr();
                CObjectInfo::CMemberIterator mi
                    = obj.FindClassMember(NStr::ToLower(name));
                // make sure there actually *is* such a field?
                if (mi.IsSet()) {
                    // complain! (dup, presumably...)
                }
                s_UnpackUserField(**it, *mi);
            } else {
                // complain!
            }
        }
        // make sure we got all mandatory fields
        break;

    case eTypeFamilyChoice:
        if (data.GetFields().size() == 1) {
            const CUser_field& field2 = *data.GetFields().front();
            TMemberIndex       index;
            switch (field2.GetLabel().Which()) {
            case CObject_id::e_Str:
            {
                string name = field2.GetLabel().GetStr();
                index = obj.FindVariantIndex(NStr::ToLower(name));
                break;
            }
            case CObject_id::e_Id:
                index = obj.FindVariantIndex(field.GetLabel().GetId());
                break;
            default:
                index = kInvalidMember;
                break;
            }
            // make sure index is valid?
            obj.GetChoiceTypeInfo()->SetIndex(obj.GetObjectPtr(), index);
            s_UnpackUserField(field2, *obj.GetCurrentChoiceVariant());
        } else {
            // complain!
        }
        break;

    case eTypeFamilyPointer:
        // initialize pointer if NULL?
        s_UnpackUserField(field, obj.GetPointedObject());
        break;
    }
}


void UnpackUserObject(const CUser_object& uo, CObjectInfo obj)
{
    _ASSERT(obj  &&  obj.GetObjectPtr());
    s_UnpackUserField(*uo.GetData().front(), obj);
}

CObjectInfo UnpackUserObject(const CUser_object& uo, const CTypeInfo* ti)
{
    _ASSERT(ti);
    CObjectInfo obj(ti);
    UnpackUserObject(uo, obj);
    return obj;
}


END_SCOPE(objects)
END_NCBI_SCOPE
