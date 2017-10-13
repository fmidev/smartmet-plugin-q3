/*
* DIFFER.JS
*
* Find differences between q3 and q2 output.
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

/*global $, alert, console */

var differ_matrices;     // global names
var differ_json;

/*
* Wrapper for keeping things local
*/
( function() {

    var RED_LIMIT= 0.03;    // relative difference bigger than this gives "fail" (otherwise "almost")

    var EPSILON= 1e-6;

    var Math_abs= Math.abs;

    /*
    * bool= differ_ignoring_whitespace( a_str, b_str )
    */
    function differ_ignoring_whitespace( a, b ) {
        return a.replace("\s","") !== b.replace("\s","");
    }


    /*
    * Create '<span class='xxx'>zzz</span>' elements.
    */
    function span( str, which ) {
        return "<span class='"+which+"'>"+str+"</span>";
    }
    function span_red( str ) {
        return span( str, "red" );
    }
    function span_yellow( str ) {
        return span( str, "yellow" );
    }

    /*
    * [ "ok"|"almost"|"fail"
    *   , a_html
    *   , b_html ]= differ_numbers( [ number|"", ... ], [ number|"", ... ] )
    *
    * Find differences in the two arrays of numbers (equally long). Mark slight differences
    * with "<span class='yellow'>" and full differences with "<span class='red'>".
    */
    function differ_numbers( a, b ) {
        var a_html= [];
        var b_html= [];
        var st= "ok";

        for( var i=0; i<a.length; i++ ) {
            var va= a[i];
            var vb= b[i];

            // Coat the 'va' and/or 'vb' with span if needed.
            //
            var span_x= null;

            if ((va==="") || (vb==="")) {   // either one or both are NAN
                if (va || vb) { span= "red"; }

            } else if (va !== vb) {
                var va_num= parseFloat(va);
                var vb_num= parseFloat(vb);

                // Do the values differ considerably, or just a bit?
                //
                var rel_diff= Math_abs( (va_num-vb_num) / ((va_num+vb_num)/2) );

//console.log( va, vb, abs_diff, rel_diff );

                span_x= (rel_diff > RED_LIMIT) ? span_red : span_yellow;
            }

            if (span_x) {
                va= span_x( va );
                vb= span_x( vb );
                
                if ((st=="ok") || (span_x==span_red)) {
                    st= (span_x==span_red) ? "fail":"almost";
                }
            }

            a_html[ a_html.length ]= va;
            b_html[ b_html.length ]= vb;
        }

        return [ st, a_html.join(","), b_html.join(",") ];
    }

    /*
    * [ "ok"|"almost"|"fail"
    *   ,a_highlighted_html
    *   ,b_highlighted_html ]= differ_matrix_one( a_str, b_str )
    *
    * 'a' and 'b' are matrix lines ("x,y;num,...")
    *
    * Compare the lines are return their differences highlighted.
    */
    function differ_matrix_one( a, b ) {

        var a_arr= a.split(";");
            //
            // [0]: "xx,yy"
            // [1]: "number,..."

        var b_arr= b.split(";");

        // Dimensions need to be the same, naturally.
        //
        if (a_arr[0] != b_arr[0]) {
            var a_html= span_red( a_arr[0] )+";"+a_arr[1];
            var b_html= span_red( b_arr[0] )+";"+b_arr[1];

            return [ "fail", a_html, b_html ];
        }

        // Dimensions are same. Find differences in the numbers (if any)
        //
        var arr= differ_numbers( a_arr[1].split(","), b_arr[1].split(",") );
            //
            // [0]: "ok"|"almost"|"fail"
            // [1]: 'a' numbers with highlight (string)
            // [2]: 'b' numbers with highlight (string)
                
        var st= arr[0];
        var a_nums_html= arr[1];
        var b_nums_html= arr[2];
        
        return [ st, a_arr[0]+";"+a_nums_html, b_arr[0]+";"+b_nums_html ];
    }

    /*
    * [ "ok"|"almost"|"fail"
    *   ,a_highlighted_html
    *   ,b_highlighted_html ]= differ_json_entry( a_str, b_str )
    *
    * 'a' and 'b' are comma-delimited parts of a JSON string (not carrying commas, themselves).
    *
    * i.e. '[ [ "asdsadsad"', " 3.34342 ]"
    *
    * The white space and/or '['']' around the inner strings must match precisely. What's left within
    * (if a number) is compared with the 3% rule
    */
    function differ_json_entry( a, b ) {
    
        var re= /^([\s\[]*)(.+?)([\s\]]*)$/;    // note: '+?' is a non-eager capture

        var a_arr= a.match(re);      
        var b_arr= b.match(re);
            //
            // [0]: (ignore)
            // [1]: header ("" if none)
            // [2]: actual part
            // [3]: trailer ("" if none)
    
        var span_x= null;

//console.log( a, a_arr );
//console.log( b, b_arr );

        if (differ_ignoring_whitespace( a_arr[1], b_arr[1] ) ||
            differ_ignoring_whitespace( a_arr[3], b_arr[3] )) {

            return [ "fail", span_red(a), span_red(b) ];

        } else {
            var va= parseFloat(a_arr[2]);   // NaN if didn't start as a number
            if (!isNaN(va)) {
                var vb= parseFloat(b_arr[2]);
                var rel_diff= Math_abs( (va-vb) / ((va+vb)/2) );
                
                if (rel_diff > EPSILON) {
                    span_x= (rel_diff > RED_LIMIT) ? span_red : span_yellow;
                }
            } else {
                // strings or something (but header and trailer match
                //
                if (a_arr[2] !== b_arr[2]) {
                    span_x= span_red;
                }
            }
        }

        if (!span_x) {        
            return [ "ok", a,b ];
        } else {
            
            if (span_x==span_red) {
                alert( "SPAN RED for: "+a+" vs. "+b );
            }

            return [ (span_x==span_red) ? "fail":"almost", 
                     a_arr[1]+span_x(a_arr[2])+a_arr[3],
                     b_arr[1]+span_x(b_arr[2])+b_arr[3] ];
        }
    }

    /*
    * [ "ok"|"almost"|"fail"
    *   ,null | a_highlighted_html
    *   ,null | b_highlighted_html ]= differ_json_one( a_str, b_str )
    *
    * 'a' and 'b' are JSON lines (i.e. "[[ "aaa", 3.343434 ], ... ]")
    *
    * Note: We do not intend to do full JSON syntax parsing. Only what's needed for highlighting
    *       the actual test cases (enhance this when/if needed).
    *
    * Compare the lines are return their differences highlighted.
    */
    function differ_json_one( a, b ) {

        var a_arr= a.split(",");
        var b_arr= b.split(",");

        if ((!a_arr) || (!b_arr) || (a_arr.length != b_arr.length)) {
            return [ (a===b) ? "ok":"fail" ];
        }

        // Both 'a' and 'b' had the same number of commas (>=1)
        //
        var a_html= [];
        var b_html= [];
        var st= "ok";

        for( var i=0; i<a_arr.length; i++ ) {
            var arr= differ_json_entry( a_arr[i], b_arr[i] );
                //
                // [0]: "ok"|"almost"|"fail"
                // [1]: 'a' entry with highlight
                // [2]: 'b' entry with highlight

            if ((st=="ok") || (arr[0]=="fail")) {
                st= arr[0];
            }
                
            a_html[i] = arr[1];
            b_html[i] = arr[2];
        }

        return [ st, a_html.join(","), b_html.join(",") ];
    }


    /*
    * differ_all_func= differ_x( differ_one_func )
    *
    * [ "ok"|"almost"|"fail", q3_highlight_html, q2_highlight_html ]= differ_one_func( q3_line_str, q2_line_str )
    *
    * [ "ok"|"almost"|"fail"
    *   ,null | q3_output_highlighted_html
    *   ,null | q2_output_highlighted_html ]= differ_all_func( q3_output_str, q2_output_str )
    *
    * This is a 'factory' for 'differ_matrices' and 'differ_json', which are the same except
    * for the function to use for the differentiation of each line.
    */
    function differ_x( differ_one_f ) {
        return function( q3_output, q2_output ) {
        
            // If both have equal number of lines and begin with 'xx,yy;' of same proportions, do detailed
            // comparison.
            //
            var lines_q3= q3_output.split( "\n" );
            var lines_q2= q2_output.split( "\n" );
            
            if (lines_q3.length == lines_q2.length) {
                var st= "ok";
                var html_q3= "";
                var html_q2= "";
    
                for (var i=0; i<lines_q3.length; i++) {
                    var arr= differ_one_f( lines_q3[i], lines_q2[i] );
                        //
                        // [0]: "ok"|"almost"|"fail"
                        // [1]: first (q3) matrix with color highlight for diffs
                        // [2]: second (q2) matrix with color highlight for diffs
                        
                    if ((st=="ok") || (arr[0]=="fail")) {
                        st= arr[0];     // lower our equality rating
                    }
                    
                    html_q3 += arr[1]+"\n";
                    html_q2 += arr[2]+"\n";
                }
                
                return [ st, html_q3, html_q2 ];
            }

            return [ "fail" ];
        };
    }

    differ_matrices= differ_x( differ_matrix_one );
    differ_json= differ_x( differ_json_one );

} )();  // execute the scope

