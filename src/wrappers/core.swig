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
 * Authors:  Josh Cherry
 *
 * File Description:  SWIG interface file for core
 *
 */

// See ncbi_modify.py for %template insertion for CTypeIterator superclasses
// and handling of certain problematic CTypeIterator ctors

// CTreeIteratorTmpl::TThis is private, but return type of operator++
%extend ncbi::CTreeIteratorTmpl {
    CTreeIteratorTmpl<LevelIterator>& Incr(void) {
        return ++(*self);
    }
}
%ignore ncbi::CTreeIteratorTmpl::operator++;

// SWIG mishandles template argument defaults for
// bvector forward declaration in util/bitset/bmfwd.h
#define BMFWD__H__INCLUDED__   

// SWIG hangs on corelib/ncbimtx, so define its #include guard
#define CORELIB___NCBIMTX__HPP

// corelib/plugin_manager.hpp is a disaster due to a nested struct
// inside of a template class that inherits from something.
#define CORELIB___PLUGIN_MANAGER__HPP

// Since the introduction of NCBI_EXCEPTION_VAR_EX, instances of NCBI_THROW
// in headers make SWIG preprocessor choke, so #define it to nothing.
%include corelib/ncbiexpt.hpp
#undef NCBI_THROW
#define NCBI_THROW(foo, bar, baz)

%template(vector_CSerialAttribInfoItem)
    std::vector<ncbi::CSerialAttribInfoItem>;

%varargs(10, char *arg = NULL) ncbi::CExec::SpawnLP;

// CSerialObject::Assign and CSerialObject::Equals yield fatal errors
// if the types don't match; generate custom wrappers that guard
// against this and throw an exception instead

// IsSameTypeInfo copied from serialobject.cpp
%{
static bool IsSameTypeInfo( const CSerialObject& obj1,
                            const CSerialObject& obj2 )
{
    TTypeInfo type1 = obj1.GetThisTypeInfo();
    while (type1->GetTypeFamily() == eTypeFamilyPointer) {
        const CPointerTypeInfo* t = dynamic_cast<const CPointerTypeInfo*>(type1);
        type1 = t->GetPointedType();
    }
    TTypeInfo type2 = obj2.GetThisTypeInfo();
    while (type2->GetTypeFamily() == eTypeFamilyPointer) {
        const CPointerTypeInfo* t = dynamic_cast<const CPointerTypeInfo*>(type2);
        type2 = t->GetPointedType();
    }
    return (type1 == type2);
}
%}

%extend ncbi::CSerialObject {
    void Assign(const ncbi::CSerialObject& source,
                ncbi::ESerialRecursionMode how=ncbi::eRecursive) {
        if (typeid(source) != typeid(*self)
            && !IsSameTypeInfo(source, *self) ) {
            string s = "Can't assign " + source.GetThisTypeInfo()->GetName()
                       + " to " + self->GetThisTypeInfo()->GetName();
            throw runtime_error(s);
        }
        self->Assign(source, how);
    }
    bool Equals(const ncbi::CSerialObject& source,
                ncbi::ESerialRecursionMode how=ncbi::eRecursive) const {
        if (typeid(source) != typeid(*self)
            && !IsSameTypeInfo(source, *self) ) {
            string s = "Can't compare " + self->GetThisTypeInfo()->GetName()
                       + " to " + source.GetThisTypeInfo()->GetName();
            throw runtime_error(s);
        }
        return self->Equals(source, how);
    }
}
%ignore ncbi::CSerialObject::Assign;
%ignore ncbi::CSerialObject::Equals;

%newobject ncbi::CObjectIStream::Open;
%newobject ncbi::CObjectIStream::Create;
%newobject ncbi::CObjectIStream::CreateFromBuffer;
%newobject ncbi::CObjectOStream::Open;

%import serial/serialbase.hpp
%template(CAliasBase_string) ncbi::CAliasBase<std::string>;
%template(CStringAliasBase_string) ncbi::CStringAliasBase<std::string>;
%template(CAliasBase_vector_char) ncbi::CAliasBase<std::vector<char> >;
%template(CStringAliasBase_vector_char)
    ncbi::CStringAliasBase<std::vector<char> >;
%template(CAliasBase_int) ncbi::CAliasBase<int>;
%template(CStdAliasBase_int) ncbi::CStdAliasBase<int>;

