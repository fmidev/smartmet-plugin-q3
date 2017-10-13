

-----------------
  q3 Test Suite
-----------------

In order to run the tests, a PHP gateway needs to be used instead of direct
access to the q2 and q3 servers. This is because of "cross domain ajax" access
restrictions in most of the browsers (Safari does not need this, when the
suite is opened as a local 'file://' URL).

To install the server side:

    1. Apache is installed and running (with PHP support) on crash.fmi.fi
    
    CentOS 5:
        sudo yum install httpd      # Apache2
        sudo yum install php
        sudo /etc/init.d/httpd restart

    2.
    sudo ln -s ~kauppi/svn/q3/trunk/test-suite/ /var/www/html/q3-trunk-testsuite
    
    This leads 'http://crash.fmi.fi/q3-trunk-testsuite' to point to us.


To use the suite:
    
    3. Open: http://crash.fmi.fi/q3-trunk-testsuite
    

-end

