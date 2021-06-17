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
