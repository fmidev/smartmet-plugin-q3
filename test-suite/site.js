/*
* SITE.JS
*
* Notes about the coding:
*
*   '$some' means that the variable carries jQuery selection ('$' is just another character when it comes
*           to JavaScript names; this is a (useful) naming convention.
*
*   Comments s.a. "something is 'div[xxx]'" use the jQuery/CSS syntax for describing selections. This one
*   means it's a 'div' object with 'xxx' attribute.
*
*   jQuery is Good. See its documentation: http://jquery.org/
*
*   Author: Asko Kauppi 22-Dec-2010 (and earlier)
*/

/*jslint white:false, 
    onevar:false, 
    undef:true, 
    nomen:false, 
    eqeqeq:false, 
    plusplus:false, 
    bitwise:true, 
    regexp:true, 
    newcap:true, 
    immed:true, 
    strict:true 
*/
"use strict";

/*global $, init_buttons, setTimeout, alert, console */

// Make 'console.log()' not be so destructive in IE8 (where it's only recognized if
// developer tools are running; otherwise causing a stop of JS processing).
//
// Also Firefox without Firebug installed will require this.
//
if (!console) {
    var console= {
        log: function(o){
            // nothing (could place the stuff to some div or something)
        }
    };
}


var SCRIPTS_PATH= "tests/";

var SCRIPT_SIZE= [100, 10];

var Q3_SERVER= "crash.fmi.fi:8091";     //"smartmet.fmi.fi/q3";
var Q2_SERVER= "smartmet.fmi.fi/q2";

var NOTHING_PNG= "gra/nothing.png";

