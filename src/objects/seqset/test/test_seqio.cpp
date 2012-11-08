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
 * Author:  Eugene Vasilchenko
 *
 * File Description:
 *   .......
 *
 */

#include <ncbi_pch.hpp>
#include <serial/serial.hpp>
#include <serial/objistr.hpp>
#include <serial/objostr.hpp>
#include <serial/objhook.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <corelib/test_boost.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>

/////////////////////////////////////////////////////////////////////////////
// Test ASN serialization

USING_NCBI_SCOPE;
USING_SCOPE(objects);

const char* const dir = "test_data/";

BOOST_AUTO_TEST_CASE(s_TestAsnSerialization)
{
    typedef CSeq_entry TObject;
    string name = "seq_entry1";

    const int kFmtCount = 4;
    const ESerialDataFormat fmt[kFmtCount] = {
        eSerial_AsnText,
        eSerial_AsnBinary,
        eSerial_Xml,
        eSerial_Json
    };
    const string ext[kFmtCount] = {
        ".asn",
        ".asb",
        ".xml",
        ".json"
    };
    for ( int in_i = 0; in_i < kFmtCount; ++in_i ) {
        string in_name = dir+name+ext[in_i];
        LOG_POST("Reading from "<<in_name);
        
        CRef<TObject> obj(new TObject);
        if ( fmt[in_i] != eSerial_Json ) {
            auto_ptr<CObjectIStream> in(CObjectIStream::Open(in_name,
                                                             fmt[in_i]));
            in->Skip(obj->GetThisTypeInfo());
        }
        {
            auto_ptr<CObjectIStream> in(CObjectIStream::Open(in_name,
                                                             fmt[in_i]));
            *in >> *obj;
        }
        for ( int out_i = 0; out_i < kFmtCount; ++out_i ) {
            string ref_name = dir+name+ext[out_i];
            string out_name = ref_name+".out";
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open(out_name,
                                                                  fmt[out_i]));
                *out << *obj;
            }
            if ( fmt[out_i] == eSerial_AsnBinary ) {
                BOOST_REQUIRE(CFile(out_name).Compare(ref_name));
            }
            else {
                BOOST_REQUIRE(CFile(out_name).CompareTextContents(ref_name, CFile::eIgnoreWs));
            }
            CFile(out_name).Remove();
        }
    }
}


class CReadVariantHook : public CReadChoiceVariantHook
{
public:
    virtual void ReadChoiceVariant(CObjectIStream& stream,
                                   const CObjectInfoCV& variant)
        {
            DefaultRead(stream, variant);
        }
};


class CSkipVariantHook : public CSkipChoiceVariantHook
{
public:
    virtual void SkipChoiceVariant(CObjectIStream& stream,
                                   const CObjectTypeInfoCV& variant)
        {
            stream.SkipObject(variant.GetVariantType().GetTypeInfo());
        }
};


BOOST_AUTO_TEST_CASE(s_TestAsnSerializationWithHook)
{
    typedef CSeq_entry TObject;
    string name = "seq_entry1";

    const int kFmtCount = 4;
    const ESerialDataFormat fmt[kFmtCount] = {
        eSerial_AsnText,
        eSerial_AsnBinary,
        eSerial_Xml,
        eSerial_Json
    };
    const string ext[kFmtCount] = {
        ".asn",
        ".asb",
        ".xml",
        ".json"
    };
    for ( int in_i = 0; in_i < kFmtCount; ++in_i ) {
        string in_name = dir+name+ext[in_i];
        LOG_POST("Reading from "<<in_name);
        
        CRef<TObject> obj(new TObject);
        if ( fmt[in_i] != eSerial_Json ) {
            auto_ptr<CObjectIStream> in(CObjectIStream::Open(in_name,
                                                             fmt[in_i]));
            CObjectHookGuard<CObject_id> g1("str", *new CSkipVariantHook, in.get());
            in->Skip(obj->GetThisTypeInfo());
        }
        {
            auto_ptr<CObjectIStream> in(CObjectIStream::Open(in_name,
                                                             fmt[in_i]));
            CObjectHookGuard<CObject_id> g1("str", *new CReadVariantHook, in.get());
            *in >> *obj;
        }
        for ( int out_i = 0; out_i < kFmtCount; ++out_i ) {
            string ref_name = dir+name+ext[out_i];
            string out_name = ref_name+".out";
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open(out_name,
                                                                  fmt[out_i]));
                *out << *obj;
            }
            if ( fmt[out_i] == eSerial_AsnBinary ) {
                BOOST_REQUIRE(CFile(out_name).Compare(ref_name));
            }
            else {
                BOOST_REQUIRE(CFile(out_name).CompareTextContents(ref_name, CFile::eIgnoreWs));
            }
            CFile(out_name).Remove();
        }
    }
}
