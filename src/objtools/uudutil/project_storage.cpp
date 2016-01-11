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
 * Author:  Liangshou Wu, Victor Joukov
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/rwstream.hpp>
#include <corelib/ncbi_url.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <serial/objostr.hpp>
#include <serial/iterator.hpp>

#include <util/compress/zlib.hpp>
#include <util/compress/lzo.hpp>
#include <util/compress/bzip2.hpp>
#include <util/compress/stream.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/gbproj/GBProject_ver2.hpp>
#include <objects/gbproj/ProjectFolder.hpp>

#include <objtools/uudutil/project_storage.hpp>

#include <connect/services/netcache_api.hpp>


#include <math.h>
#include <sstream>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

/// Predefined constants.
/// GB project magic number
static const Uint2 kNC_ProjectMagic = 0x3232;
/// string magic number
/// This is used only when explicitly calling SaveString().
/// For anything else, kNC_ProjectMagic is used.
static const Uint2 kNC_StringMagic = 0x3333;

/// version number
static const Uint2 kNC_CurrentVersion = 1;

static const char* kKeyError = "The given key is invalid or inaccessible!";

static const CCompression::ELevel kCompressionLevel = CCompression::eLevel_Lowest;
static const streamsize kBufSize = 16384;
static const int kWindowBits = 15;
static const int kMemLevel = 9;
static const int kStrategy = kZlibDefaultStrategy;



CProjectStorage::CProjectStorage(const string& client, const string& nc_service,
                                 const string& password, TNetStorageFlags default_flags) : 
    m_Magic(kNC_ProjectMagic),
    m_Version(kNC_CurrentVersion),
    m_CmprsFmt(eNC_Uncompressed),
    m_DataFmt(eSerial_None),
    m_Password(password),
    m_HasNetStorage(false),
    m_NS(eVoid)
{

    if (!m_Password.empty()) {
        if (NStr::Find(nc_service, "&") != NPOS) { 
            CUrlArgs url_parser(nc_service);
            m_NC.reset(new CNetCacheAPI(url_parser.GetValue("nc"), url_parser.GetValue("client")));
        } else {
            m_NC.reset(new CNetCacheAPI(nc_service, client));
        }
    } else {
        m_HasNetStorage = true;

        if (NStr::Find(nc_service, "&") == NPOS) { // old style use CNetCacheAPI for writing and CNetStorage for reading
            m_NC.reset(new CNetCacheAPI(nc_service, client));
            string ns_service = "client=";
            ns_service += client;
            m_NS = CNetStorage(ns_service, default_flags);
        } else {
            m_NS = CNetStorage(nc_service, default_flags);
        }
    }
}


void CProjectStorage::SetCommTimeout(float sec)
{
    if (sec > 0 && m_NC) {
        unsigned int num_sec  = (unsigned int)floor(sec);
        unsigned int num_msec = (unsigned int)((sec - num_sec) * 1000000.0f);
        STimeout to = {num_sec, num_msec};
        m_NC->SetCommunicationTimeout(to);
    }
}


string CProjectStorage::SaveProject(const IGBProject& project,
                                    const string& key, 
                                    TCompressionFormat compression_fmt, 
                                    TDataFormat data_fmt, 
                                    unsigned int time_to_live,
                                    TNetStorageFlags default_flags)
{
    const CGBProject_ver2* prj_ver2 =
        dynamic_cast<const CGBProject_ver2*>(&project);
    if ( !prj_ver2 ) {
        return key;
    }

    // Only support ASN text/binary when saving GB Project
    x_ValidateAsnSerialFormat(data_fmt);

    string nc_key = key;
    CNetStorageObject nso;
    auto_ptr<CObjectOStream> ostr(x_GetObjectOStream(data_fmt, nso, nc_key,
                                                     compression_fmt,
                                                     time_to_live,
                                                     default_flags));
    // Set magic to kNC_ProjectMagic
    m_Magic = kNC_ProjectMagic;
    *ostr << *prj_ver2;
    ostr.reset(0);

    if (nso) {
        //        nso.SetAttribute("type", prj_ver2->GetTypeInfo()->GetName());
        nso.Close();
    }

    return nc_key;
}


