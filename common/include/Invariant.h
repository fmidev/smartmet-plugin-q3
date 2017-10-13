/*
* INVARIANT.H                   Copyright (c) 2008-2010, Ilmatieteen laitos
*
* Documentary INVARIANT function to classes.
*/
#ifndef INVARIANT_H
#define INVARIANT_H

/*
* Use 'INVARIANT()' whenever an object changes its state.
*/
#ifdef NDEBUG
# define INVARIANT() /*nothing*/
#else
# define INVARIANT() _INVARIANT(__FILE__,__LINE__)
#endif

#endif
    // INVARIANT_H
