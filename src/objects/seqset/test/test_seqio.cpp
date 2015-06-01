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
#include <serial/objistrxml.hpp>
#include <serial/objostrxml.hpp>
#include <serial/objhook.hpp>
#include <serial/objcopy.hpp>
#include <corelib/ncbifile.hpp>
#include <common/test_data_path.h>
#include <objects/seqset/Seq_entry.hpp>
#include <corelib/test_boost.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <corelib/ncbierror.hpp>

/////////////////////////////////////////////////////////////////////////////
// Test ASN serialization

USING_NCBI_SCOPE;
USING_SCOPE(objects);

class CSysWatch
{
public:
    CSysWatch()
    {
        Reset();
    }
    void Reset(void)
    {
        GetCurrentProcessTimes(&m_User_time, &m_System_time);
    }
    double Elapsed(void)
    {
        double user, system;
        GetCurrentProcessTimes(&user, &system);
        return (user  - m_User_time) + (system - m_System_time);
    }
private:
    double m_User_time;
    double m_System_time;
};


BOOST_AUTO_TEST_CASE(s_TestAsnSerialization)
{
    typedef CSeq_entry TObject;
    string filename = "seq_entry1";

    const int kFmtCount = 5;
    const ESerialDataFormat fmt[kFmtCount] = {
        eSerial_AsnText,
        eSerial_AsnBinary,
        eSerial_Xml,
        eSerial_Xml,
        eSerial_Json
    };
    const bool std_xml[kFmtCount] = {
        false,
        false,
        false,
        true,
        false
    };
    const string ext[kFmtCount] = {
        ".asn",
        ".asb",
        ".xml",
        ".sxml",
        ".json"
    };

    string src_dir = CDirEntry::MakePath(NCBI_GetTestDataPath(),
                                         "objects/seqset/test");
    string dst_dir = ".";
    int in_i = 0;
    LOG_POST("-------------------------------------------------");
    LOG_POST("TestAsnSerialization");
    for ( ; in_i < kFmtCount; ++in_i ) {
        string loc_name = CDirEntry::MakePath(dst_dir, filename, ext[in_i]) + "_loc";
        {
            string in_name  = CDirEntry::MakePath(src_dir, filename, ext[in_i]);
            LOG_POST("Reading from "<<in_name);
            if (!CFile(in_name).Copy(loc_name))
            {
                LOG_POST("Error copying file:  " << CNcbiError::GetLast());
                if (!CFile(loc_name).Exists()) {
                    continue;
                }
            }
        }
        
        CRef<TObject> obj(new TObject);
        {
            auto_ptr<CObjectIStream> in(CObjectIStream::Open(loc_name,
                                                             fmt[in_i]));
            if ( std_xml[in_i] ) {
                dynamic_cast<CObjectIStreamXml*>(in.get())
                    ->SetEnforcedStdXml(true);
            }
            CSysWatch sw;
            in->Skip(obj->GetThisTypeInfo());
            LOG_POST(sw.Elapsed() << "s:  Skip");
        }
        {
            auto_ptr<CObjectIStream> in(CObjectIStream::Open(loc_name,
                                                             fmt[in_i]));
            if ( std_xml[in_i] ) {
                dynamic_cast<CObjectIStreamXml*>(in.get())
                    ->SetEnforcedStdXml(true);
            }
            CSysWatch sw;
            *in >> *obj;
            LOG_POST(sw.Elapsed() << "s:  Read");
        }
//        LOG_POST("Write into:");
        for ( int out_i = 0; out_i < kFmtCount; ++out_i ) {
            string ref_name = CDirEntry::MakePath(src_dir, filename, ext[out_i]);
            string out_name = CDirEntry::MakePath(dst_dir, filename, ext[out_i]);
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open(out_name,
                                                                  fmt[out_i]));
                if ( std_xml[out_i] ) {
                    dynamic_cast<CObjectOStreamXml*>(out.get())
                        ->SetEnforcedStdXml(true);
                }
                if (fmt[out_i] == eSerial_Json) {
                    out->SetWriteNamedIntegersByValue(false);
                }
//                CSysWatch sw;
                *out << *obj;
//                LOG_POST("\t" << ext[out_i] << "\t" << sw.Elapsed());
            }
            if ( fmt[out_i] == eSerial_AsnBinary ) {
                BOOST_REQUIRE(CFile(out_name).Compare(ref_name));
            }
            else {
                BOOST_REQUIRE(CFile(out_name).CompareTextContents(ref_name, CFile::eIgnoreWs));
            }
            CFile(out_name).Remove();
        }
        CFile(loc_name).Remove();
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
    string filename = "seq_entry1";

    const int kFmtCount = 5;
    const ESerialDataFormat fmt[kFmtCount] = {
        eSerial_AsnText,
        eSerial_AsnBinary,
        eSerial_Xml,
        eSerial_Xml,
        eSerial_Json
    };
    const bool std_xml[kFmtCount] = {
        false,
        false,
        false,
        true,
        false
    };
    const string ext[kFmtCount] = {
        ".asn",
        ".asb",
        ".xml",
        ".sxml",
        ".json"
    };
    string src_dir = CDirEntry::MakePath(NCBI_GetTestDataPath(),
                                         "objects/seqset/test");
    string dst_dir = ".";
    int in_i = 0;
    LOG_POST("-------------------------------------------------");
    LOG_POST("TestAsnSerializationWithHook");
    for ( ; in_i < kFmtCount; ++in_i ) {
        string loc_name = CDirEntry::MakePath(dst_dir, filename, ext[in_i]) + "_loc";
        {
            string in_name = CDirEntry::MakePath(src_dir, filename, ext[in_i]);
            LOG_POST("Reading from "<<in_name);
            if (!CFile(in_name).Copy(loc_name))
            {
                LOG_POST("Error copying file:  " << CNcbiError::GetLast());
                if (!CFile(loc_name).Exists()) {
                    continue;
                }
            }
        }
        
        CRef<TObject> obj(new TObject);
        {
            auto_ptr<CObjectIStream> in(CObjectIStream::Open(loc_name,
                                                             fmt[in_i]));
            if ( std_xml[in_i] ) {
                dynamic_cast<CObjectIStreamXml*>(in.get())
                    ->SetEnforcedStdXml(true);
            }
            CObjectHookGuard<CObject_id> g1("str", *new CSkipVariantHook, in.get());
            CSysWatch sw;
            in->Skip(obj->GetThisTypeInfo());
            LOG_POST(sw.Elapsed() << "s:  Skip");
        }
        {
            auto_ptr<CObjectIStream> in(CObjectIStream::Open(loc_name,
                                                             fmt[in_i]));
            if ( std_xml[in_i] ) {
                dynamic_cast<CObjectIStreamXml*>(in.get())
                    ->SetEnforcedStdXml(true);
            }
            CObjectHookGuard<CObject_id> g1("str", *new CReadVariantHook, in.get());
            CSysWatch sw;
            *in >> *obj;
            LOG_POST(sw.Elapsed() << "s:  Read");
        }
//        LOG_POST("Write into:");
        for ( int out_i = 0; out_i < kFmtCount; ++out_i ) {
            string ref_name = CDirEntry::MakePath(src_dir, filename, ext[out_i]);
            string out_name = CDirEntry::MakePath(dst_dir, filename, ext[out_i]);
            {
                auto_ptr<CObjectOStream> out(CObjectOStream::Open(out_name,
                                                                  fmt[out_i]));
                if ( std_xml[out_i] ) {
                    dynamic_cast<CObjectOStreamXml*>(out.get())
                        ->SetEnforcedStdXml(true);
                }
                if (fmt[out_i] == eSerial_Json) {
                    out->SetWriteNamedIntegersByValue(false);
                }
//                CSysWatch sw;
                *out << *obj;
//                LOG_POST("\t" << ext[out_i] << "\t" << sw.Elapsed());
            }
            if ( fmt[out_i] == eSerial_AsnBinary ) {
                BOOST_REQUIRE(CFile(out_name).Compare(ref_name));
            }
            else {
                BOOST_REQUIRE(CFile(out_name).CompareTextContents(ref_name, CFile::eIgnoreWs));
            }
            CFile(out_name).Remove();
        }
        CFile(loc_name).Remove();
    }
}

