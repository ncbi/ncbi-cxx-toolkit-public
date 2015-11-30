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

/*
 * @attention
 * Thread safety
 * It is thread-safe to process different documents from different threads.
 * It is not thread-safe to modify the same document from different threads.
 * It is thread-safe to perform non-modifying operations on the same document
 * from different thread except of the following cases:
 * 1. Applying the same stylesheet to different documents. (Note: this
 *    operation is thread-safe only if your compiler is C++11 conformant.)
 * 2. Using copies of an XPath query result node sets from different threads.
 * 3. For a certain node the following operations are not thread-safe to be
 *    performed from different threads:
 *    - dereferencing the node iterator
 *    - dereferencin the node attributes iterator
 *    - searching for an attribute of the node
 *
 * For more details see the 'Thread Safety' section in the C++ Toolkit Book at:
 * http://www.ncbi.nlm.nih.gov/toolkit/doc/book/ch_xmlwrapp/
 */


#ifndef _xmlwrapp_xmlwrapp_h_
#define _xmlwrapp_xmlwrapp_h_

#include <misc/xmlwrapp/ownership.hpp>
#include <misc/xmlwrapp/xml_init.hpp>
#include <misc/xmlwrapp/node.hpp>
#include <misc/xmlwrapp/attributes.hpp>
#include <misc/xmlwrapp/document.hpp>
#include <misc/xmlwrapp/event_parser.hpp>
#include <misc/xmlwrapp/namespace.hpp>
#include <misc/xmlwrapp/xpath_expression.hpp>
#include <misc/xmlwrapp/node_set.hpp>
#include <misc/xmlwrapp/schema.hpp>
#include <misc/xmlwrapp/dtd.hpp>
#include <misc/xmlwrapp/exception.hpp>
#include <misc/xmlwrapp/errors.hpp>
#include <misc/xmlwrapp/xml_save.hpp>
#include <misc/xmlwrapp/document_proxy.hpp>

#endif
