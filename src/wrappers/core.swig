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


%%

corelib/ddumpable.hpp
corelib/metareg.hpp
corelib/ncbi_bswap.hpp
corelib/ncbi_limits.hpp
//corelib/ncbi_os_mswin.hpp
corelib/ncbi_process.hpp
corelib/ncbi_safe_static.hpp
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
corelib/version.hpp
util/compress/bzip2.hpp
util/compress/compress.hpp
util/compress/reader_zlib.hpp
util/compress/stream.hpp
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
util/itree.hpp
util/lightstr.hpp
util/linkedset.hpp
util/logrotate.hpp
util/md5.hpp
util/random_gen.hpp
util/range.hpp
util/range_coll.hpp
util/rangemap.hpp
util/reader_writer.hpp
util/regexp.hpp
util/resize_iter.hpp
util/resource_pool.hpp
util/rwstream.hpp
util/rwstreambuf.hpp
util/smalldns.hpp
util/static_map.hpp
util/static_set.hpp
util/strbuffer.hpp
util/stream_utils.hpp
util/strsearch.hpp
util/thread_pool.hpp
util/utf8.hpp
util/util_exception.hpp
util/cache/icache.hpp
util/dictionary.hpp
// weakmap.inl won't compile
//util/weakmap.hpp
connect/email_diag_handler.hpp
connect/ncbi_conn_exception.hpp
connect/ncbi_conn_stream.hpp
connect/ncbi_core_cxx.hpp
connect/ncbi_namedpipe.hpp
connect/ncbi_namedpipe_connector.hpp
connect/ncbi_pipe.hpp
connect/ncbi_pipe_connector.hpp
connect/ncbi_socket.hpp
connect/threaded_server.hpp
serial/aliasinfo.hpp
serial/asntypes.hpp
serial/autoptrinfo.hpp
serial/choice.hpp
serial/choiceptr.hpp
serial/classinfo.hpp
serial/classinfob.hpp
serial/classinfohelper.hpp
serial/continfo.hpp
serial/delaybuf.hpp
serial/enumerated.hpp
serial/enumvalues.hpp
serial/exception.hpp
serial/hookdata.hpp
serial/hookdataimpl.hpp
serial/hookdatakey.hpp
serial/hookfunc.hpp
serial/item.hpp
serial/iterator.hpp
serial/iteratorbase.hpp
serial/member.hpp
serial/memberid.hpp
serial/memberlist.hpp
serial/objcopy.hpp
serial/object.hpp
serial/objectinfo.hpp
serial/objectio.hpp
serial/objectiter.hpp
serial/objecttype.hpp
serial/objhook.hpp
serial/objistr.hpp
serial/objistrasn.hpp
serial/objistrasnb.hpp
serial/objistrimpl.hpp
serial/objistrxml.hpp
serial/objlist.hpp
serial/objostr.hpp
serial/objostrasn.hpp
serial/objostrasnb.hpp
serial/objostrxml.hpp
serial/objstack.hpp
serial/objstrasnb.hpp
serial/objstrb.hpp
serial/pack_string.hpp
serial/pathhook.hpp
serial/ptrinfo.hpp
serial/rpcbase.hpp
serial/serial.hpp
serial/serialasn.hpp
serial/serialasndef.hpp
serial/serialbase.hpp
serial/serialdef.hpp
serial/serialimpl.hpp
serial/serializable.hpp
serial/serialutil.hpp
serial/stdtypeinfo.hpp
serial/stdtypes.hpp
serial/stdtypesimpl.hpp
serial/stltypes.hpp
serial/stltypesimpl.hpp
serial/timetypeinfo.hpp
serial/typeinfo.hpp
serial/typeinfoimpl.hpp
serial/typemap.hpp
serial/typemapimpl.hpp
// looks dead; no .cpp, destructor not defined
//serial/typemapper.hpp
serial/typeref.hpp
serial/variant.hpp

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

/*
 * ===========================================================================
 * $Log$
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
