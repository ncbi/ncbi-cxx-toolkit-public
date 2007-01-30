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


#ifdef SWIGRUBY
// int vs. enum for ios_base::fmtflags
%ignore std::ios_base::setf;
%ignore std::ios_base::unsetf;
%ignore std::ios::clear;
%ignore std::ios::setstate;
%ignore std::ios::exceptions;
// and ios_base::openmode
%ignore std::ifstream::ifstream(const char *file_name, openmode flags);
%ignore std::ifstream::open(const char *file_name, openmode flags);
%ignore std::ofstream::ofstream(const char *file_name, openmode flags);
%ignore std::ofstream::open(const char *file_name, openmode flags);
%ignore *::CRotatingLogStreamBuf::CRotatingLogStreamBuf;

// constants with names starting with '_' are forbidden
%ignore _CPP_BACKWARD_BACKWARD_WARNING_H;
%ignore _BACKWARD_BACKWARD_WARNING_H;

%ignore *::CSoapServerApplication;  // ??? causes crash in require
%ignore *::CBlobStorage_NetCache;
%ignore *::CNetCacheStorageException;
%ignore *::CBlobStorageFactory_NetCache;
%ignore *::CSeq_entry_ScopeInfo;
%ignore *::CBioseq_set_ScopeInfo;
%ignore *::CSeq_annot_ScopeInfo;
%ignore *::CGBReaderCacheManager;
%ignore *::CId1Reader;
%ignore *::CPubseqReader;
%ignore *::CBioseq_ScopeInfo;
#endif


namespace ncbi {namespace objects {namespace validator {}} namespace blast {}}

%immutable st;
%inline %{
    string st;  // stack trace from CException
%}

