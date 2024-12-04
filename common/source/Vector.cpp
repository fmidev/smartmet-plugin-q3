/*
* VECTOR.CPP                          Copyright (c) 2009-10, Ilmatieteen laitos
*/
#include "Vector.h"

#include <cstring>
#include <math.h>

#include <iostream>
    // debug
    
using namespace std;

LuaNew_ID VectorBind::ID;

const auto Inf = Vector( INFINITY, 0.0, true );		// result of divide-by-zero


/*---=== VectorBind ===---*/

/*
* Set up metatable.
*/
void VectorBind::setup( lua_State *L ) {

    assert( lua_istable(L,-1) );

    // Metamethods
    //
    lua_pushliteral(L,"__eq");
    lua_pushcfunction(L,eq);
    lua_settable(L,-3);

    lua_pushliteral(L,"__lt");
    lua_pushcfunction(L,lt);
    lua_settable(L,-3);

    lua_pushliteral(L,"__index");
    lua_pushcfunction(L,index);
    lua_settable(L,-3);

    lua_pushliteral(L,"__unm");
    lua_pushcfunction(L,unm);
    lua_settable(L,-3);

    lua_pushliteral(L,"__add");
    lua_pushcfunction(L,add);
    lua_settable(L,-3);

    lua_pushliteral(L,"__mul");
    lua_pushcfunction(L,mul);
    lua_settable(L,-3);

    lua_pushliteral(L,"__div");
    lua_pushcfunction(L,div);
    lua_settable(L,-3);

    lua_pushliteral(L,"__tostring"); 
    lua_pushcfunction(L,tostring);
    lua_settable(L,-3);
}

/*
* bool= __eq( vector_ud, any )
* bool= __eq( any, vector_ud )
*
* Return 'true' if both parameters are vectors and their value is same.
*
* Note: This method is not really needed, but it may be expected by some (to be able
*       to compare vectors by value). Without this, Lua compares userdata references,
*       not their values (only the object itself is equal to itself).
*
* NOTE: Equality of floating point numbers should ALWAYS be used with care
*       (or rather not used, at all).
*/
int VectorBind::eq( lua_State *L ) {
    const Vector *a= Vector::instance(L,1);
    const Vector *b= Vector::instance(L,2);
    bool ret= false;

    if (a && b) {
        // Doing it like this helps keep unnecessary inaccuracies away
        //
        if (a->isPolar()) {
            ret= (a->getAbs()==b->getAbs()) && (a->getDeg()==b->getDeg());
        } else {
            ret= (a->getX()==b->getX()) && (a->getY()==b->getY());
        }
    }
    lua_pushboolean( L, ret );
    return 1;
}

/*
* bool= __lt( vector_ud, any )
* bool= __lt( any, vector_ud )
*
* Return 'true' if first vector is less than second (by its absolute value).
*
* Note: Two vectors with same absolute strength but different angles are deemed not-less-than
*       to each other. Still, they are not equal either. In other words, this function
*       cannot be used for sorting.
*
* Note: How exactly vectors are placed in order (and whether an order is required, at all)
*       is arguable. This feature is here mainly for MAXZ et.al. to be able to find strongest
*       values also in vector fields.
*/
int VectorBind::lt( lua_State *L ) {
    const Vector *a= Vector::instance(L,1);
    const Vector *b= Vector::instance(L,2);
    bool ret;

    if (a && b) {
        ret= a->getAbs() < b->getAbs();
    } else {
        luaL_error( L, "Cannot compare %s and %s", L_typename(1), L_typename(2) );
        ret= false;  // never (pleases the compiler)
    }
    lua_pushboolean( L, ret );
    return 1;
}

/*
* num= __index( vector_ud, "x" )
* num= __index( vector_ud, "y" )
* num= __index( vector_ud, "abs" )
* num= __index( vector_ud, "dir" )
* bool= __index( vector_ud, "isnan" )
*/
int VectorBind::index( lua_State *L ) {
    const Vector &my= *Vector::instance(L,1);
    double v;

    const char *s= lua_tostring(L,2);
    if (!s) {
        luaL_error( L, "Bad index (for vector): %s", L_typename(2) );
    }

    if (strcmp(s,"x")==0) {
        v= my.getX();
    }
    else if (strcmp(s,"y")==0) {
        v= my.getY();
    }
    else if (strcmp(s,"abs")==0) {
        v= my.getAbs();
    }
    else if (strcmp(s,"deg")==0) {
        v= my.getDeg();
    }
    else if (strcmp(s,"isnan")==0) {
        lua_pushboolean( L, my.isnan() );
        return 1;
    }
    else {
        luaL_error( L, "Bad index (for vector): %s", s );
        v= 0.0;    // never
    }

    lua_pushnumber( L, v );
    return 1;
}


