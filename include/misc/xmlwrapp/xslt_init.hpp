/*
 * Copyright (C) 2001-2003 Peter J Jones (pjones@pmade.org)
 * All Rights Reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id$ 
 * NOTE: This file was modified from its original version 0.6.0
 *       to fit the NCBI C++ Toolkit build framework and
 *       API and functionality requirements. 
 */

/** @file
 * This file contains the definition of the xslt::init class.
**/

#ifndef _xsltwrapp_init_h_
#define _xsltwrapp_init_h_

// xmlwrapp includes
#include <misc/xmlwrapp/xml_init.hpp>

/// XSLT library namespace
namespace xslt {

/**
 * The xslt::init class is used to configure the XSLT engine.
 *
 * If you want to use any of the xslt::init member functions, do so before
 * you start any threads or use any other part of xsltwrapp. The member
 * functions may alter global and/or static variables. In other words, this
 * class is not thread safe.
 *
 * @note In xmlwrapp versions prior to 0.6.0, this class was used to initialize
 *       the library and exactly one instance had to be created before first
 *       use. This is no longer true: user code doesn't have to create any
 *       instances, but it @em can create as many instances as it wants.
**/
class init : public xml::init {
public:
    init (void);
    virtual ~init (void);

    /**
     * This function controls whether or not the XSLT engine will process
     * XInclusions by default while parsing the stylesheet. The default is
     * true.
     *
     * @param flag True to enable XInclusing processing; False otherwise.
     * @author Peter Jones
    **/
    static void process_xincludes (bool flag);

    /**
     * The current implementation of the extension functions support is rather
     * a hack than a clean code. This is related to some undocumented behavior
     * of libxml2 when it destroys xpath objects and poor documentation of how
     * extansion functions are supported in general.
     * Consequently all efforts were applied to have the proper memory management
     * however there is no guarantee that it works in 100% cases and there is
     * always a chance that libxml2 changes something unpredictedly.
     * So, to be on the safe side this function allows to switch on/off
     * intelligent memory management manually. If the memory control is
     * switched off then memory leaks may appear in case of passing node set
     * arguments to an extention function and setting node set return value. In
     * return you get proper node delivery.
     * Default: memory management is switched on.
     *
     * @param flag True to allow memory leaks in case of node set arguments and
     *             return values.
     * @author Sergey Satskiy, NCBI
    **/
    static void set_allow_extension_functions_leak (bool flag);

    /**
     * Provides the current setting of the extension functions memory
     * management.
     *
     * @author Sergey Satskiy, NCBI
    **/
    static bool get_allow_extension_functions_leak (void);

    /**
     * This member function controls whether or not the libxslt library data
     * will be cleaned up at exit. The default is true.
     *
     * @note By default the library clears all the libxslt data at exit.
     *       However it is possible that some parts of an application interact
     *       with libxslt directly and might call the libxml2 library cleanup
     *       data may lead to a crash.
     *       This member provides a workaround for the cases when multiple
     *       parties cleanup libxslt data at exit in multithreaded applications.
     *       It is supposed that at the beginning of the application there is
     *       a call similar to the following:
     *       xslt::init::library_cleanup_on_exit(false);
     *       and it is the application responsibility to make sure that
     *       XmlWrapp finishes xslt data handling before the other part of the
     *       application calls xsltCleanupGlobals().
     *
     * @param flag True to turn on libxslt cleanup at exit, false to turn it off
     * @author Denis Vakatov
    **/
    static void library_cleanup_on_exit (bool flag);

private:
    init (const init&);
    init& operator= (const init&);

    void init_library();
    void shutdown_library();

    static int ms_counter;
    static bool ext_func_leak;
    static bool do_cleanup_at_exit;
}; // end xslt::init class

// use a "nifty counter" to ensure that any source file that uses xsltwrapp
// will initialize the library prior to its first use
namespace {
    xslt::init g_xsltwrapp_initializer;
}

} // end xslt namespace
#endif