%include exception.i
%{
// helper function for exception catching
#include <iostream>
#include <sstream>
#include <corelib/ncbi_stack.hpp>
void HandleException(void) {
    try {
        throw;
    } catch(ncbi::CException& e) {
#ifdef SWIGPYTHON
        const ncbi::CStackTrace* cst = e.GetStackTrace();
        if (cst) {
            ostringstream ostr;
            cst->Write(ostr);
            st = ostr.str();
        } else {
            st.erase();
        }
#endif
        // const_cast for Perl
        SWIG_exception(SWIG_RuntimeError, const_cast<char *>(e.what()));
    } catch(std::exception& e) {
        // const_cast for Perl
        SWIG_exception(SWIG_RuntimeError, const_cast<char *>(e.what()));
    } catch(...) {
        SWIG_exception(SWIG_RuntimeError, "Unknown error");
    }
    fail:
        return;
}
%}
%exception {
    try {
        $action
    } catch(...) {
        HandleException();
        goto fail;
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

// NCBI_DEPRECATED expands to gcc-specific "__attribute__(something)" on linux
#define __attribute__(x)

// Similarly for __declspec on Windows
#define __declspec(x)

%feature("autodoc");  // generate documentation for Python

%include ncbi_stl.i
%include ncbi_iostream.i

#ifdef SWIGPYTHON
%include std_common.i
%include std_set.i
#endif

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

// Need to hide auto_ptr declaration in ncbimisc.hpp from SWIG
// due to nested template mess
%import corelib/config/ncbiconf_msvc.h
#undef HAVE_NO_AUTO_PTR

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

// SWIG is confused by recent CConstRef changes
namespace ncbi {
    template<class T> class CConstRef
    {
    public:
        CConstRef();
        CConstRef(const T*);
        const T* operator->();
        const T& operator*();
    };
}
%ignore ncbi::CConstRef;

%include cref.i           // CRef handling
%include cobject.i        // Typemaps, etc., for CObject
%include ncbi_cobjects.i  // A macro call for each CObject (auto-generated)

#define ASNCALL  // needed on Windows for serialasndef.hpp

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

%include basic_containers.i    // vectors and lists of builtins and strings
// Make NStr::StringTo* work with std::string and scripting lang. strings
#ifdef SWIGPYTHON
    %typemap(in) const ncbi::CTempString & (ncbi::CTempString temp, ncbi::CTempString *s) {
        if (PyString_Check($input)) {
            temp = ncbi::CTempString(PyString_AsString($input),
                                     PyString_Size($input));
            $1 = &temp;
        } else if (SWIG_ConvertPtr($input, (void **) &s,
                                   $1_descriptor, 0) != -1) {
            $1 = s;
        } else if (CheckPtrForbidNull($input, $descriptor(std::string *))) {
            std::string *std_string;
            SWIG_ConvertPtr($input, (void **) &std_string,
                            $descriptor(std::string *), 0);
            temp = ncbi::CTempString(*std_string);
            $1 = &temp;
        } else if (CheckPtrForbidNull($input, $descriptor(char *))) {
            char *char_p;
            SWIG_ConvertPtr($input, (void **) &char_p,
                            $descriptor(char *), 0);
            temp = ncbi::CTempString(char_p);
            $1 = &temp;
        } else {
            SWIG_exception(SWIG_TypeError, "string expected");
        }
    }

    %typecheck(SWIG_TYPECHECK_STRING) const ncbi::CTempString & {
        if (CheckPtrForbidNull($input, $1_descriptor)) {
            $1 = 1;
        } else if (PyString_Check($input)) {
            $1 = 1;
        } else if (CheckPtrForbidNull($input, $descriptor(std::string *))) {
            $1 = 1;
        } else if (CheckPtrForbidNull($input, $descriptor(char *))) {
            $1 = 1;
        } else {
            $1 = 0;
        }
    }
#endif
%import corelib/ncbiobj.hpp    // Need CConstRef before ncbi_templates.i
%include ncbi_templates.i      // Some auto-generated %templates


// operator[] for read access
#ifdef SWIGPYTHON
%rename (__getitem__) *::operator[];
#endif
#ifdef SWIGPERL
%rename (get) *::operator[];
#endif

# Function to break into debugger
#ifndef _MSC_VER
%inline %{

void debugger(void) {
    asm("int $3");
}

%}
#endif

%%

%include core.swig
%include general.swig
%include seq.swig
%include pub.swig
%include misc.swig
%ifndef WINDOWS %include mmdb.swig
%ifndef WINDOWS %include algo_ms.swig
%include algo.swig
%include data_loaders.swig
%include seqext.swig
%include xblastformat.swig

%ifndef WINDOWS objtools/align/alngraphic.hpp

objtools/simple/simple_om.hpp

%ifndef WINDOWS %include pub_chem.swig
%include web.swig
%include sqlite.swig
%include bdb.swig
%include validator.swig
%include dbapi.swig
%include dbapi_driver.swig
%include lds.swig
%include image.swig

%include gui.swig

%%

%include text_editor.i                // CTextEditor (display/editing)
%include serial_extensions.i          // (de)serialization, Spec(), etc.
%include additional_scripting_code.i  // lots of important extensions
%include version.i                    // "version" global string variable
                                      // (auto-generated)
#ifdef SWIGRUBY
%init %{

    rb_eval_string("\
class Ncbi::CSerialObject\n\
    def TextView()\n\
        Ncbi::CTextEditor::DisplayText(self.Asn())\n\
    end\n\
end\n\
");

%}
#endif

/*
 * ===========================================================================
 * $Log$
 * Revision 1.11  2006/11/17 19:37:52  jcherry
 * Adjust for changed exception/stack trace API
 *
 * Revision 1.10  2006/11/16 23:29:46  jcherry
 * Experimental exception stack trace capture for Python
 *
 * Revision 1.9  2006/11/02 22:15:07  jcherry
 * Exception handling: catch only (...) and handle in a function (reduces
 * library size by 3-4 Mb).  Deal with fact that NStr methods now take
 * CTempString.  Added debugger() function for intel linux.  Some use of
 * standard stl wrapping for Python.  Ruby stuff.
 *
 * Revision 1.8  2005/12/29 17:50:04  jcherry
 * Move blast formatting stuff to its own file and wrap on all platforms.
 * Fix for auto_ptr nested templates problem on Windows.
 *
 * Revision 1.7  2005/11/22 15:20:09  jcherry
 * #define __declspec(x) to nothing
 *
 * Revision 1.6  2005/11/10 19:10:53  jcherry
 * #define __attribute__(x) to nothing (for NCBI_DEPRECATED).
 * Conditionally wrap some additional blast format headers.
 *
 * Revision 1.5  2005/09/15 16:32:26  jcherry
 * Turn on wrapping of some gui stuff
 *
 * Revision 1.4  2005/08/01 17:23:22  jcherry
 * Made %include basic_containers.i come earlier (NStr was coming before
 * vector<string> and list<string>)
 *
 * Revision 1.3  2005/07/06 21:58:00  jcherry
 * Use conditional processing
 *
 * Revision 1.2  2005/06/29 18:14:29  jcherry
 * Moved CConstRef (needs to be early)
 *
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