/*
* vector_ud= __unm( vector_ud )
*/
int VectorBind::unm( lua_State *L ) {
    Vector &a= *Vector::instance(L,1);
    new(L) Vector( -a );
    return 1;
}


/*
* vector_ud= __add( vector_ud, vector_ud|(any) )
* vector_ud= __add( vector_ud|(any), vector_ud )
*/
int VectorBind::add( lua_State *L ) {
    Vector *a= Vector::instance(L,1);
    Vector *b= Vector::instance(L,2);

    if ((!a) || (!b)) {
        luaL_error( L, "Cannot add %s and %s", L_typename(1), L_typename(2) );
    }
    new(L) Vector( *a + *b );
    return 1;
}


/*
* vector_ud= __sub( vector_ud, vector_ud|(any) )
* vector_ud= __sub( vector_ud|(any), vector_ud )
*/
int VectorBind::sub( lua_State *L ) {
    Vector *a= Vector::instance(L,1);
    Vector *b= Vector::instance(L,2);

    if ((!a) || (!b)) {
        luaL_error( L, "Cannot subtract %s and %s", L_typename(1), L_typename(2) );
    }
    new(L) Vector( *a + (-*b) );      // bypassing need for a subtract operation
    return 1;
}


/*
* vector_ud= __mul( vector_ud, num|(any) )
* vector_ud= __mul( num|(any), vector_ud )
*/
int VectorBind::mul( lua_State *L ) {
    int b_index=2;
    Vector *a= Vector::instance(L,1);
    if (!a) {
        a= Vector::instance(L,2);
        b_index= 1;
    }
    L_ASSERT(a);

    if (!lua_isnumber(L,b_index)) {
        luaL_error( L, "Cannot multiply %s and %s", L_typename(1), L_typename(2) );
    }

    new(L) Vector( (*a) * lua_tonumber(L,b_index) );
    return 1;
}

/*
* vector_ud= __div( vector_ud, num|(any) )
* vector_ud= __div( (any), vector_ud )      -- this is illegal, but will take us here
*/
int VectorBind::div( lua_State *L ) {
    Vector *a= Vector::instance(L,1);
    if ((!a) || (!lua_isnumber(L,2))) {
        luaL_error( L, "Cannot divide %s and %s", L_typename(1), L_typename(2) );
    }

    double d= lua_tonumber(L,2);
    if (d==0.0) {
        luaL_error( L, "Trying to divide by zero" );
    }

    new(L) Vector( (*a) * (1.0/d) );    // bypassing need for div operation
    return 1;
}

/*
* str= mt.__tostring( obj )
*
* Note: 'RegTools' decimals and binary setting only affect matrix output, not us
*       (we output in full accuracy, always).
*/
int VectorBind::tostring( lua_State *L ) {

    // Note: Malign client scripts can call this function with surprising parameters;
    //      make sure we don't crash.
    //
    const Vector &me= *Vector::instance(L,1);

    if (!&me) {
        throw E_LOG_USAGE( "Bad parameter: %s", L_typename(1) );    // internal bug or deliberate hack attempt
    }

    // Output all as cartesian
    //
    // Note: We do lose values in the case polar coordinates would have either part NAN (but the other not).
    //       Outputting these in cartesian will make it all look like NAN.
    //
    lua_pushfstring( L, "(%f %f)", me.getX(), me.getY() );
    return 1;
}


/*---=== Vector ===---*/

