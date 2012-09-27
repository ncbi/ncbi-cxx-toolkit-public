/*
 * $Id$
 */

#ifndef _raw_xslt_hpp_
#define _raw_xslt_hpp_


#include <ncbiconf.h>

namespace xslt
{
    class stylesheet;
}

namespace xslt {

    namespace impl {
        // This function is temporary and created specifically for
        // Ricardo Villamarin-Salomon
        // The function will be removed as soon as C++ style support for
        // extension functions and elements is introduced.
        // Note: the provided pointer is of type xsltStylesheet *
        NCBI_DEPRECATED
        void *  temporary_existing_get_raw_xslt_stylesheet(xslt::stylesheet & s);
    }
}


#endif

