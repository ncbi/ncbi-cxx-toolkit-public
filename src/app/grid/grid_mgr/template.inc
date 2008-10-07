<!-- -- Start page menu view -- -->
<@START_PAGE_MENU@>
<table border="1" cellspacing="0" cellpadding="2">
<tr>
   <td>
     <a href="<@SELF_URL@>?cmd=showserver"><@ns_url_label@></a>
   </td>
</tr>
<tr>
    <td>
      <a href="<@SELF_URL@>?cmd=showncserver"><@nc_url_label@></a>
	</td>
</tr>
</table>
</@START_PAGE_MENU@>


<!-- -- NC Services view -- -->
<@NC_SERVICES_VIEW@>
<table border="1" cellspacing="0" cellpadding="2">
<@srvs_table_row_hook@>
</table>
</@NC_SERVICES_VIEW@>

<@nc_srvs_table_row_template@>
<tr>
    <td><@service@></td> <td><@host_port@></td>
</tr>
</@nc_srvs_table_row_template@>


<!-- -- Services view -- -->
<@SERVICES_VIEW@>
<table border="1" cellspacing="0" cellpadding="2">
<@srvs_table_row_hook@>
</table>
</@SERVICES_VIEW@>

<@srvs_table_row_template@>
<tr class="row_header"><td align="center">
<@SERVICE_NAME@>
</td></tr>
<tr><td>
<table width="700" cellspacing="0" cellpadding="2">
    <tr class="row_colored">
        <td>Host</td>
        <td>Port</td>
        <td>Version</td>
    </tr>
    <@srv_table_row_hook@>
</table>
</td></tr>
<tr><td>
</td></tr>
</@srvs_table_row_template@>




<@srv_table_row_template@>
<tr class="row_<@class@>">
    <td><@srv_host@></td>
    <td><@srv_port@></td>
    <td><@srv_version@></td>
</tr>
<tr class="row_<@class@>">
    <td>&nbsp;</td>
    <td>Queues</td>
    <td><@queue_href_hook@></td>
</tr>
</@srv_table_row_template@>

<@queue_href_template@>
<@queue_sep@>
<a
href="<@SELF_URL@>?cmd=queue_stat&server=<@queue_host@>&port=<@queue_port@>&queue=<@queue_name@>">
<@queue_name@>
</a>
</@queue_href_template@>



<!-- -- Queue view -- -->
<@QUEUE_VIEW@>
<table width="700" cellspacing="0">
<tr class="row_header"><td>
Statistics from <@HOST@>:<@PORT@>
</td></tr>
<tr class="row_header"><td>
Queue name : <@QUEUE_NAME@> </br>
</td></tr>
</table>
</br>

<table width="700" border="1" cellspacing="0" cellpadding="2">
<tr class="row_even"><td><pre class="row_even">
<@INFO@>
</pre></td></tr>
</table>
</br>
<h2>Worker nodes:</h2>
<@wm_table@>
</@QUEUE_VIEW@>

<@wm_table@>
<table width="700" border="1" cellspacing="0" cellpadding="2">
    <tr class="row_colored">
        <td>Client Name</td>
        <td>Program Version</td>
        <td>Host</td>
        <td>Last Access Date</td>
        <td>Build Date</td>
    </tr>
    <@wn_table_row_hook@>
</table>
</@wm_table@>

<@wn_table_row_template@>
    <tr class="row_<@class@>">
        <td><@name@></td>
        <td><@version@></td>
        <td><@wn_host_temp@></td>
        <td><@lastaccess@></td>
        <td><@builddate@></td>
    </tr>
</@wn_table_row_template@>

<@wn_host_text@>
<@wn_host@>:<@wn_port@>
</@wn_host_text@>

<@wn_host_href@>
<a href="<@SELF_URL@>?cmd=wn_stat&server=<@wn_host@>&port=<@wn_port@>">
    <@wn_host@>:<@wn_port@>
</a>
</@wn_host_href@>


<!-- -- Worker node view -- -->
<@WORKERNODE_QUEUE_VIEW@>
<table width="700" cellspacing="0">
<tr class="row_header"><td>
Statistics for worker node @ <@HOST@>:<@PORT@> </br>
</td></tr>
</table>
</br>

<table width="700" border="1" cellspacing="0" cellpadding="2">
<tr class="row_even"><td><pre class="row_even">
<pre>
<@INFO@>
</pre></td></tr>
</table>
</@WORKERNODE_QUEUE_VIEW@>


<!-- -- NetCache server statistics -- -->
<@NC_STAT_VIEW@>
<table width="700" cellspacing="0">
<tr class="row_header"><td>
Statistics for NetCache server @ <@HOST@>:<@PORT@> </br>
</td></tr>
</table>
</br>

<table width="700" border="1" cellspacing="0" cellpadding="2">
<tr class="row_colored"><td><@VERSION@></td> </tr>
<tr class="row_even"><td><pre class="row_even">
<pre>
<@INFO@>
</pre></td></tr>
</table>
</@NC_STAT_VIEW@>


<@ERROR_VIEW@>
Error message: <@error@>
</@ERROR_VIEW@>
