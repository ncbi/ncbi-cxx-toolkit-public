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
 * File Description:  Interface file for wrapping NCBI C++ Toolkit with SWIG
 *
 */


%module(docstring="Interface to the NCBI C++ Toolkit") ncbi

namespace ncbi {namespace objects {namespace validator {}} namespace blast {}}

%include exception.i
%exception {
    try {
        $action
    } catch(std::exception& e) {
        // const_cast for Perl
        SWIG_exception(SWIG_RuntimeError, const_cast<char *>(e.what()));
    } catch(...) {
        SWIG_exception(SWIG_RuntimeError, "Unknown error");
    }
}

#ifdef SWIGPERL
// Some macros defined by Perl headers that conflict with our names
%runtime %{
#undef New
#undef Stat
#undef Copy
#undef IsSet
#undef Bit
%}
#endif

%feature("autodoc");  // generate documentation for Python

%include ncbi_stl.i
%include ncbi_iostream.i

#ifdef _MSC_VER

// Windows %ignore's (temporary expedient)
%ignore ncbi::COctetStringTypeInfoGetTypeInfo;
%ignore ncbi::CAutoPointerTypeInfoGetTypeInfo;
%ignore ncbi::CSetOfTypeInfoGetTypeInfo;
%ignore ncbi::CSequenceOfTypeInfoGetTypeInfo;
%ignore ncbi::COldAsnTypeInfoGetTypeInfo;

%ignore ncbi::blast::CSeqVecSeqInfoSrc::CSeqVecSeqInfoSrc;
%ignore ncbi::objects::CCreatedFeat_Ref;
%ignore ncbi::BDB_Register_Cache;
%ignore ncbi::DBAPI_RegisterDriver_CTLIB;
%ignore ncbi::DBAPI_RegisterDriver_DBLIB;

// Trouble due to some MSVC #define of strcasecmp
%ignore ncbi::NStr::strcasecmp;

#endif  // _MSC_VER

%include ignores.i  // Various things to %ignore

// This is nasty, but SWIG cannot overload based on
// different enum types, and in this case it
// wraps the new one (feat subtype) by default.  Let's wrap the
// old one (feat type) instead.
%ignore ncbi::objects::SAnnotSelector::SAnnotSelector(TFeatSubtype);


%rename(Incr) *::operator++;
%rename(Decr) *::operator--;
#ifdef SWIGPYTHON
%rename(__nonzero__) *::operator bool;
#endif
#ifdef SWIGPERL
%rename(Bool) *::operator bool;
#endif

%{
namespace ncbi {namespace objects {namespace validator {}} namespace blast {}}
using namespace ncbi;
using namespace ncbi::objects;
using namespace ncbi::objects::validator;
using namespace ncbi::blast;
%}

%include cref.i           // CRef handling
%include cobject.i        // Typedefs, etc., for CObject
%include ncbi_cobjects.i  // A macro call for each CObject (auto-generated)

// Kind of funky.  We want our Python proxies for
// ncbi classes to inherit from a
// certain class that defines some extensions.  It would
// be nice if we could tell swig what to use as the ultimate
// base class, but it uses "object".  So we'll declare our
// own "object" in this namespace, which will inherit from
// the real "object".
#ifdef SWIGPYTHON
%pythoncode %{
class ncbi_object(object):
    pass
object = ncbi_object
%}
#endif

%include serial_manip.i        // MSerial_AsnText, etc.

%import corelib/ncbiobj.hpp    // Need CConstRef before ncbi_templates.i
%include ncbi_templates.i      // Some auto-generated %templates
%include basic_containers.i    // vectors and lists of builtins and strings


// operator[] for read access
#ifdef SWIGPYTHON
%rename (__getitem__) *::operator[];
#endif
#ifdef SWIGPERL
%rename (get) *::operator[];
#endif

%%

%include core.swig
%include general.swig
%include seq.swig
%include pub.swig
%include misc.swig
%include mmdb.swig
%include algo_ms.swig
%include algo.swig
%include data_loaders.swig
%include seqext.swig

objtools/blast_format/blastfmtutil.hpp
objtools/blast_format/showalign.hpp
objtools/blast_format/showdefline.hpp
objtools/align/alngraphic.hpp

objtools/simple/simple_om.hpp

%include pub_chem.swig
%include web.swig
%include sqlite.swig
%include bdb.swig
%include validator.swig
%include dbapi.swig
%include dbapi_driver.swig
%include lds.swig
%include image.swig

//%include gui.swig

%%

%include text_editor.i                // CTextEditor (display/editing)
%include serial_extensions.i          // (de)serialization, Spec(), etc.
%include additional_scripting_code.i  // lots of important extensions
%include version.i                    // "version" global string variable
                                      // (auto-generated)


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
