/* Host info */
#undef HOST
#undef HOST_CPU
#undef HOST_VENDOR
#undef HOST_OS

/* Platform info */
#undef NCBI_OS
#undef NCBI_OS_UNIX
#undef NCBI_OS_MSWIN
#undef NCBI_OS_MAC

/* There is gethostbyname_r() */
#undef HAVE_GETHOSTBYNAME_R

/* There is strdup() */
#undef HAVE_STRDUP

/* Microsoft C++ specific */
#undef SIZEOF___INT64

/* Does not give enough support to the in-class template functions */
#undef NO_INCLASS_TMPL

/* Can use exception specifications("throw(...)" after func. proto) */
#undef NCBI_USE_THROW_SPEC

/* Non-standard basic_string::compare() -- most probably, from <bastring> */
#undef NCBI_OBSOLETE_STR_COMPARE

/* "auto_ptr" template class is not implemented in <memory> */
#undef HAVE_NO_AUTO_PTR

/* "min"/"max" templates are not implemented in <algorithm> */
#undef HAVE_NO_MINMAX_TEMPLATE

/* SYBASE libraries are available */
#undef HAVE_LIBSYBASE

/* NCBI C Toolkit libs are available */
#undef HAVE_NCBI_C

/* Fast-CGI library is available */
#undef HAVE_LIBFASTCGI

/* NCBI SSS DB library is available */
#undef HAVE_LIBSSSDB

/* NCBI PubMed libraries are available */
#undef HAVE_PUBMED

/* New C++ streams dont have ios_base:: */
#undef HAVE_NO_IOS_BASE

/* This is good for the GNU C/C++ compiler on Solaris */ 
#undef __EXTENSIONS__

/* This is good for the EGCS C/C++ compiler on Linux(e.g. proto for putenv) */
#undef _SVID_SOURCE

/* There is no "std::char_traits" type defined */
#undef HAVE_NO_CHAR_TRAITS