// Avoid error:
//    ISO C++ forbids casting to an array type
//    `ncbi::utf8::SUnicodeTranslation (* const*[256])[256]'
%ignore *::UnicodeToAscii;
%ignore *::UTF8ToAscii;
%ignore *::UTF8ToAsciiString;

// Python keywords used as variable names in connect/*.h
#ifdef SWIGPYTHON
%rename(pass_) pass;  // connect/ncbi_connutil.h
%rename(from_) from;  // connect/ncbi_sendmail.h
#endif

%%

corelib/ncbicfg.h
corelib/ncbi_limits.h

corelib/blob_storage.hpp
corelib/ddumpable.hpp
corelib/env_reg.hpp
corelib/error_codes.hpp
corelib/guard.hpp
corelib/metareg.hpp
corelib/ncbi_bswap.hpp
corelib/ncbi_limits.hpp
corelib/ncbi_mask.hpp
//corelib/ncbi_os_mswin.hpp
corelib/ncbi_param.hpp
corelib/ncbi_process.hpp
corelib/ncbi_safe_static.hpp
corelib/ncbi_signal.hpp
corelib/ncbi_stack.hpp
corelib/ncbi_system.hpp
//corelib/ncbi_tree.hpp  // just templates
corelib/ncbi_xstr.hpp
corelib/ncbiapp.hpp
corelib/ncbiargs.hpp
corelib/ncbiatomic.hpp
corelib/ncbicntr.hpp
corelib/ncbidbg.hpp
corelib/ncbidiag.hpp
// problems for Perl?
corelib/ncbidll.hpp
corelib/ncbienv.hpp
corelib/ncbiexec.hpp
corelib/ncbiexpt.hpp
corelib/ncbifile.hpp
corelib/ncbimempool.hpp
corelib/ncbimisc.hpp
corelib/ncbimtx.hpp
corelib/ncbiobj.hpp
corelib/ncbireg.hpp
corelib/ncbistd.hpp
corelib/ncbistl.hpp
corelib/ncbistr.hpp
corelib/ncbistr_util.hpp
corelib/ncbistre.hpp
corelib/ncbithr.hpp
corelib/ncbithr_conf.hpp
corelib/ncbitime.hpp
corelib/ncbiutil.hpp
corelib/obj_store.hpp
corelib/plugin_manager.hpp
corelib/plugin_manager_impl.hpp
//corelib/plugin_manager_store.hpp  // nested struct in template class
corelib/reader_writer.hpp
corelib/rwstream.hpp
corelib/stream_utils.hpp
corelib/syslog.hpp
corelib/tempstr.hpp
corelib/version.hpp

util/error_codes.hpp

util/align_range.hpp
util/align_range_coll.hpp
util/align_range_oper.hpp
util/arg_regexp.hpp
util/compress/bzip2.hpp
util/compress/compress.hpp
util/compress/lzo.hpp
util/compress/reader_zlib.hpp
util/compress/stream.hpp
util/compress/tar.hpp
util/compress/zlib.hpp
util/sequtil/sequtil.hpp
util/sequtil/sequtil_convert.hpp
util/sequtil/sequtil_expt.hpp
util/sequtil/sequtil_manip.hpp
util/math/matrix.hpp
util/math/promote.hpp
util/ascii85.hpp
util/bytesrc.hpp
util/checksum.hpp
util/ddump_viewer.hpp
util/file_obsolete.hpp
util/fileblobstorage/blobstorage_file.hpp
util/format_guess.hpp
// #include's some stuff in util/bitset that leads to problems
//util/id_mux.hpp
util/itransaction.hpp
util/itree.hpp
//util/lightstr.hpp  // deprecated header
util/line_reader.hpp
util/linkedset.hpp
util/lock_vector.hpp
util/logrotate.hpp
util/mask_regexp.hpp
util/md5.hpp
util/multi_writer.hpp
util/mutex_pool.hpp
util/ncbi_cache.hpp
util/ncbi_table.hpp
util/random_gen.hpp
util/range.hpp
util/range_coll.hpp
util/rangemap.hpp
util/reader_writer.hpp
util/regexp.hpp
util/request_control.hpp
util/resize_iter.hpp
util/resource_pool.hpp
util/scheduler.hpp
util/sgml_entity.hpp
util/simple_buffer.hpp
util/smalldns.hpp
util/static_map.hpp
util/static_set.hpp
util/strbuffer.hpp
util/strsearch.hpp
util/thread_nonstop.hpp
util/thread_pool.hpp
util/thread_pool_ctrl.hpp
util/time_line.hpp
util/transmissionrw.hpp
util/unicode.hpp
// util/unicode.hpp and util/utf8.hpp both define ncbi::utf8::eSkip
//util/unicode.hpp
util/utf8.hpp
util/util_exception.hpp
util/uttp.hpp
util/weakmap.hpp
util/cache/cache_ref.hpp
util/cache/icache.hpp
util/cache/icache_cf.hpp
util/cache/icache_clean_thread.hpp
util/dictionary.hpp
util/dictionary_util.hpp
util/qparse/query_exec.hpp
util/qparse/query_exec_bv.hpp
util/qparse/query_parse.hpp
// weakmap.inl won't compile
//util/weakmap.hpp
util/tables/raw_scoremat.h

