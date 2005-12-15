<@SESSION_VARS@>
Session attributes: <br>
      <@session_vars_table@>
</@SESSION_VARS@>


<@session_vars_table@>
<table border="1" width="600" cellspacing="0" cellpadding="2">
  <tr class="row_header"> 
    <th width="20%">Name</th>
    <th width="55%">Value</th>
    <th>Action</th>
  </tr>
  <@session_vars_hook@>
</table>
</@session_vars_table@>

<@session_var_row@>
<tr>
   <td><@name@></td>
   <td><@value@></td>
   <td><INPUT TYPE="submit" NAME="<@name@>_action" VALUE="Delete">
       <INPUT TYPE="submit" NAME="<@name@>_action" VALUE="Modify">
   </td>
</tr>
</@session_var_row@>