string CProjectStorage::SaveObject(const CSerialObject& obj,
                                   const string& key, 
                                   TCompressionFormat compression_fmt, 
                                   TDataFormat data_fmt, 
                                   unsigned int time_to_live,
                                   TNetStorageFlags default_flags)
{
    string nc_key = key;
    CNetStorageObject nso;
    auto_ptr<CObjectOStream> ostr(x_GetObjectOStream(data_fmt, nso, nc_key,
                                                     compression_fmt,
                                                     time_to_live,
                                                     default_flags));
    *ostr << obj;
    ostr->Close();
    ostr.reset(0);
    if (nso) {
        nso.SetAttribute("type", obj.GetThisTypeInfo()->GetName());
        nso.Close();
    }
    

    return nc_key;
}


string CProjectStorage::SaveString(const string& str,
                                   const string& key,
                                   TCompressionFormat compression_fmt,
                                   unsigned int time_to_live,
                                   TNetStorageFlags default_flags)
{
    x_ValidateCompressionFormat(compression_fmt);
    
    m_CmprsFmt = compression_fmt;
    m_Magic = kNC_StringMagic;
    string nc_key = key;
    CNetStorageObject nso;
    auto_ptr<CNcbiOstream> ostr = x_GetOutputStream(nc_key, time_to_live, default_flags, nso);
    *ostr << str;
    ostr.reset(0);
    if (nso) 
        nso.Close();
    return nc_key;
}


string CProjectStorage::SaveStream(CNcbiIstream& istream,
                                   const string& key,
                                   TCompressionFormat compression_fmt,
                                   unsigned int time_to_live,
                                   TNetStorageFlags default_flags)
{
    x_ValidateCompressionFormat(compression_fmt);

    m_CmprsFmt = compression_fmt;
    m_Magic = kNC_ProjectMagic;
    
    string nc_key = key;
    CNetStorageObject nso;
    auto_ptr<CNcbiOstream> ostr = x_GetOutputStream(nc_key, time_to_live, default_flags, nso);
    NcbiStreamCopy(*ostr, istream);
    ostr.reset(0);
    if (nso) 
        nso.Close();

    return nc_key;
}


CObjectOStream* CProjectStorage::AsObjectOStream(TDataFormat data_fmt,
                                                 string& key,
                                                 TCompressionFormat compression_fmt,
                                                 unsigned int time_to_live,
                                                 TNetStorageFlags default_flags)
{
    CNetStorageObject nso;
    return x_GetObjectOStream(data_fmt, nso, key, compression_fmt, time_to_live, default_flags);
}


CObjectOStream* CProjectStorage::x_GetObjectOStream(TDataFormat data_fmt,
                                                    CNetStorageObject nso,
                                                    string& key,
                                                    TCompressionFormat compression_fmt,
                                                    unsigned int time_to_live,
                                                    TNetStorageFlags default_flags)
{
    x_ValidateCompressionFormat(compression_fmt);
    x_ValidateSerialFormat(data_fmt);

    m_DataFmt = data_fmt;
    m_CmprsFmt = compression_fmt;
    m_Magic = kNC_ProjectMagic;
    auto_ptr<CNcbiOstream> ostr = x_GetOutputStream(key, time_to_live, default_flags, nso);
    return CObjectOStream::Open(m_DataFmt, *ostr.release(), eTakeOwnership);
}


string CProjectStorage::SaveRawData(const void* buf,
                                    size_t size,
                                    const string& key,
                                    unsigned int time_to_live,
                                    TNetStorageFlags default_flags)
{

    if (m_NC) {
        if (Exists(key)) {
            m_NC->PutData(key, buf, size,
                          (nc_blob_ttl=time_to_live,nc_blob_password=m_Password));
            return key;
        }

        return m_NC->PutData(buf, size,
                             (nc_blob_ttl=time_to_live, nc_blob_password=m_Password));
    } else {
        CNetStorageObject nso = Exists(key) ? m_NS.Open(key) : m_NS.Create(default_flags);
        nso.Write(buf, size);
        nso.Close();
        return nso.GetLoc();
        
    }
}


