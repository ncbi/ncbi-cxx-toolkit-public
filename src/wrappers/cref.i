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
 * File Description:  SWIG interface file for custom handling of CRef's
 *
 */


namespace ncbi {
    template<class T> class CRef
    {
        %typemap(out) CRef<T> {
            if ($1.NotNull()) { // don't dereference a null CRef
                $1->AddReference();
            }
#ifdef SWIGPYTHON
            $result = SWIG_NewPointerObj((void *) $1.ReleaseOrNull(),
                                         $descriptor(T *), true);
#endif
#ifdef SWIGPERL
            ST(argvi) = sv_newmortal();
            SWIG_MakePtr(ST(argvi++), (void *) $1.ReleaseOrNull(),
                         $descriptor(T *), SWIG_OWNER|SWIG_SHADOW);
#endif
        }

        // This makes, e.g., vector::operator[]
        // return a raw object in scripting language
        %typemap(out) CRef<T>&, CRef<T>* {
            if ($1->NotNull()) { // don't dereference a null CRef
                (*$1)->AddReference();
            }
#ifdef SWIGPYTHON
            $result = SWIG_NewPointerObj((void *) $1->GetPointerOrNull(),
                                         $descriptor(T *), true);
#endif
#ifdef SWIGPERL
            ST(argvi) = sv_newmortal();
            SWIG_MakePtr(ST(argvi++), (void *) $1->GetPointerOrNull(),
                         $descriptor(T *), SWIG_OWNER|SWIG_SHADOW);
#endif
        }

        %typemap(in)  CRef<T> (CRef<T> *pcref, T* pobj) {
            // accept either a CRef<T> or a T
            if (SWIG_ConvertPtr($input,(void **) &pcref, 
                                $&1_descriptor,0) != -1) {
                $1 = *pcref;
            } else if (SWIG_ConvertPtr($input, (void **) &pobj, 
                                $descriptor(T *), 0) != -1) {
                $1.Reset(pobj);
            } else {
#ifdef SWIGPYTHON
                PyErr_SetString(PyExc_TypeError, #T " or CRef<" #T "> expected");
#endif
                SWIG_fail;
            }
        }
        %typemap(in)  const CRef<T> & (CRef<T> *pcref, T* pobj, CRef<T> cref) {
            // accept either a CRef<T> or a T
            if (SWIG_ConvertPtr($input, (void **) &pcref, 
                                $1_descriptor, 0) != -1) {
                $1 = pcref;
            } else if (SWIG_ConvertPtr($input, (void **) &pobj, 
                                $descriptor(T *), 0) != -1) {
                cref.Reset(pobj);
                $1 = &cref;
            } else {
#ifdef SWIGPYTHON
                PyErr_SetString(PyExc_TypeError, #T " or CRef<" #T "> expected");
#endif
                SWIG_fail;
            }
        }
    public:
        T* operator->(void);
    };
}
%ignore ncbi::CRef;


// Support for specialized vector<CRef<CFoo> > and list<CRef<CFoo> >.
// These take and return CFoo proxy objects rather than CRef proxies.

#ifdef SWIGPYTHON
%{

static int CRefCheck(PyObject *obj, swig_type_info *ty) {
    void *ptr;
    if (SWIG_ConvertPtr(obj, &ptr, ty, 0) == -1) {
        PyErr_Clear();
        return 0;
    } else {
        // Forbid NULL; correct?
        return ptr != 0;
    }
}

template<class T> ncbi::CRef<T> CRefToCpp(PyObject *arg, swig_type_info *ty) {
    T* pobj;
    ncbi::CRef<T> rv;
    if (SWIG_ConvertPtr(arg, (void **) &pobj, 
                        ty, 0) != -1) {
        rv.Reset(pobj);
    } else {
        //PyErr_SetString(PyExc_TypeError, #T " or CRef<" #T "> expected");
        //SWIG_fail;
    }
    return rv;
}


template<class T> PyObject* CRefFromCpp(ncbi::CRef<T> cr, swig_type_info *ty) {
    if (cr.NotNull()) { // don't dereference a null CRef
        cr->AddReference();
    }
    return SWIG_NewPointerObj((void *) cr.ReleaseOrNull(),
                              ty, true);
}


%}

%define VECTOR_CREF(cref_name, T)
#define CRefCheck_XXX(obj) CRefCheck(obj, $descriptor(T*))
#define CRefToCpp_XXX(obj) CRefToCpp<T>(obj, $descriptor(T*))
#define CRefFromCpp_XXX(cr) CRefFromCpp(cr, $descriptor(T*))
namespace std {
specialize_std_vector(ncbi::CRef<T>, CRefCheck_XXX, CRefToCpp_XXX, CRefFromCpp_XXX);
}
%template(cref_name) std::vector<ncbi::CRef<T> >;
#undef CRefCheck_XXX
#undef CRefToCpp_XXX
#undef CRefFromCpp_XXX
%enddef

%define LIST_CREF(cref_name, T)
#define CRefCheck_XXX(obj) CRefCheck(obj, $descriptor(T*))
#define CRefToCpp_XXX(obj) CRefToCpp<T>(obj, $descriptor(T*))
#define CRefFromCpp_XXX(cr) CRefFromCpp(cr, $descriptor(T*))
namespace std {
specialize_std_list(ncbi::CRef<T>, CRefCheck_XXX, CRefToCpp_XXX, CRefFromCpp_XXX);
}
%template(cref_name) std::list<ncbi::CRef<T> >;
#undef CRefCheck_XXX
#undef CRefToCpp_XXX
#undef CRefFromCpp_XXX
%enddef

#endif  // SWIGPYTHON

#ifdef SWIGPERL
%{

static int CRefCheck(SV *obj, swig_type_info *ty) {
    void *ptr;
    if (SWIG_ConvertPtr(obj, &ptr, ty, 0) == -1) {
        return 0;
    } else {
        // Forbid NULL; correct?
        return ptr != 0;
    }
}

template<class T> ncbi::CRef<T> CRefToCpp(SV *arg, swig_type_info *ty) {
    T* pobj;
    ncbi::CRef<T> rv;
    if (SWIG_ConvertPtr(arg, (void **) &pobj, 
                        ty, 0) != -1) {
        rv.Reset(pobj);
    } else {
        //SWIG_fail;
    }
    return rv;
}


template<class T> void CRefFromCpp(SV* sv, ncbi::CRef<T> cr, swig_type_info *ty) {
    if (cr.NotNull()) { // don't dereference a null CRef
        cr->AddReference();
    }
    //ST(argvi) = sv_newmortal();
    SWIG_MakePtr(sv, (void *) cr.ReleaseOrNull(),
                 ty, SWIG_OWNER|SWIG_SHADOW);
}


%}

// These macros are just like the Python equivalents except that
// CRefFromCpp* takes an additional argument.
// Conditional SWIGing seems not to work within the macro.

%define VECTOR_CREF(cref_name, T)
#define CRefCheck_XXX(obj) CRefCheck(obj, $descriptor(T*))
#define CRefToCpp_XXX(obj) CRefToCpp<T>(obj, $descriptor(T*))
#define CRefFromCpp_XXX(sv, cr) CRefFromCpp(sv, cr, $descriptor(T*))
namespace std {
specialize_std_vector(ncbi::CRef<T>, CRefCheck_XXX, CRefToCpp_XXX, CRefFromCpp_XXX);
}
%template(cref_name) std::vector<ncbi::CRef<T> >;
#undef CRefCheck_XXX
#undef CRefToCpp_XXX
#undef CRefFromCpp_XXX
%enddef

%define LIST_CREF(cref_name, T)
#define CRefCheck_XXX(obj) CRefCheck(obj, $descriptor(T*))
#define CRefToCpp_XXX(obj) CRefToCpp<T>(obj, $descriptor(T*))
#define CRefFromCpp_XXX(sv, cr) CRefFromCpp(sv, cr, $descriptor(T*))
namespace std {
specialize_std_list(ncbi::CRef<T>, CRefCheck_XXX, CRefToCpp_XXX, CRefFromCpp_XXX);
}
%template(cref_name) std::list<ncbi::CRef<T> >;
#undef CRefCheck_XXX
#undef CRefToCpp_XXX
#undef CRefFromCpp_XXX
%enddef

#endif  // SWIGPERL


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
