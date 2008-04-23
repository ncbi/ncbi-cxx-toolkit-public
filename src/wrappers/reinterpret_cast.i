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
 * File Description:  Provide analog of reinterpret_cast in scripting languages
 *
 */


%rename(reinterpret_cast) reinterpret_cast_func;

#ifdef SWIGPERL
%typemap(out) SV* "ST(argvi++) = $1;";
#endif

%inline %{

#ifdef SWIGPYTHON
static PyObject*
#endif
#ifdef SWIGPERL
static SV*
#endif
reinterpret_cast_func(const char* desc, void *ptr)
{
    swig_type_info *type;
    type = NCBI_SWIG_MangledTypeQueryModule(&swig_module, &swig_module, desc);
    if (type) {
        return SWIG_NewPointerObj(ptr, type, 0);
    } else {
        throw std::runtime_error(std::string("type not found: ") + desc);
    }
}

%}


#ifdef SWIGPYTHON
%runtime %{

#define NCBI_SWIG_MangledTypeQueryModule SWIG_MangledTypeQueryModule

%}
#endif  // SWIGPYTHON

#ifdef SWIGPERL

// Mangled type query for Perl is broken, at least for older SWIG.
// We'll build our own look-up map the first time around.

%runtime %{

#include <string>
#include <map>

static swig_type_info*
NCBI_SWIG_MangledTypeQueryModule(swig_module_info *start, 
                                 swig_module_info *end, 
                                 const std::string& name) {

  static std::map<std::string, swig_type_info*> *info_map = 0;

  if (!info_map) {
    info_map = new(std::map<std::string, swig_type_info*>);

    swig_module_info *iter = start;
    do {
      for (size_t i = 0; i < iter->size; ++i) {
        if (iter->types[i]->name) {
          (*info_map)[iter->types[i]->name] = iter->types[i];
        }
      }
      iter = iter->next;
    } while (iter != end);
  }

  return (*info_map)[name];

}

%}

#endif  // SWIGPERL