string CProjectStorage::Clone(const string& key, unsigned int time_to_live, TNetStorageFlags default_flags)
{
    if (!Exists(key))
        NCBI_THROW(CPrjStorageException, eInvalidKey, kKeyError);
        
    if (m_HasNetStorage) {
        CNetStorageObject nso = m_NS.Open(key);
        CRStream is(&nso.GetReader());
        string new_key;
        if (m_NC) {
            AutoPtr<CNcbiOstream> os = m_NC->CreateOStream(new_key);
            NcbiStreamCopyThrow(*os, is);
        } else {
            CNetStorageObject nso_clone = m_NS.Create(default_flags);
            CWStream os(&nso_clone.GetWriter());
            NcbiStreamCopyThrow(os, is);
            nso_clone.Close();
            new_key = nso_clone.GetLoc();
        }
        return new_key;
    }
    CSimpleBuffer bdata;
    CNetCacheAPI::EReadResult rres =
        m_NC->GetData(key, bdata, nc_blob_password=m_Password);
    if (rres == CNetCacheAPI::eNotFound) {
        NCBI_THROW(CPrjStorageException, eInvalidKey, kKeyError);
    }

    return m_NC->PutData(bdata.data(), bdata.size(),
                        (nc_blob_ttl=time_to_live, nc_blob_password=m_Password));
}


CIRef<IGBProject> CProjectStorage::GetProject(const string& key)
{
    auto_ptr<CObjectIStream> obj_istr = GetObjectIstream(key);
    CRef<CGBProject_ver2> real_project(new CGBProject_ver2);
    CIRef<IGBProject> proj(real_project.GetPointer());

    try {
        *obj_istr >> *real_project;
    } catch (CException& e) {
        LOG_POST(Error << "Can't deserialize the blob data as a GBProject ASN, msg: "
                 << e.GetMsg());
        throw;
    }
    return proj;
}


CProjectStorage::TAnnots CProjectStorage::GetAnnots(const string& key)
{
    TAnnots annots;
    CRef<CSerialObject> obj = GetObject(key);

    if (obj) {
        if (CGBProject_ver2* proj = dynamic_cast<CGBProject_ver2*>(obj.GetPointer())) {
            CTypeIterator<CSeq_annot> annot_iter(proj->SetData());
            for (; annot_iter; ++annot_iter) {
                annots.push_back(CRef<CSeq_annot>(&*annot_iter));
            }
        } else {
            CTypeIterator<CSeq_annot> annot_iter(*obj);
            for (; annot_iter; ++annot_iter) {
                annots.push_back(CRef<CSeq_annot>(&*annot_iter));
            }
        }
    }
    
    return annots;
}


CRef<CSerialObject> CProjectStorage::GetObject(const string& key)
{
    CRef<CSerialObject> obj;
    {
        auto_ptr<CObjectIStream> obj_istr = GetObjectIstream(key);
        
        try  {
            CRef<CGBProject_ver2> real_project(new CGBProject_ver2);
            *obj_istr >> *real_project;
            obj.Reset(real_project.GetPointer());
            return obj;
            
        } catch (CEofException& e) {
            // data type is correct, other error, give up
            LOG_POST(Warning << "NCTools::GetObjects: failed to get GB project, msg: "
                     << e.ReportAll());
            throw;
            
        } catch (CException& e) {
        }
    }

    // reinitialize the input stream
    // try it with seq-annot

    auto_ptr<CObjectIStream> obj_istr = GetObjectIstream(key);
    try {
        CRef<CSeq_annot> annot(new CSeq_annot);
        *obj_istr >> *annot;
        obj.Reset(annot.GetPointer());
        
    } catch (CEofException& ee) {
        // data type is correct, other error, give up
        LOG_POST(Warning << "NCTools::GetObjects: failed to get seq-annot, msg: "
                 << ee.GetMsg());
        throw;
        
    } catch (CException& e) {
        // unknown data, give up
        // report the earlier exception
        LOG_POST(Warning << "NCTools::GetObjects: " << e.ReportAll());
        NCBI_THROW(CPrjStorageException, eAsnObjectNotMatch, e.ReportAll() /*"Unknown ASN content"*/);
    }

    return obj;
}


void CProjectStorage::GetString(const string& key, string& str)
{
    auto_ptr<CNcbiIstream> istr = GetIstream(key);

    CConn_MemoryStream mem_str;
    NcbiStreamCopy(mem_str, *istr);
    mem_str.ToString(&str);
}


void CProjectStorage::GetVector(const string& key, vector<char>& vec)
{
    auto_ptr<CNcbiIstream> istr = GetIstream(key);
    CConn_MemoryStream mem_str;
    NcbiStreamCopy(mem_str, *istr);
    mem_str.flush();
    mem_str.ToVector(&vec);
}

