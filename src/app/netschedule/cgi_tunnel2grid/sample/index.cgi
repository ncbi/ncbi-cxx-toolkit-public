#! /bin/sh

echo "Content-type: text/html"
echo
echo

echo "<html>"
echo "<head>"
echo "  <title>Start Tunnel to Grid Test</title>"
echo "</head>"

echo "<p/>"
echo "Submit a test job <br/>"

echo '<form method="post" action="../cgi_tunnel2grid.cgi">'

echo '    <INPUT TYPE="submit" NAME="Submit" VALUE="Submit">'
echo '    <INPUT TYPE="HIDDEN" NAME="ctg_project" VALUE="sample">'
echo '    <INPUT TYPE="HIDDEN" NAME="ctg_input" VALUE="html 8 19 2 33 22 4 22 1 445">'
echo '    <INPUT TYPE="HIDDEN" NAME="ctg_error_url" VALUE="sample/index.cgi">' 

echo '</form>'


   
echo '</body>'
echo '</html>'