/*
* For polar coordinates, degs are converted to rads and normalized to certain range.
*
* Values are adjusted to be within the [0,inf) and [0,2*M_PI) ranges. Doing it here 
* makes sure all calculations will automatically get their values rightly adjusted
* (i.e. multiplication by negatives will turn the direction around).
*/
Vector::Vector( double x_or_abs, double y_or_deg, bool polar_ ) noexcept
    : v1(x_or_abs), v2(y_or_deg), polar(polar_) {

    if (polar_) {
        // Calculate using 'double' to keep conversion inaccuracies minimal.
        //
        double rad= ((double)v2) * M_PI/180.0;   // deg to rad
 
        if (v1<0.0f) {
            v1= -v1;
            rad += M_PI;     // turns the direction around
        }

        // Note: 'fmod()' works both sides of zero; the remainder may be negative
        //
        rad= fmod( rad, 2.0*M_PI );   // place in range

        // Convert back to float *before* doing the final adjustment. Otherwise we
        // get sometimes '2.0*pi' values because of double/float conversion issues
        // (when value is very slightly negative).
        //
        v2= (float)rad;        // converts very slightly negative double to 0.0 as float

        if (v2<0.0f) {
            v2 += 2.0*M_PI;    // now within [0, 2pi) range
        }
    }

    INVARIANT();
}

/*
* Return the direction of the vector, in [0,360) range (0=North, grows clockwise).
*
* Note: Instead of doing special if/else for the NAN case, we make sure it flows through
*       the regular calculations, producing NAN at the end (this is faster).
*/
double Vector::getDeg() const {
    double deg;

    if (polar) {
        assert( isnanf(v2) || (v2>=0.0 && v2 <2.0*M_PI) );
        deg= v2*(180.0/M_PI); 

    } else if ((v1==0.0) && (v2==0.0)) {
        // give 0.0 for null vector ('atan2()' would give 180.0)
        deg= 0.0;

    } else {
//LOG_DEBUG( "x: %f y: %f", (double)v1, (double)v2 );

        // 'atan2()' value range is (-pi,pi] (latter inclusive).
        //
        double rad= atan2(v2,v1);       // (-pi,pi] (0=East, grows anticlockwise)

        // 1. Adding pi/2 turns 0 to South.     (-pi/2,3/2*pi]
        // 2. reducing from pi turns 0 to North and swaps to clockwise growth.  [-3/2*pi, pi/2)
        // 3. adding 2*pi and applying 'fmod()' takes to [0,2*pi) range.
        // 4. scaling to degrees
        //
        //deg= fmod( 2.0*M_PI + (M_PI - (rad+M_PI/2.0)), 2.0*M_PI ) * (180.0/M_PI);
        
        deg= fmod( 5.0/2.0*M_PI -rad, 2.0*M_PI ) * (180.0/M_PI);      // reduced

//LOG_DEBUG( "rad: %f deg: %f", rad, deg );
    }
    
    assert( isnanf(deg) || ((deg>=0.0) && (deg<360.0)) );

    return deg;
}

/*
*/
Vector Vector::operator - () const {
    if (!polar) {
        return Vector( -v1, -v2, false );
    } else {
        return Vector( -v1, v2, true );     // will normalize into angle turn instead
    }
}

/*
* Addition converts polar vectors into cartesian (as it should, and must,
* addition only makes sense in cartesian coordinates).
*/
Vector Vector::operator + ( const Vector& o ) const {
    return Vector( getX()+o.getX(), getY()+o.getY(), false );
}

/*
*/
Vector Vector::operator * ( double d ) const {
    if (!polar) {
        return Vector( v1*d, v2*d, false );
    } else {
        return Vector( v1*d, v2, true );    // constructor will turn the direction if 'v' is negative
    }
}


#ifndef NDEBUG
void Vector::selftest() {
    const double EPS= 1e-6;

    Vector east( 1.0, 0.0, false );
    Vector west( -1.0, 0.0, false );
    Vector north( 0.0, 1.0, false );
    Vector south( 0.0, -1.0, false );
    Vector southeast( 1.0, -1.0, false );
    Vector southwest( -1.0, -1.0, false );
    Vector northeast( 1.0, 1.0, false );
    Vector northwest( -1.0, 1.0, false );
    
    assert( fabs( north.getDeg() - 0.0 ) < EPS );
    assert( fabs( northeast.getDeg() - 45.0) < EPS );
    assert( fabs( east.getDeg() - 90.0) < EPS );
    assert( fabs( southeast.getDeg() - 135.0) < EPS );
    assert( fabs( south.getDeg() - 180.0) < EPS );
    assert( fabs( southwest.getDeg() - 225.0) < EPS );
    assert( fabs( west.getDeg() - 270.0) < EPS );
    assert( fabs( northwest.getDeg() - 315.0) < EPS );
    
    LOG_OK0( "Vector selftest passed." );
}
#endif


