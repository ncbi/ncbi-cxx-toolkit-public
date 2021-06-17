##############################################################################
#
#
#  The template library for demo_html_template.
#
#
##############################################################################

Any text outside blocks <@NAME@>...</@NAME@> will be ignored.
So, comments can be easy added everywhere in any convenient format.
Blocks with sub-template definitions should start from the beginning of line.


// Page header. Multi-line definition.

<@HEADER@>
    <h3>
        <@HEADLINE@>
    </h3>
</@HEADER@>

// Page view.

<@VIEW@>
    <@table@>
</@VIEW@>


// Page footer. One-line definition.

<@FOOTER@><p>Generation date: <@DATE@><p></@FOOTER@>


// Any tag can be remapped latter in the other loaded template library
// or directly in the program.

<@TITLE@>Default page title if the TITLE tag is not mapped</@TITLE@>



// Include some sub-template libraries.
// Included files can contains relative or absolute path
// in OS dependent format. 

#include "demo_html_template_phone.inc"
#include "demo_html_template_adder.inc"