auto_ptr<CNcbiIstream> CProjectStorage::GetIstream(const string& key, bool raw)
{
    if (!Exists(key)) 
        NCBI_THROW(CPrjStorageException, eInvalidKey, kKeyError);
    auto_ptr<CNcbiIstream> strm;
    CNetStorageObject nso;
    if (m_HasNetStorage) {
         nso = m_NS.Open(key);
        strm.reset(nso.GetRWStream());
    } else {
        strm.reset(m_NC->GetIStream(key, NULL, nc_blob_password=m_Password));
    }

    if (!raw) {
        /// check if the blob indeed contains a valid header
        bool valid_header = false;
        
        /// extract the header
        strm->read((char*)&m_Magic, sizeof(m_Magic));
        strm->read((char*)&m_Version, sizeof(m_Version));
        if ((m_Magic == kNC_ProjectMagic  ||  m_Magic == kNC_StringMagic)  && m_Version == kNC_CurrentVersion) {
            strm->read((char*)&m_CmprsFmt, sizeof(m_CmprsFmt));
            strm->read((char*)&m_DataFmt, sizeof(m_DataFmt));
            
            if (x_ValidateCompressionFormat(m_CmprsFmt, true)  &&
                x_ValidateSerialFormat(m_DataFmt, true)) {
                valid_header = true;

                /// decompress the data if necessary
                if (m_CmprsFmt != eNC_Uncompressed) {
                    CCompressionStreamProcessor* proc = NULL;
                    if (m_CmprsFmt == eNC_ZlibCompressed) {
                        proc = new CZipStreamDecompressor(kBufSize, kBufSize,
                                                          kWindowBits, 0);
                    } else if (m_CmprsFmt == eNC_Bzip2Compressed) {
                        proc = new CBZip2StreamDecompressor();
                    } else if (m_CmprsFmt == eNC_LzoCompressed) {
#if defined(HAVE_LIBLZO)
                        proc = new CLZOStreamDecompressor();
#else
                        // The blob is lzo-compressed, but the client
                        // code used to retrived the blob doesn't support
                        // lzo compression. Throw an exceptoion
                        NCBI_THROW(CPrjStorageException,
                                   eUnsupportedCompression,
                                   "The blob is lzo-compressed, but the client code doesn't support lzo-compression.");
#endif // HAVE_LIBLZO
                    }
                    //                    if (m_HasNetStorages) {
                    //                        auto_ptr<CNcbiIstream> zip_str(new CCompressionIStream(*(strm.release()), proc, 
                    //                                                                               CCompressionStream::fOwnStream));
            //                        strm = zip_str;
            //                    } else {
                    auto_ptr<CNcbiIstream> zip_str(new CCompressionIStream(*(strm.release()), proc, 
                                                                   CCompressionStream::fOwnAll));
                    strm = zip_str;
                        //        }
                }
            }
        }

        if ( !valid_header ) {
            // Reinitialize the input stream
            if (m_HasNetStorage) {
                strm.reset(0);
                nso = m_NS.Open(key);
                strm.reset(nso.GetRWStream());
            } else {
                strm.reset(m_NC->GetIStream(key, NULL, nc_blob_password=m_Password));
            }
            
            // reset the states
            m_Magic = kNC_ProjectMagic;
            m_Version = kNC_CurrentVersion;
            m_CmprsFmt = eNC_Uncompressed;

            // m_DataFmt is relevant only when the input stream is used 
            // for deserializing to an Asn/XML/JSON object. In this case, we assume
            // the blob stores ASN binary seq-annot for blobs with no header.
            m_DataFmt = eSerial_AsnBinary;
        }
    }

    return strm;
}


auto_ptr<CObjectIStream> CProjectStorage::GetObjectIstream(const string& key)
{
    auto_ptr<CNcbiIstream> istr = GetIstream(key);
    auto_ptr<CObjectIStream> obj_istr(CObjectIStream::Open(m_DataFmt,
                                                           *istr.release(),
                                                           eTakeOwnership));
    return obj_istr;
}



void CProjectStorage::Delete(const string& key)
{
    if (!Exists(key)) 
        NCBI_THROW(CPrjStorageException, eInvalidKey, kKeyError);
    if (m_HasNetStorage)
        m_NS.Remove(key);
    else
        m_NC->Remove(key, nc_blob_password=m_Password);
}


