
JSLint is a tool for checking JavaScript

http://www.jslint.com/
http://www.jslint.com/lint.html
http://www.jslint.com/rhino/index.html

To run it from command line, use Rhino (runs JS via Java).

    $ fink install rhino    # on OS X
    $ install -d ~/Library/Java/Extensions
    $ ln -s /sw32/share/java/rhino/js.jar ~/Library/Java/Extensions/js.jar

Obviously, Linux etc. will need adjustment of these commands, but You can do that..

Options can be given as comments in the source file:

/*jslint onevar: true, undef: true, nomen: true, eqeqeq: true, plusplus: true, bitwise: true, regexp: true, newcap: true, immed: true, strict: true */

Launching via Rhino:

    java org.mozilla.javascript.tools.shell.Main jslint.js myprogram.js

-AKa 14-Oct-2010