connect/error_codes.hpp

connect/connect_export.h
connect/ncbi_buffer.h
connect/ncbi_connection.h
connect/ncbi_connector.h
connect/ncbi_connutil.h
connect/ncbi_core.h
connect/ncbi_file_connector.h
connect/ncbi_ftp_connector.h
connect/ncbi_heapmgr.h
connect/ncbi_host_info.h
connect/ncbi_http_connector.h
connect/ncbi_memory_connector.h
connect/ncbi_sendmail.h
connect/ncbi_server_info.h
connect/ncbi_service.h
connect/ncbi_service_connector.h
connect/ncbi_socket.h
connect/ncbi_socket_connector.h
connect/ncbi_socket_unix.h
connect/ncbi_types.h
connect/ncbi_util.h

connect/email_diag_handler.hpp
connect/ncbi_conn_exception.hpp
connect/ncbi_conn_reader_writer.hpp
connect/ncbi_conn_stream.hpp
connect/ncbi_core_cxx.hpp
connect/ncbi_namedpipe.hpp
connect/ncbi_namedpipe_connector.hpp
connect/ncbi_pipe.hpp
connect/ncbi_pipe_connector.hpp
connect/ncbi_socket.hpp
connect/ncbi_socket_unix.hpp
connect/server.hpp
connect/services/blob_storage_netcache.hpp
connect/services/error_codes.hpp
connect/services/grid_client.hpp
connect/services/grid_client_app.hpp
connect/services/grid_control_thread.hpp
connect/services/grid_debug_context.hpp
// Deprecated, causes trouble
//connect/services/grid_default_factories.hpp
connect/services/grid_worker.hpp
connect/services/grid_worker_app.hpp
connect/services/grid_worker_app_impl.hpp
connect/services/netcache_api.hpp
connect/services/netcache_api_expt.hpp
connect/services/netcache_client.hpp
connect/services/neticache_client.hpp
connect/services/netcache_key.hpp
connect/services/netcache_rw.hpp
connect/services/netschedule_api.hpp
connect/services/netschedule_api_const.hpp
connect/services/netschedule_api_expt.hpp
connect/services/netschedule_key.hpp
connect/services/netservice_api_expt.hpp
connect/services/netschedule_client.hpp
connect/services/netschedule_storage.hpp
connect/services/netservice_api.hpp
connect/services/netservice_client.hpp
connect/services/ns_client_factory.hpp
connect/services/srv_discovery.hpp
connect/services/srv_connections.hpp
connect/threaded_server.hpp

serial/impl/memberlist.hpp
// need this for CClassTypeInfoBase::CIteratorb base class
serial/impl/classinfob.hpp
serial/delaybuf.hpp
serial/enumvalues.hpp
serial/error_codes.hpp
serial/exception.hpp
// before iterator.hpp to avoid %include within %import
serial/objectinfo.hpp
serial/iterator.hpp
serial/objcopy.hpp
serial/objectio.hpp
serial/objectiter.hpp
serial/objhook.hpp
serial/objistr.hpp
serial/objistrasn.hpp
serial/objistrasnb.hpp
serial/objistrxml.hpp
serial/objostr.hpp
serial/objostrasn.hpp
serial/objostrasnb.hpp
serial/objostrjson.hpp
serial/objostrxml.hpp
serial/pack_string.hpp
serial/rpcbase.hpp
serial/serial.hpp
serial/serialasn.hpp
serial/serialbase.hpp
serial/serialdef.hpp
serial/serialimpl.hpp
serial/serializable.hpp
serial/serialutil.hpp
serial/typeinfo.hpp