BOOST_AUTO_TEST_CASE(s_TestAsnSerializationCopy)
{
    typedef CSeq_entry TObject;
    string filename = "seq_entry1";

    const int kFmtCount = 5;
    const ESerialDataFormat fmt[kFmtCount] = {
        eSerial_AsnText,
        eSerial_AsnBinary,
        eSerial_Xml,
        eSerial_Xml,
        eSerial_Json
    };
    const bool std_xml[kFmtCount] = {
        false,
        false,
        false,
        true,
        false
    };
    const string ext[kFmtCount] = {
        ".asn",
        ".asb",
        ".xml",
        ".sxml",
        ".json"
    };
    string src_dir = CDirEntry::MakePath(NCBI_GetTestDataPath(),
                                         "objects/seqset/test");
    string dst_dir = ".";
    int in_i = 0;
    LOG_POST("-------------------------------------------------");
    LOG_POST("TestAsnSerializationCopy");
    for ( ; in_i < kFmtCount; ++in_i ) {
        string loc_name = CDirEntry::MakePath(dst_dir, filename, ext[in_i]) + "_loc";
        {
            string in_name = CDirEntry::MakePath(src_dir, filename, ext[in_i]);
            LOG_POST("Reading from "<<in_name);
            if (!CFile(in_name).Copy(loc_name))
            {
                LOG_POST("Error copying file:  " << CNcbiError::GetLast());
                if (!CFile(loc_name).Exists()) {
                    continue;
                }
            }
        }
        
        LOG_POST("Copy into:");
        for ( int out_i = 0; out_i < kFmtCount; ++out_i ) {
            string ref_name = CDirEntry::MakePath(src_dir, filename, ext[out_i]);
            string out_name = CDirEntry::MakePath(dst_dir, filename, ext[out_i]);

            {
                auto_ptr<CObjectIStream> in(CObjectIStream::Open(loc_name,
                                                                 fmt[in_i]));
                if ( std_xml[in_i] ) {
                    dynamic_cast<CObjectIStreamXml*>(in.get())
                        ->SetEnforcedStdXml(true);
                }

                auto_ptr<CObjectOStream> out(CObjectOStream::Open(out_name,
                                                                    fmt[out_i]));
                if ( std_xml[out_i] ) {
                    dynamic_cast<CObjectOStreamXml*>(out.get())
                        ->SetEnforcedStdXml(true);
                }
                if (fmt[out_i] == eSerial_Json) {
                    out->SetWriteNamedIntegersByValue(false);
                }

                CObjectStreamCopier copier(*in,*out);
                CSysWatch sw;
                copier.Copy(TObject::GetTypeInfo());
                LOG_POST("\t" << ext[out_i] << "\t" << sw.Elapsed());
            }


            if ( fmt[out_i] == eSerial_AsnBinary ) {
                BOOST_REQUIRE(CFile(out_name).Compare(ref_name));
            }
            else {
                BOOST_REQUIRE(CFile(out_name).CompareTextContents(ref_name, CFile::eIgnoreWs));
            }
            CFile(out_name).Remove();
        }
        CFile(loc_name).Remove();
    }
}
