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
*   Simple program demonstrating how to a assign value to a serial object.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id_set.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <serial/objistr.hpp>

USING_SCOPE(ncbi);
USING_SCOPE(objects);

int main(int argc, const char* argv[])
{
    {{
        // set primitive value
        CSeq_id obj;
        CObjectInfo info(ObjectInfo(obj));
        CObjectInfo local(info.SetChoiceVariant(info.FindVariantIndex("local")));
        local.SetChoiceVariant(local.FindVariantIndex("id")).SetPrimitiveValueInt(234);
        cout << MSerial_AsnText << obj;
    }}

    {{
        CSeq_loc obj;
        CObjectInfo info(ObjectInfo(obj));

        // set choice variant
        info.SetChoiceVariant(info.FindVariantIndex("null"));
        cout << MSerial_AsnText << obj;

        // change choice variant
        CObjectInfo seqid(info.SetChoiceVariant(info.FindVariantIndex("empty")));
        CObjectInfo local(seqid.SetChoiceVariant(seqid.FindVariantIndex("local")));
        local.SetChoiceVariant(local.FindVariantIndex("id")).SetPrimitiveValueInt(987);
        cout << MSerial_AsnText << obj;
        
        // there is no way to reset choice variant
        // this does not work:
        //info.SetChoiceVariant(0);
    }}

    {{
        CSeq_id_set obj;
        CObjectInfo info(ObjectInfo(obj));
        // 'implicit' class has one member with no name
        CObjectInfo mem(info.SetClassMember(1));

        // add container element
        CObjectInfo elem(mem.AddNewPointedElement());
        CObjectInfo local(elem.SetChoiceVariant(elem.FindVariantIndex("local")));
        local.SetChoiceVariant(local.FindVariantIndex("id")).SetPrimitiveValueInt(345);

        // add container element
        elem = mem.AddNewPointedElement();
        local = elem.SetChoiceVariant(elem.FindVariantIndex("local"));
        local.SetChoiceVariant(local.FindVariantIndex("str")).SetPrimitiveValueString("345string");

        // add container element
        elem = mem.AddNewPointedElement();
        CObjectInfo prf = elem.SetChoiceVariant(elem.FindVariantIndex("prf"));
        prf.SetClassMember(prf.FindMemberIndex("name")).SetPrimitiveValueString("prfname");
        prf.SetClassMember(prf.FindMemberIndex("release")).SetPrimitiveValueString("prfrelease");
        prf.SetClassMember(prf.FindMemberIndex("version")).SetPrimitiveValueInt(567);

        cout << MSerial_AsnText << obj;

        // unset class member
        // only optional members can be unset
        if (prf.FindClassMember("version").IsSet()) {
            prf.FindClassMember("version").Reset();
        }
        cout << MSerial_AsnText << obj;
    }}

    return 0;
}