bool CProjectStorage::Exists(const string& key)
{
    if (key.empty())
        return false;
    try {
        if (m_HasNetStorage)
            return m_NS.Exists(key);
        return m_NC->HasBlob(key, nc_blob_password=m_Password);
    } catch (CNetCacheException& e) {
        string msg;
        if (e.GetErrCode() == CNetCacheException::eAccessDenied) {
            msg = "Can't access the blob (may be it is password-protected)";
        } else {
            msg = "Error when trying to connect to NetCache service";
        }
        LOG_POST(Info << msg << ", msg: " << e.GetMsg());
        return false;
    } catch (CException& e) {
        LOG_POST(Warning
                 << "Error connecting to the net storage service: "
                 << e.GetMsg());
        return false;
    } catch (exception& e) {
        LOG_POST(Warning
                 << "Error connecting to net storage service: "
                 << e.what());
        return false;
    }
    return true;
}


auto_ptr<CNcbiOstream> CProjectStorage::x_GetOutputStream(string& key, unsigned int time_to_live, TNetStorageFlags default_flags, CNetStorageObject nso)
{
#if !defined(HAVE_LIBLZO)
    if (m_CmprsFmt == eNC_LzoCompressed) {
        NCBI_THROW(CPrjStorageException, eUnsupportedCompression,
                   "The client code doesn't support lzo compression.");
    }
#endif // HAVE_LIBLZO
    auto_ptr<CNcbiOstream> ostr;

    if (m_NC) {
        ostr.reset(m_NC->CreateOStream(key, (nc_blob_ttl=time_to_live, nc_blob_password=m_Password)));
    } else {
        nso = Exists(key) ? m_NS.Open(key) : m_NS.Create(default_flags);
        key = nso.GetLoc();
        ostr.reset(nso.GetRWStream());
    }
    ostr->write((char*)(&m_Magic), sizeof(m_Magic));
    ostr->write((char*)(&m_Version), sizeof(m_Version));
    ostr->write((char*)(&m_CmprsFmt), sizeof(m_CmprsFmt));
    ostr->write((char*)(&m_DataFmt), sizeof(m_DataFmt));

    if (m_CmprsFmt != eNC_Uncompressed) {
        auto_ptr<CCompressionStreamProcessor> compressor;

        if (m_CmprsFmt == eNC_ZlibCompressed) {
            compressor.reset(new CZipStreamCompressor(kCompressionLevel,
                                                      kBufSize, kBufSize,
                                                      kWindowBits, kMemLevel,
                                                      kStrategy, 0));
        } else if (m_CmprsFmt == eNC_Bzip2Compressed) {
            compressor.reset(new CBZip2StreamCompressor(kCompressionLevel,
                                                        kBufSize, kBufSize));
#if defined(HAVE_LIBLZO)
        } else if (m_CmprsFmt == eNC_LzoCompressed) {
            compressor.reset(new CLZOStreamCompressor(kCompressionLevel,
                                                      kBufSize, kBufSize));
#endif // HAVE_LIBLZO
        }

        auto_ptr<CNcbiOstream> zip_ostr(
            new CCompressionOStream(*ostr.release(), compressor.release(),
                                    CCompressionStream::fOwnAll));

        return zip_ostr;
    }
    return ostr;
}


bool CProjectStorage::x_ValidateSerialFormat(TDataFormat fmt,
                                      bool no_throw) const
{
    if (fmt > eSerial_Json) {
        if (no_throw) return false;
        
        string msg = "The serialization format (";
        msg += NStr::NumericToString((int)fmt);
        msg += ") is not supported.";
        NCBI_THROW(CPrjStorageException, eUnsupportedSerialFmt, msg);
    }
    
    return true;
}


bool CProjectStorage::x_ValidateAsnSerialFormat(TDataFormat fmt,
                                         bool no_throw) const
{
    if (fmt != eSerial_AsnText  &&  fmt != eSerial_AsnBinary) {
        if (no_throw) return false;
        
        string msg = "The (de)serialization format for ASN objects must be "
            "either ASN text or binary.";
        NCBI_THROW(CPrjStorageException, eUnsupportedSerialFmt, msg);
    }
    
    return true;
}



bool CProjectStorage::x_ValidateCompressionFormat(TCompressionFormat fmt,
                                           bool no_throw) const
{
    if (fmt > eNC_LzoCompressed) {
        if (no_throw) return false;
        
        string msg = "The compression format (";
        msg += NStr::NumericToString((int)fmt);
        msg += ") is not supported.";
        NCBI_THROW(CPrjStorageException, eUnsupportedCompression, msg);
    }
    
    return true;
}



END_NCBI_SCOPE
