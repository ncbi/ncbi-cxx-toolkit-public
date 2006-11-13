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

// SWIG hangs on corelib/ncbimtx, so define its #include guard
#define CORELIB___NCBIMTX__HPP

// corelib/plugin_manager.hpp is a disaster due to a nested struct
// inside of a template class that inherits from something.
#define CORELIB___PLUGIN_MANAGER__HPP

// util/align_range_coll.hpp confuses SWIG
// It thinks that ncbi::CAlignRangeCollection::CAlignRangeCollException exists.
#define UTIL___ALIGN_RANGE_COLL__HPP

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

%import serial/serialbase.hpp
%template(CAliasBase_string) ncbi::CAliasBase<std::string>;
%template(CStringAliasBase_string) ncbi::CStringAliasBase<std::string>;
%template(CAliasBase_vector_char) ncbi::CAliasBase<std::vector<char> >;
%template(CStringAliasBase_vector_char)
    ncbi::CStringAliasBase<std::vector<char> >;
%template(CAliasBase_int) ncbi::CAliasBase<int>;
%template(CStdAliasBase_int) ncbi::CStdAliasBase<int>;

// Since the introduction of NCBI_EXCEPTION_VAR_EX, instances of NCBI_THROW
// in headers make SWIG preprocessor choke, so #define it to nothing.
%include corelib/ncbiexpt.hpp
#undef NCBI_THROW
#define NCBI_THROW(foo, bar, baz)

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

corelib/ddumpable.hpp
corelib/env_reg.hpp
corelib/metareg.hpp
corelib/ncbi_bswap.hpp
corelib/ncbi_limits.hpp
//corelib/ncbi_os_mswin.hpp
corelib/ncbi_process.hpp
corelib/ncbi_safe_static.hpp
corelib/ncbi_stack.hpp
corelib/ncbi_system.hpp
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
corelib/ncbistre.hpp
corelib/ncbithr.hpp
corelib/ncbithr_conf.hpp
corelib/ncbitime.hpp
corelib/ncbiutil.hpp
corelib/plugin_manager.hpp
corelib/plugin_manager_impl.hpp
corelib/reader_writer.hpp
corelib/rwstream.hpp
corelib/syslog.hpp
corelib/version.hpp

util/align_range.hpp
util/align_range_coll.hpp
util/arg_regexp.hpp
util/compress/bzip2.hpp
util/compress/compress.hpp
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
util/format_guess.hpp
// #include's some stuff in util/bitset that leads to problems
//util/id_mux.hpp
util/itree.hpp
util/lightstr.hpp
util/line_reader.hpp
util/linkedset.hpp
util/logrotate.hpp
util/md5.hpp
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
util/rwstream.hpp
util/sgml_entity.hpp
util/smalldns.hpp
util/static_map.hpp
util/static_set.hpp
util/strbuffer.hpp
util/stream_utils.hpp
util/strsearch.hpp
util/thread_nonstop.hpp
util/thread_pool.hpp
util/transmissionrw.hpp
util/unicode.hpp

// util/unicode.hpp and util/utf8.hpp both define ncbi::utf8::eSkip
//util/unicode.hpp
util/utf8.hpp
util/util_exception.hpp
util/weakmap.hpp
util/cache/icache.hpp
util/dictionary.hpp
util/dictionary_util.hpp
// weakmap.inl won't compile
//util/weakmap.hpp

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
connect/services/blob_storage_netcache.hpp
connect/services/grid_client.hpp
connect/services/grid_client_app.hpp
connect/services/grid_control_thread.hpp
connect/services/grid_debug_context.hpp
// Deprecated, causes trouble
//connect/services/grid_default_factories.hpp
connect/services/grid_worker.hpp
connect/services/grid_worker_app.hpp
connect/services/grid_worker_app_impl.hpp
connect/services/netcache_client.hpp
connect/services/neticache_client.hpp
connect/services/netschedule_client.hpp
connect/services/netschedule_storage.hpp
connect/services/netservice_client.hpp
connect/services/ns_client_factory.hpp
connect/threaded_server.hpp

// these must come early to avoid %include within %import
serial/pathhook.hpp
serial/item.hpp
serial/memberid.hpp

// need this for CClassTypeInfoBase::CIteratorb base class
serial/impl/memberlist.hpp

