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


// Table definition.
// Tag "table_row_hook" will be replaced with some "table_row_template"
// See s_TableRowHook() function in the source code.

<@table@>
<table border="1" width="600" cellspacing="0" cellpadding="2">
  <tr class="row_header"> 
    <th width="40%"> Name  </th>
    <th width="*">   Phone </th>
    <th width="40%"> Email </th>
  </tr>
  <@table_row_hook@>
</table>
</@table@>


// Table row template definition

<@table_row_template@>
  <tr class="row_<@class@>"> 
    <td><@name@></td>
    <td><@phone@></td>
    <td><@email@></td>
  </tr>
</@table_row_template@>

// Sub-templates for the numeric adder example
<@ADDER@>
<p>
<@num_add@>
<b>Sum = <@num_sum@><b>
</p>
</@ADDER@>