// No export specifiers
// need this because soap_message.hpp #include's soap_11__.hpp
%ifndef WINDOWS serial/soap/soap_11__.hpp
%ifndef WINDOWS serial/soap/soap_11_module.hpp
%ifndef WINDOWS serial/soap/soap_body.hpp
%ifndef WINDOWS serial/soap/soap_body_.hpp
%ifndef WINDOWS serial/soap/soap_client.hpp
%ifndef WINDOWS serial/soap/soap_detail.hpp
%ifndef WINDOWS serial/soap/soap_detail_.hpp
%ifndef WINDOWS serial/soap/soap_envelope.hpp
%ifndef WINDOWS serial/soap/soap_envelope_.hpp
%ifndef WINDOWS serial/soap/soap_fault.hpp
%ifndef WINDOWS serial/soap/soap_fault_.hpp
%ifndef WINDOWS serial/soap/soap_header.hpp
%ifndef WINDOWS serial/soap/soap_header_.hpp
%ifndef WINDOWS serial/soap/soap_message.hpp
%ifndef WINDOWS serial/soap/soap_server.hpp

%%

%template(CNcbiMatrix_double) ncbi::CNcbiMatrix<double>;

// dynamic casting for type info
%extend ncbi::CClassTypeInfo {
public:
    static ncbi::CClassTypeInfo* __dynamic_cast_to__(ncbi::CTypeInfo *obj) {
        return dynamic_cast<ncbi::CClassTypeInfo*>(obj);
    }
}
%extend ncbi::CChoiceTypeInfo {
public:
    static ncbi::CChoiceTypeInfo* __dynamic_cast_to__(ncbi::CTypeInfo *obj) {
        return dynamic_cast<ncbi::CChoiceTypeInfo*>(obj);
    }
}
%extend ncbi::CSequenceOfTypeInfo {
public:
    static ncbi::CSequenceOfTypeInfo* __dynamic_cast_to__(ncbi::CTypeInfo *obj) {
        return dynamic_cast<ncbi::CSequenceOfTypeInfo*>(obj);
    }
}
%extend ncbi::CPrimitiveTypeInfo {
public:
    static ncbi::CPrimitiveTypeInfo* __dynamic_cast_to__(ncbi::CTypeInfo *obj) {
        return dynamic_cast<ncbi::CPrimitiveTypeInfo*>(obj);
    }
}

%template(COpenRange_TSeqPos) ncbi::COpenRange<ncbi::TSeqPos>;
%template(CRange_TSeqPos) ncbi::CRange<ncbi::TSeqPos>;
%template(COpenRange_TSignedSeqPos) ncbi::COpenRange<ncbi::TSignedSeqPos>;
%template(CRange_TSignedSeqPos) ncbi::CRange<ncbi::TSignedSeqPos>;
%template(TAlnRng) ncbi::CAlignRange<ncbi::TSignedSeqPos>;
%template(TAlnRngColl)  ncbi::CAlignRangeCollection<ncbi::CAlignRange<int> >;
                      //  Using TSignedSeqPos here makes SWIG crash   ^^^


// An ugly semi-fix.  With SWIG/Python, if there are static and
// non-static signatures of the same method name, only
// static or non-static are usable, depending on declaration order.
// This gets us the static ones.
#ifdef SWIGPYTHON
%extend ncbi::CObjectIStream {
%pythoncode %{
    Open = staticmethod(Open)
%}
}
#endif


// for corelib/ncbi_stack.hpp
%template(list_CStackTrace_SStackFrameInfo)
    std::list<ncbi::CStackTrace::SStackFrameInfo>;

// for CNetCacheClient::SBlobData data member
%template(AutoPtr_unsigned_char_ArrayDeleter_unsigned_char)
ncbi::AutoPtr<unsigned char, ncbi::ArrayDeleter<unsigned char> >;

%inline %{

BEGIN_NCBI_SCOPE

// A concrete subclass of CNcbiApplication
// whose Run() method does nothing
class CDoNothingApp : public CNcbiApplication
{
public:
    int Run() {return 0;}
    using CNcbiApplication::AppMain;
    int AppMain(const vector<string>& args,
                EAppDiagStream diag = eDS_Default,
                string conf = "", string name = "")
    {
        vector<const char*> argv(args.size());
        for (unsigned int i = 0; i < args.size(); ++i) {
            argv[i] = args[i].c_str();
        }
        return AppMain(args.size(), &(argv[0]), 0, diag,
                       conf.c_str(), name.c_str());
    }
};

END_NCBI_SCOPE

%}
