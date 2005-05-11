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
 * File Description:  SWIG interface file for custom handling of CObject's
 *
 */


// Set up reference counting for CObjects

#ifdef SWIGPYTHON
%define DECLARE_COBJECT(short_name, qual_name)
    // pointers and references
    %typemap(out) qual_name *, qual_name & {
        // always tell SWIG_NewPointerObj we're the owner
        $result = SWIG_NewPointerObj((void *) $1, $1_descriptor, true);
        if ($1) {
            $1->AddReference();
        }
    }
    // return by value
    %typemap(out) qual_name {
        $&1_ltype resultptr;
        resultptr = new $1_ltype(($1_ltype &) $1);
        $result = SWIG_NewPointerObj((void *) resultptr, $&1_descriptor, 1);
        resultptr->AddReference();
    }

    // make "deletion" in scripting language just decrement ref. count
    %extend qual_name {
    public:
        ~short_name() {self->RemoveReference();};
    }
    %ignore qual_name::~short_name;
    // provide for dynamic casting from CObject
    %extend qual_name {
    public:
        static qual_name* __dynamic_cast_to__(ncbi::CObject *obj) {
            return dynamic_cast<qual_name*>(obj);
        }
    }
    // Temporary, for backwards compatibility (CRefs)
    %extend qual_name {
    public:
        qual_name* __deref__(void) {
            return self;
        }
    }
%enddef
#endif

#ifdef SWIGPERL
%define DECLARE_COBJECT(short_name, qual_name)
    // pointers and references
    %typemap(out) qual_name *, qual_name & {
        // pointers and refs
        ST(argvi) = sv_newmortal();
        SWIG_MakePtr(ST(argvi++), (void *) $1, $1_descriptor, $shadow|$owner);
        if ($1) {
            $1->AddReference();
        }
    }
    
    // return by value
    %typemap(out) qual_name {
        // return by value
        $&1_ltype resultobj = new $1_ltype(($1_ltype &)$1);
        ST(argvi) = sv_newmortal();
        SWIG_MakePtr(ST(argvi++), (void *) resultobj,
                     $&1_descriptor, $shadow|$owner);
        resultobj->AddReference();
    }

    // make "deletion" in scripting language just decrement ref. count
    %extend qual_name {
    public:
        ~short_name() {self->RemoveReference();};
    }
    %ignore qual_name::~short_name;
    // provide for dynamic casting from CObject
    %extend qual_name {
    public:
        static qual_name* __dynamic_cast_to__(ncbi::CObject *obj) {
            return dynamic_cast<qual_name*>(obj);
        }
    }
    // Temporary, for backwards compatibility (CRefs)
    %extend qual_name {
    public:
        qual_name* __deref__(void) {
            return self;
        }
    }
%enddef
#endif


// CObject has a bunch of methods that are not so useful in 
// scripting languages, and clutter readline completion and
// the like.
// These also caused code-bloat when CRef's were being
// wrapped, but this is no longer an issue.
%ignore ncbi::CObject::CanBeDeleted;
%ignore ncbi::CObject::Referenced;
%ignore ncbi::CObject::ReferencedOnlyOnce;
%ignore ncbi::CObject::AddReference;
%ignore ncbi::CObject::RemoveReference;
%ignore ncbi::CObject::ReleaseReference;
%ignore ncbi::CObject::DoNotDeleteThisObject;
%ignore ncbi::CObject::DoDeleteThisObject;

/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/05/11 21:27:35  jcherry
 * Initial version
 *
 * ===========================================================================
 */