serial/aliasinfo.hpp
serial/asntypes.hpp
serial/autoptrinfo.hpp
serial/choice.hpp
serial/choiceptr.hpp
serial/classinfo.hpp
serial/classinfob.hpp
serial/continfo.hpp
serial/delaybuf.hpp
serial/enumerated.hpp
serial/enumvalues.hpp
serial/exception.hpp
serial/iterator.hpp
serial/member.hpp
serial/objcopy.hpp
serial/objectinfo.hpp
serial/objectio.hpp
serial/objectiter.hpp
serial/objecttype.hpp
serial/objhook.hpp
serial/objistr.hpp
serial/objistrasn.hpp
serial/objistrasnb.hpp
serial/objistrxml.hpp
serial/objostr.hpp
serial/objostrasn.hpp
serial/objostrasnb.hpp
serial/objostrxml.hpp
serial/pack_string.hpp
serial/ptrinfo.hpp
serial/rpcbase.hpp
serial/serial.hpp
serial/serialasn.hpp
serial/serialbase.hpp
serial/serialdef.hpp
serial/serialimpl.hpp
serial/serializable.hpp
serial/serialutil.hpp
serial/stdtypes.hpp
serial/stdtypesimpl.hpp
serial/stltypes.hpp
serial/stltypesimpl.hpp
serial/timetypeinfo.hpp
serial/typeinfo.hpp
// looks dead; no .cpp, destructor not defined
//serial/typemapper.hpp
serial/typeref.hpp
serial/variant.hpp

// No export specifiers
%ifndef WINDOWS serial/soap/soap_client.hpp
%ifndef WINDOWS serial/soap/soap_code.hpp
%ifndef WINDOWS serial/soap/soap_detail.hpp
%ifndef WINDOWS serial/soap/soap_fault.hpp
%ifndef WINDOWS serial/soap/soap_message.hpp
%ifndef WINDOWS serial/soap/soap_reason.hpp
%ifndef WINDOWS serial/soap/soap_server.hpp
%ifndef WINDOWS serial/soap/soap_subcode.hpp
%ifndef WINDOWS serial/soap/soap_text.hpp

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


/*
 * ===========================================================================
 * $Log$
 * Revision 1.24  2006/11/13 15:14:02  jcherry
 * Removed deleted connect/ncbi_service_misc.h
 *
 * Revision 1.23  2006/11/09 21:04:35  jcherry
 * Added corelib/ncbi_stack.hpp
 *
 * Revision 1.22  2006/10/19 17:52:09  jcherry
 * Added connect/*.h
 *
 * Revision 1.21  2006/10/17 16:29:45  jcherry
 * Reflect movement of more headers from serial to serial/impl
 *
 * Revision 1.20  2006/10/12 16:00:52  jcherry
 * Dropped headers that have been moved from serial to serial/impl
 *
 * Revision 1.19  2006/08/17 20:22:47  jcherry
 * Added some headers from util/
 *
 * Revision 1.18  2006/07/14 14:14:42  jcherry
 * Work-around for strange problem involving NCBI_EXCEPTION_VAR_EX
 *
 * Revision 1.17  2006/06/12 14:01:14  jcherry
 * Added header
 *
 * Revision 1.16  2006/03/14 17:00:03  jcherry
 * Work-around for CObjectIStream::Open
 *
 * Revision 1.15  2006/03/01 16:27:15  jcherry
 * Adjusted header list
 *
 * Revision 1.14  2006/02/16 17:27:08  jcherry
 * Reflect header rearrangement
 *
 * Revision 1.13  2006/01/03 14:38:02  jcherry
 * Added headers
 *
 * Revision 1.12  2005/12/15 20:31:24  jcherry
 * Added headers
 *
 * Revision 1.11  2005/12/13 22:50:28  jcherry
 * Added headers
 *
 * Revision 1.10  2005/08/16 15:18:46  jcherry
 * Hide util/align_range_coll.hpp from SWIG
 *
 * Revision 1.9  2005/08/03 14:57:10  jcherry
 * Added headers
 *
 * Revision 1.8  2005/07/27 13:12:11  jcherry
 * Hid corelib/plugin_manager.hpp from SWIG
 *
 * Revision 1.7  2005/07/01 14:12:58  jcherry
 * Eliminate the need for dummy headers
 *
 * Revision 1.6  2005/06/29 18:14:29  jcherry
 * Moved CConstRef (needs to be early)
 *
 * Revision 1.5  2005/06/27 17:01:31  jcherry
 * Provide a simple declaration for CConstRef (the new real one confuses SWIG)
 *
 * Revision 1.4  2005/05/24 18:34:04  jcherry
 * Added util/compress/tar.hpp
 *
 * Revision 1.3  2005/05/18 18:15:57  jcherry
 * Made CAliasBase stuff work
 *
 * Revision 1.2  2005/05/16 18:11:28  jcherry
 * Guard against passing an object of a different type to
 * CSerialObject::Assign and CSerialObject::Equals
 *
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