// If loaded via 'http://crash.fmi.fi/q3-testsuite/', use PHP to do cross domain Ajax.
//
// Otherwise (if loaded via 'file://...'), do ajax directly (but requires a browser s.a. Safari that allows this).
//
var USE_PHP = !( location.href.match( /^file:\/\// ) );

/*
* str= trim( str )
*
* Remove white space from the beginning and end of the string.
*/
function trim( str ) {
    // Note: '\s\S' means "anything" (even newline). '.' in JavaScript regexps does not cover newline.
    //       '*?' is a non-greedy operator
    //
    var arr= str.match( /^\s*([\s\S]*?)\s*$/ );
        //
        // [1]: trimmed string

    return arr ? arr[1] : str;
}

function trim_begin( str ) {
    return str.replace( /^\s+/, "" );
}

function trim_end( str ) {
    return str.replace( /\s+$/, "" );
}


/*
* If 'params_mode'=true, lines are URL parameters as such.
*/
function my_escape( str, params_mode ) {

    if (typeof str !== "string") {
        throw( typeof(str) );
    }

    // Note: replace multiple whitespace with only one (does not change either q3 or q2 
    //      script but shortens the visible URLs slightly)
    //
    str= str.replace( / +/g, "%20" )
            .replace( /#/g, "%23" )
            .replace( /\+/g, "%2B" )
            .replace( /\-/g, "%2D" )
            .replace( /\{/g, "%7B" )    // needed if passing URL via PHP proxy
            .replace( /\}/g, "%7D" )    // -''-
            ;

    if (params_mode) {
        return str.replace( /\n+/g, "&" );

    } else {
        return str.replace( /\n/g, "%0A" )
                    .replace( /&/g, "%26" )
                    .replace( /\=/g, "%3D" );
    }
}


/*
* Create a PHP gateway URL for accessing the ajax target 'url'.
*
*/
function gateway_url( url ) {

    if (!USE_PHP) {
        return url;     // gateway not needed - go right through
    } else {
        // See 'php/proxy.php' for its API.
        //
        return "php/proxy.php?mode=native&url="+ my_escape(url);
    }
}


/*
* Check whether the Q3 server is up and show its version.
*/
function check_version() {
    var $span= $("#q3_version");
    var url= "http://"+ $("#q3_url").val();

    $span.text("");     // while we're refreshing it

    $.ajax( {
        // TBD: For some reason the first line gets lost when going through PHP proxy.
        //
        url: gateway_url( url+"/?code=return%20RPM_VERSION" ),
        type: "GET",
        dataType: "text", 
        cache: false,
        success: function(data) {
            // Seems to come here (with data=="") even in timeout
            //
            if (!data) {
                $span.text("not running").addClass("offline");
            } else {
                // Remove heading and trailing '"'
                //
                var ver= data.replace(/["\s]/g,"");
                
                // Non-packaged ('make run') will show "null"
                //
                $span.text( ver!="null" ? ver: "running (version unknown)" ).removeClass("offline");
            }
        },
        error: function() {
            $span.text("not running").addClass("offline");
        }
    } );
}


/*
* str= form_params( { key: val, ... } )
*
* Create a URL params part ("key1=val1&key2=val2&...")
*/
function form_params( t ) {

    var s= "";
    var delim= "";
 
    $.each(t, function(k,v) {
        if (v) {    // skip 'key=null'
            s += delim+k+"="+my_escape(v);
            delim= "&";
        }
    } );

    return s;
}


/*
* str|null= find_meta( lines_str, key_str )
*
* Find a certain meta field from test comments
*
* lines:    "-- xxx" (multiple lines)
* key:      i.e. "description"
*
* key 'description' returns the end of the line "-- description: ..."
*/
function find_meta( str, key ) {

    // 'm' flag for multiple lines ('^' and '$' work for each line)
    //
    // Note: Must use 'new Regexp' to build the regex using 'key'.
    //
    var re= new RegExp( "^\\-\\- "+key+": +(.*)$", "m" );
    
    var arr= str.match( re );
        //
        // arr[1]: value part
        
    return arr && arr[1];
}

// Self-test
//
var tmp= find_meta( "\n\n-- key: xxx\nyyy", "key" );
if (tmp != "xxx") {
    throw ("SELFTEST FAILED: '"+tmp+"' isn't 'xxx'");
}


/*
* Given a 'div[url_params_x]', resolves a URL for running that script (on q3 server).
*/
function q3_url( $q3 ) {

    var code= $q3.find("textarea.script").val();
    
    code= code.replace( /\-\-.*$/mg, "" );    // remove line comments (to keep URL short)
        //
        // Note: Replaced '.' with '[\s\S[' to please jslint

    return "http://"+ $("#q3_url").val() +"?"+ $q3.attr("url_params_x")+"&code="+ my_escape( code );
}

/*
* Given a 'div[url_params_x]', resolves a URL for running that script (on q2 server).
*/
function q2_url( $q2 ) {

    var url= "http://"+ $("#q2_url").val() +"?";
    
    var q2_code= $q2.find("textarea").val();
    if (q2_code) {
        if (q2_code.match( /\s*[Rr][Ee][Ss][Uu][Ll][Tt]\s*=/ )) {
            url += $q2.attr("url_params_x") +
                    "&requestType=macroParam" +
                    "&macroParamStr="+ my_escape(q2_code);
        } else {
            // Take some (but not all) of the fields given in test case comments
            //
            var tmp= $q2.attr("url_params_x");

            $.each( [ "maxDecimals", 
                      "validTime",
                      "originTime" ], function(_,v) {
                var arr= tmp.match( new RegExp( v+"=([^&]+)" ) );
                    //
                    // arr[1]: value part

                if (arr) {
                    url += v+"="+my_escape(arr[1])+"&";
                }
            } );
            
            // 'q2_code' contains URL params as such (mode 'timeSerial' or other), one on 
            // each line
            //
            url += my_escape( q2_code, true );      // replace newlines with '&'
        }
    }
    return url;
}
        

/*
* void= test_entry( $host, test_str, "q2"|"ok"|"image" )
*
* Create one test entry into '$host'.
*
* Note: We only create the structure for the entry. Caller fills in the scripts etc.
*/
function test_entry( $host, test, type ) {

    var type_q2=    type=="q2";
    var type_ok=    type=="ok";
    var type_image= type=="image";

    /*
    * Note: Output fields are 'div' (not 'textarea') so that we can place color highlighted
    *       text into them.
    *
    <host [class=collapsed]>
        <div class='header'>
            <div class='go_button' picture='go' />     // floated to right by CSS
            <div class='collapse_button' />             // -''-
            <div class='name'>...test...</div>
            <div class='description'></div>
        </div>
        <div class='q3'>
            Q3 script:
            <a class='url' target='_blank'>(url)</a>
            <textarea class='script'></textarea>
            <div class='result'></div>          // if 'type'!="image"
        </div>
        <div class='q2'>                        // if 'type'=="q2"
            Q2 script:
            <a class='url' target='_blank'>(url)</a>
            <textarea class='script'></textarea>
        </div>
        <div class='ok'></div>                  // if 'type'=="ok"
        <img class='result' />                  // if 'type'=="image"
        <img class='ok' />                      // if 'type'=="image"
    </host>
    */

    $host.html( 
        "<div class='header'>"+
            "<div class='go_button' picture='go' />"+
            "<div class='collapse_button' />"+
            "<div class='name'>"+test+"</div>"+
            "<div class='description'></div>"+
        "</div>"+
        "<div class='q3'>Q3 script:"+
            "<a class='url' target='_blank'>...</a>"+
            "<textarea class='script'></textarea>"+
            (type_image ? "" : "<div class='result'></div>")+
        "</div>"
    );
    
    // All tests collapsed by default
    //
    $host.addClass( "collapsed" );

    if (type_q2) {
        $host.append(
            "<div class='q2'>Q2 script:"+
                "<a class='url' target='_blank'>...</a>"+
                "<textarea class='script'></textarea>"+
            "</div>"
        );
    } 
    
    $host.append( 
        type_image ? "<img class='result' /><img class='ok' />" 
                   : "<div class='ok'></div>"
    );

    var $q3= $host.find(".q3");

    var q3_update_f= function() {
        var url= q3_url( $q3 );

        $q3.find("a.url").attr( "href", url ).text( url ).attr( "title", url );
    };

    $("#q3_url").change( q3_update_f );
    $q3.find("textarea.script").blur( q3_update_f );

    if (type_q2) {    
        var $q2= $host.find(".q2");
        var q2_update_f= function() {
            var url= q2_url( $q2 );
            $q2.find("a.url").attr( "href", url ).text( url ).attr( "title", url );
        };

        $("#q2_url").change( q2_update_f );
        $q2.find("textarea.script").blur( q2_update_f );
    }

    // Note: 'rows' and 'cols' are HTML attributes and don't come from CSS. Slightly annoying.
    //
    $host.find("textarea").each( function() {
        var $this= $(this);     // textarea

        if ($this.hasClass("script")) {
            $this.attr( "rows", SCRIPT_SIZE[1] ).attr( "cols", SCRIPT_SIZE[0] );
        } 
    } );

    // Button logic
    //    
    init_buttons( $host.find(".go_button") );

    init_collapse_buttons( $host.find(".collapse_button") );
}

/*
* Initialize a test entry
*
* $host:    The div within which we'll place the entry (once loaded)
* test:     name of the test
*/
function test_init( $host, test ) {

    // Load the script
    //
    $.ajax( {
        url: SCRIPTS_PATH+test+".lua",
        type: "GET",
        dataType: "text", 
        cache: false,
        success: function(data) {
            // Extract information from the comments
            // 
            /*
--[[
-- NAME WHATEVER
--
-- description: <description text>
--]]

--[[q2:
    ... q2 comparison script (optional)
]]

--[[ok:
    ... right response (if 'q2' not given)
]]
            */
            // NOTE: '.' cannot be used for multiline catches; it does not match a newline, ever.
            //      '[\s\S]' means literally "anything" (including a newline).  --AKa 13-Sep-10
            //
            // Note: We remove leading and trailing whitespace from the script part.
            //      ('+?' is JavaScript regexp syntax for non-greedy match)
            //
            var arr= data.match( /^\s*\-\-\[\[\s*([\s\S]*?)\s*\-\-\]\]\s*([\s\S]+?)\s*$/ );
                //
                // [1]: header part (trimmed)
                // [2]: rest (trimmed)

            var meta= arr && arr[1];
            var rest= arr ? arr[2] : data;    // if no meta part at all

            var description;
            var image_src;

            // Extract header fields
            //         
            var projection;
            var validtime;
            var decimals;
            var origintime;
            var gridsize;
   
            if (meta) {
                description= find_meta( meta, "description" );

                image_src= find_meta( meta, "image" );
                if (image_src) {
                    image_src= SCRIPTS_PATH + ((image_src=="true") ? test : image_src) +".png";
                }

                projection= find_meta( meta, "projection" );
                validtime= find_meta( meta, "validtime" );
                decimals= find_meta( meta, "decimals" );
                origintime= find_meta( meta, "origintime" );
                gridsize= find_meta( meta, "gridsize" );
            }

            var ok_block;
            var q2_block;

            // Extract 'ok:' block (if any)
            //
            arr= rest.match( /^([\s\S]*?)\s*\-\-\[\[ok:\s*([\s\S]+?)\s*\]\]([\s\S]*)$/ );
                //
                // [1]: head
                // [2]: ok block (trimmed)
                // [3]: rest (trimmed from beginning)
            
            if (arr) {
                ok_block= arr[2];
                rest= arr[1]+arr[3];
            }

            // Extract 'q2:' block (if any)
            //
            arr= rest.match( /^([\s\S]*?)\s*\-\-\[\[q2:\s*([\s\S]+?)\s*\]\]([\s\S]*)$/ );
                //
                // [1]: head
                // [2]: q2 block (trimmed)
                // [3]: rest (trimmed from beginning)
            
            if (arr) {
                q2_block= arr[2];
                rest= trim_end( arr[1]+arr[3] );
            }

            // Extract yet more '--[[ ... ]]' block comments away
            //
            // This allows using '--' line comments to show them in the autotest HTML page
            // and '--[[ ... ]]' only in the file (not in HTML) as comments.
            //
            rest= rest.replace( /\-\-\[\[[\s\S]*?\]\]/g, "" );

            // Remove empty lines in the front and end
            //
            var q3_block= trim(rest);

            // Create a structure for the test into '$host'
            //
            test_entry( $host, test, q2_block ? "q2" : image_src ? "image" : "ok" );

            // Fill in the script etc.
            //
            $host.find(".q3 textarea.script").val( q3_block );
            
            if (q2_block) {
                $host.find(".q2 textarea.script").val( q2_block );
            }
            
            if (ok_block) {
                $host.find("div.ok").text( ok_block );
            }
            
            if (image_src) {
                $host.find("img.result").attr( "src", NOTHING_PNG );  // show a non-loaded image picture (load only when test started)
                
                $host.find("img.ok").attr( "src", image_src );  // comparison picture
            }
            
            // Attach URL parameters to the '.q3' and '.q2' elements
            //
            $host.find(".q3").attr( "url_params_x", 
                form_params( {
                    projection: projection,
                    validtime: validtime,
                    decimals: decimals,
                    origintime: origintime,
                    gridsize: gridsize
                } ) );

            $host.find(".q2").attr( "url_params_x",
                form_params( {
                    projection: projection || "stereographic,20,90,60:6,51.3,49,70.2",
                    validTime: validtime || "NOW",
                    maxDecimals: decimals,
                    originTime: origintime,
                    gridSize: gridsize || "5,6"
                } ) );

            // Update the URLs
            //
            $host.find("textarea.script").blur();   // like someone edited it
        },
        error: function(_,b,c) {
            console.log( "Failed", b,c );
        }
    } );
}


/*
* Init button scripts
*/
function init_buttons( $set ) {

    $set.click( function() {
        var $this= $(this);
        var $test= $this.parent().parent();
        
        var state= $this.attr("picture");   // "go"|"stop"|...
                
        if (state=="stop") {
            // TBD: Behaviour of 'run all' button when earlier launched tests are still
            //      running is not implemented. At least don't give repeated nags.
            //
            // alert( "q3 API does not allow stop... (zero-mq would help)" );
        } else {
            $this.attr( { picture: "stop" } );
            $test.attr( { status: "busy" } );
            var $button= $this;
    
            var q3_url= $test.find( ".q3 .url" ).attr("href");
            var q2_url= $test.find( ".q2 .url" ).attr("href");

            var $image_result= $test.find( "img.result" );

            if ($image_result.length > 0) {
                // No ajax needed, just set the image 'src' and let the user compare to sample picture
                //
                $image_result.attr( { src: "" } );
                
                setTimeout( function() {
                    // When the image has loaded, change the 'stop' button back to go.
                    //
                    $image_result.load( function() {
                        $this.attr( { picture: "go" } );
                        $test.removeAttr( "status" );
                        
                        // Note: we should also clear the binding to us here... TBD
                    } );

                    $image_result.attr( { src: q3_url } );
                }, 500 );
                
                return;
            }

            var $q3_result= $test.find( ".q3 div.result" );
            var $ok= $test.find("div.ok");     // used both for fixed right answer and for q2 output

            // Is there actually a q2 script or do we go with fixed known-good result
            //
            var use_q2= $test.find(".q2").length > 0;

            // Clear the results
            //
            $q3_result.html("");
            if (use_q2) {
                $ok.html("");
            }
            
            var wait= use_q2 ? 2:1;    // Need this many calls to 'done_f()' 

            /*
            * Called for succesful 'q3' and 'q2' queries - if both done,
            * do the comparison.
            */
            var done_f= function() {
                if (--wait > 0) { return; }   // waiting for the other

                // Process the 'q3' output - remove some JSON formatting, trailing newline etc.
                //
                var a= trim_end( $q3_result.text() )    // remove trailing newline
                        .replace( /^\"/, "" ).replace( /\"$/, "" );     // remove '"' surrounding a text string (s.a. '"ok"'->'ok')

                var b= $ok.text();
                var st;
                
                if (!use_q2) {

                    // Compare the output by a special function that adds color markup to 'q3',
                    // trying to mark the places that were wrong.
                    //
                    var arr= differ_json( a, b );
                        //
                        // { "ok"|"fail", null | q3_highlighted_str, null | ok_highlighted_str }
                    
                    st= arr[0];
                    if (arr[1]) {
                        $q3_result.html( arr[1] );
                    }
                    if (arr[2]) {
                        $ok.html( arr[2].trim() );     // Does not change its '.text()' - just highlighting added
                    }

                } else {
    
                    // Compare the output by a special function that adds color markup to both
                    // entries, highlighting slight deviations and major differences.
                    //
                    var arr= differ_matrices( a, b );
                        //
                        // { "ok"|"almost"|"fail", q3_highlighted_str, q2_highlighted_str }
                    
                    st= arr[0];
                    if (arr[1]) {
                        $q3_result.html( arr[1] );
                    }
                    if (arr[2]) {
                        $ok.html( arr[2] );
                    }
                }
          
                if (st=="fail") {
                    $button.attr( "picture", "failed" );
                } else /* "almost" or "ok" */ {
                    $button.attr( "picture", "success" );
                }
                
                // Placing the status string to '[status]' of the 'div[test]' element changes
                // its background (see CSS).
                //
                $test.attr( { status: st } );
            };

            // During the processing set '.status' to 'busy'
            //
            $test.attr( { status: "busy" } );

console.log( gateway_url( q3_url ) );

            // Start fetching q3 results
            //
            $.ajax( {
                url: gateway_url( q3_url ),
                type: "GET",
                dataType: "text", 
                cache: false,
                timeout: 10000, // ms
                success: function( data, status, xml_http_req ) {
                    // Seems to come here (with data=="", status=="success) even in timeout
                    //
//console.log( { data: data, st: status, xml: xml_http_req } );
 
                    if (!data) {
                        $this.attr( { picture: "failed" } );
                        $test.attr( { status: "fail" } );
                        $q3_result.text( "(nothing returned; server down?)" );
                        
                    } else {
                        // Strip JSON formatting (if any)
                        //
                        //data= data.replace( /^"/, "" ).replace( /"\s*$/, "" );

                        $q3_result.text( data );
                        done_f();
                    }
                },
                error: function(_,c,d) {
                    $this.attr( "picture", "failed" );
                    $test.attr( { status: "fail" } );
                    $q3_result.text( "Failed: "+c+" "+d );
                }
            } );   
            
            if (use_q2) {
                // Start fetching q2 results
                //
                $.ajax( {
                    url: gateway_url( q2_url ),
                    type: "GET",
                    dataType: "text", 
                    cache: false,
                    success: function(data) {
                        // Seems to come here (with data=="") even in timeout
                        //
                        if (!data) {
                            $this.attr( { picture: "failed" } );
                            $test.attr( { status: "fail" } );
                            $ok.text( "(nothing returned; server down?)" );
                        } else {
                            $ok.text( data );
                            done_f();
                        }
                    },
                    error: function(_,c,d) {
                        $this.attr( { picture: "failed" } );
                        $test.attr( { status: "fail" } );
                        $ok.text( "Failed: "+c+" "+d );
                    }
                } );   
            }
        }

    } );
}


/*
* Init collapse button scripts
*/
function init_collapse_buttons( $set ) {

    $set.click( function() {
        var $this= $(this);
        var $test= $this.parent().parent();     // 'div[test]'
        
        $test.toggleClass( "collapsed" );
    } );
}


/*
* General page initialization
*/
$( function() {

    // Show a warning if opened via 'file://' with browser other than Safari.
    //
    if ((!USE_PHP) && (!$.browser.webkit)) {
        $("#use_via_server").fadeIn(3000);
    }

    $("div[test]").each( function() {
        var $this= $(this);     // div
        var test= $this.attr("test");       // i.e. "test1"

        test_init( $this, test );
    } );
    
    $("#q2_url").val( Q2_SERVER );
    $("#q3_url").val( Q3_SERVER );

    //--- 
    // Version indicator
    //    
    $("#q3_version").click( function() { check_version(); } );

    // Update the version field if '#q3_url' is edited
    //
    $("#q3_url").bind( "blur", function() {
        check_version();
    } );

    // Update once now and every N seconds after that
    //
    var me_f= function() {
        check_version();
        setTimeout( me_f, 20000 );
    };
    setTimeout( me_f, 0 );
    
    //---
    // "Run all" button functionality
    //
    $("#go_all").click( function() {
        if ($("#q3_version").hasClass("offline")) {
            alert( "q3 server is not running. Please select another server." );
        } else {
            $(".go_button").click();
        }
    } );
    
    //---
    // q3 presets functionality
    //
    $("#q3_presets").change( function() {
        var $selected_option = $(this).find(":selected");
        $("#q3_url").val( $selected_option.attr("x_url") );

        $("#q3_version").text("..checking version..");  // immediately show the new state
        check_version();            

        $("#q3_url").change();    // update each test's URLs
    } );
    
    $("#q3_presets option:first").attr( { selected: "1" } ); 
} );

