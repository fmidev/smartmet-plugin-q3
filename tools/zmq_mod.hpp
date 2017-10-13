/*
* Enhanced version of ZMQ C++ header - to be pushed upstream.
*   --AKa 18-Jan-10
*
* Enhancements:
*   - better error messages
*   - enums instead of flag ints
*   - set methods instead of use of enums or ints
*   - use of 'zmq::pollitem' instead of C 'zmq_pollitem_t' (more covering up C details)
*   - error handling for 'zmq::poll()' (was missing!!)
*   - added 'const' to methods not changing the object
*   - 'zmq::socket_t' changed to 'zmq::socket' (and particular variants)
*   - ...
*
* Note:
*   - using 'inline' is unnecessary if the body of the function is there:
*       <http://www.parashift.com/c++-faq-lite/inline-functions.html#faq-9.8>
*/

/*
    Copyright (c) 2007-2010 iMatix Corporation

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the Lesser GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __ZMQ_HPP_INCLUDED__
#define __ZMQ_HPP_INCLUDED__

#include "zmq.h"

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdexcept>
#include <cstdarg>

namespace zmq
{
    typedef zmq_free_fn free_fn;

    std::string string_fmt( const char *fmt, ... ) {
        char buf[4000];
        va_list vl;
        va_start( vl, fmt );
        {    
            vsnprintf( buf, sizeof(buf), fmt, vl );
        }
        va_end( vl );
        buf[sizeof(buf)-1]= '\0';   // make sure it's always terminated
        return buf;
    }

    /*
    */
    class error : public std::exception
    {
    public:
        error( const char *func_name ) 
            : msg( string_fmt("error in %s: %s", func_name, zmq_strerror(errno) )) {}

        /*virtual*/ ~error() throw() {}

        // The returned pointer will be valid only as long as our exception 
        // object is.
        //
        /*virtual*/ const char *what () const throw ()
        {
            return msg.c_str();
        }

    private:
        const std::string msg;
        //const int errnum;
    };

    /*
    * Note: 'pollitem' must NOT have any extra members. It's size must be same as 'zmq_pollitem_t's.
    */
    struct pollitem : private zmq_pollitem_t { 
        // void *socket; 
        // int fd; 
        // short events; 
        // short revents; 

        // Q: Should both polling in _and_ out be allowed for the same socket?
        //    If only one, then we can simplify this by splitting to 'pollitem_in' and
        //    'pollitem_out' constructors, and having only one 'happened()' output.
        //
        enum e_inout {
            POLLIN= ZMQ_POLLIN,     // poll for incoming messages
            POLLOUT= ZMQ_POLLOUT    // poll will return if a message of at least one byte can be written to the socket.
        };

        pollitem( void *socket_, enum e_inout e ) {
            socket= socket_;
            fd= 0;
            events= e;
            revents= 0;
        }

        pollitem( int fd_, enum e_inout e ) {
            socket= NULL;
            fd= fd_;
            events= e;
            revents= 0;
        }
        
        enum e_inout happened() const {
            return (enum e_inout)revents;
        }
    };

    // This will ensure that 'sizeof(pollitem) == sizeof(zmq_pollitem_t)'
#if 1
    class _just_once {
      public:
        _just_once() {
            if (sizeof(pollitem) != sizeof(zmq_pollitem_t)) {
                throw std::runtime_error( "sizeof(pollitem) != sizeof(zmq_pollitem_t)" );
            }
        }
    };
    static _just_once jo;
#endif

    /*
    * Returns: Number of items signaled.
    *
    * Sample:
    *       zmq::pollitem items[]= {
    *           zmq::pollitem( s, zmq::POLLIN ),
    *           zmq::pollitem( my_fd, zmq::POLLIN )
    *       };
    *       unsigned n= zmq::poll( items, 2 );
    */
    unsigned poll (pollitem *items_, int nitems_, long timeout_us_ = -1)
    {
        int rc= zmq_poll ( (zmq_pollitem_t*)items_, nitems_, timeout_us_);
        if (rc<0) {
            // EFAULT - 0MQ socket in the pollset belonging to a different application thread.
            // ENOTSUP - 0MQ context was initialised without ZMQ_POLL flag. I/O multiplexing is disabled.
            throw error("zmq_poll");
        }
        return rc;
    }

    /*
    */
    class message : private zmq_msg_t
    {
        friend class socket_base;

    public:
        message () { init(); }
        message (size_t size_) { init(size_); }
        message (void *data_, size_t size_, free_fn *ffn_, void *hint_ = NULL) {
            init(data_, size_, ffn_, hint_);
        }

        ~message ()
        {
            close();
        }

        void rebuild ()
        {
            close(); init();
        }

        void rebuild (size_t size_)
        {
            close(); init(size_);
        }

        void rebuild (void *data_, size_t size_, free_fn *ffn_,
            void *hint_ = NULL)
        {
            close(); init(data_, size_, ffn_, hint_);
        }

        void move (message *msg_)
        {
            int rc = zmq_msg_move (this, (zmq_msg_t*) msg_);
            assert(rc==0); (void)rc;
        }

        void copy (message *msg_) /*const*/
        {
            int rc = zmq_msg_copy (this, (zmq_msg_t*) msg_);
            assert(rc==0); (void)rc;
        }

        void *data ()
        {
            return zmq_msg_data (this);
        }

        size_t size () /*const*/
        {
            return zmq_msg_size (this);
        }

    private:
        void init() {
            int rc = zmq_msg_init (this);
            assert(rc==0); (void)rc;
        }

        void init (size_t size_) {
            int rc = zmq_msg_init_size (this, size_);
            if (rc == ENOMEM) {
                throw error("zmq_msg_init_size");
            }
            assert( rc==0 ); (void)rc;
        }

        void init (void *data_, size_t size_, free_fn *ffn_, void *hint_)
        {
            int rc = zmq_msg_init_data (this, data_, size_, ffn_, hint_);
            assert(rc==0); (void)rc;
        }

        void close() {
            int rc = zmq_msg_close (this);
            assert(rc==0); (void)rc;
        }

        //  Disable implicit message copying, so that users won't use shared
        //  messages (less efficient) without being aware of the fact.
        message(const message&);
        void operator = (const message&);
    };

    /*
    */
    class context
    {
        friend class socket_base;

    public:
        enum e_flags {
            POLL= ZMQ_POLL,
            POLLABLE= POLL      // alias (more descriptive)
        };

        context (unsigned app_threads_, unsigned io_threads_, enum e_flags e= (enum e_flags)0 )
        {
            ptr = zmq_init (app_threads_, io_threads_, (int)e);
            if (ptr == NULL) {
                // EINVAL: less than one application thread (or number of I/O threads is negative)
                throw error("zmq_init");
            }
        }

        ~context ()
        {
            int rc = zmq_term (ptr);
            assert (rc == 0); (void)rc;
        }

    private:
        void *ptr;

        context (const context&);
        void operator = (const context&);
    };

    /*
    * Internal base class for all the sockets
    */
    class socket_base
    {
      protected:
      /*
        enum e_type {
            // Peer to peer:
            //
            P2P= ZMQ_P2P,   // communicate with single peer (who'se also P2P)

            // Publish / subscribe:
            //
            PUB= ZMQ_PUB,   // publish data (send only) to 0..N peers (who are ZMQ_SUB)
            SUB= ZMQ_SUB,   // subscribe data (receive only) from 0..N peers (who are ZMQ_PUB)
            
            // Request / reply:
            //
            REQ= ZMQ_REQ,   // send requests and receive replies (alternated send/rec)
                            // peers: REP, XREP
            REP= ZMQ_REP,   // receive requests and send replies (alternated rec/send)
                            // peers: REQ, XREQ

            // Request/reply middleboxes:
            //
            XREQ= ZMQ_XREQ, // peers: REP, XREP
            XREP= ZMQ_XREP, // peers: REQ, XREQ

            // Downstream/Upstream:
            //
            DOWNSTREAM= ZMQ_DOWNSTREAM  // (send only) peers: UPSTREAM
            UPSTREAM= ZMQ_UPSTREAM,     // (receive only) peers: DOWNSTREAM
        };
        */

        socket_base( context &context_, int type_ )
        {
            ptr = zmq_socket (context_.ptr, type_);
            if (ptr == NULL) {
                // (EINVAL -  invalid socket type)
                // EMTHREAD - number of application threads allowed to own 0MQ sockets was exceeded. 
                //            See app_threads parameter to zmq_init function. 
                //
                throw error("zmq_socket");
            }
        }

    public:
        ~socket_base ()
        {
            int rc = zmq_close (ptr);
            assert(rc==0); (void)rc;
        }

        operator void* ()
        {
            return ptr;
        }

        socket_base &set_water_marks( size_t hwm_bytes, size_t lwm_bytes=0 ) {
            int64_t v= hwm_bytes;
            setsockopt( ZMQ_HWM, &v, sizeof(v) );
            v= lwm_bytes;
            setsockopt( ZMQ_LWM, &v, sizeof(v) );
            return *this;
        }

        socket_base &set_swap( size_t bytes ) {
            int64_t v= bytes;
            setsockopt( ZMQ_SWAP, &v, sizeof(v) );
            return *this;
        }

        socket_base &set_affinity( unsigned bits ) {
            int64_t v= bits;
            setsockopt( ZMQ_AFFINITY, &v, sizeof(v) );
            return *this;
        }

        socket_base &set_identity( const char *s ) {
            setsockopt( ZMQ_IDENTITY, s, s ? strlen(s):0 );
            return *this;
        }

        // Q: Should this be under receive-only socket?
        //
        socket_base &set_multicast_loop( bool enable ) {
            uint64_t v= enable ? 1:0;
            setsockopt( ZMQ_MCAST_LOOP, &v, sizeof(v) );
            return *this;
        }

        void bind (const char *addr_)
        {
            int rc = zmq_bind (ptr, addr_);
            if (rc != 0) {
                // EPROTONOSUPPORT  - unsupported protocol
                // ENOCOMPATPROTO   - protocol not compatible with socket type 
                // EADDRINUSE       - address already in use
                // EADDRNOTAVAIL    - nonexisting interface or non-local address
                //
                throw error("zmq_bind");
            }
        }

        void connect (const char *addr_)
        {
            int rc = zmq_connect (ptr, addr_);
            if (rc != 0) {
                // EPROTONOSUPPORT - unsupported protocol
                // ENOCOMPATPROTO - protocol not compatible with socket type 
                //
                throw error("zmq_connect");
            }
        }

    protected:
    /*
        enum e_option {
            HWM=        ZMQ_HWM,            // High watermark for the message pipes associated with the socket. (bytes)
            LWM=        ZMQ_LWM,
            SWAP=       ZMQ_SWAP,
            AFFINITY=   ZMQ_AFFINITY,
            IDENTITY=   ZMQ_IDENTITY,
            SUBSCRIBE=  ZMQ_SUBSCRIBE,
            UNSUBSCRIBE= ZMQ_UNSUBSCRIBE,
            RATE=       ZMQ_RATE,
            RECOVERY_IVL= ZMQ_RECOVERY_IVL,
            MCAST_LOOP= ZMQ_MCAST_LOOP,
            SNDBUF=     ZMQ_SNDBUF,
            RCVBUF=     ZMQ_RCVBUF
        };
    */
        void setsockopt (int option_, const void *optval_, size_t optvallen_)
        {
            int rc = zmq_setsockopt (ptr, option_, optval_, optvallen_);
            if (rc != 0)
                // EINVAL - unknown option, a value with incorrect length or invalid value.
                throw error("zmq_setsockopt");
        }

        void set_receive_bufsize( size_t bytes ) {
            uint64_t v= bytes;
            setsockopt( ZMQ_SNDBUF, &v, sizeof(v) );
        }

        void set_send_bufsize( size_t bytes ) {
            uint64_t v= bytes;
            setsockopt( ZMQ_RCVBUF, &v, sizeof(v) );
        }

        bool receive (message *msg_, int flags)
        {
            int rc = zmq_recv (ptr, msg_, flags);
            if (rc == 0) {
                return true;
            } else if (errno == EAGAIN) {
                return false;
            }
            throw error("zmq_recv");
        }

        bool send (message &msg_, int flags)
        {
            int rc = zmq_send (ptr, &msg_, flags);
            if (rc == 0) {
                return true;
            } else if (errno == EAGAIN) {
                return false;   // 'flags' had NOBLOCK
            }

            // (ENOTSUP - not supported by socket type)
            // EFSM - cannot be called at the moment, because socket is not in the approprite state
            //
            throw error("zmq_send");
        }

        void flush ()
        {
            int rc = zmq_flush (ptr);
            if (rc != 0) {
                // (ENOTSUP - not supported by socket type)
                // EFSM - cannot be called at the moment, because socket is not in the approprite state
                //
                throw error("zmq_flush");
            }
        }

    private:
        void *ptr;

        socket_base(const socket_base&);
        void operator = (const socket_base&);
    };
    
    /*
    * 
    */
    class socket_w : public socket_base {
      protected:
        socket_w( context &ctx, int type_ ) : socket_base(ctx,type_) {}

      public:
        enum e_flags {
            NOBLOCK=    ZMQ_NOBLOCK,    // operation should be performed in non-blocking mode
            NOFLUSH=    ZMQ_NOFLUSH     // wait for ':flush()' before sending this message 
        };

        socket_w &set_rate_kbps( unsigned rate ) {
            uint64_t v= rate;
            setsockopt( ZMQ_RATE, &v, sizeof(v) );
            return *this;
        }

        socket_w &set_send_bufsize( size_t bytes ) {
            socket_base::set_send_bufsize( bytes );
            return *this;
        }

        bool send (message &msg_, enum e_flags flags= (enum e_flags)0) {
            return socket_base::send( msg_, (int)flags );
        }
        
        void flush() {
            socket_base::flush();
        }

      private:
    };
    
    /*
    */
    class socket_r : public socket_base {
      protected:
        socket_r( context &ctx, int type_ ) : socket_base(ctx,type_) {}

      public:
        enum e_flags {
            NOBLOCK=    ZMQ_NOBLOCK,    // operation should be performed in non-blocking mode
        };

        // Q: Should this be under receive-only socket, or somewhere else?
        //
        socket_r &set_recovery_ivl( unsigned secs ) {
            uint64_t v= secs;
            setsockopt( ZMQ_RECOVERY_IVL, &v, sizeof(v) );
            return *this;
        }

        socket_r &set_receive_bufsize( size_t bytes ) {
            socket_base::set_receive_bufsize( bytes );
            return *this;
        }

        bool receive (message *msg_, enum e_flags flags= (enum e_flags)0)
        {
            return socket_base::receive( msg_, (int)flags );
        }
        
      private:
    };

    /*
    */
    class socket_rw : public socket_w {
      protected:
        socket_rw( context &ctx, int type_ ) : socket_w(ctx,type_) {}

      public:
        socket_rw &set_receive_bufsize( size_t bytes ) {
            socket_base::set_receive_bufsize( bytes );
            return *this;
        }

        bool receive (message *msg_, enum e_flags flags= (enum e_flags)0)
        {
            return socket_base::receive( msg_, (int)flags );
        }
    };
    
    /*
    */
    class socket_p2p : public socket_rw {
      public:
        socket_p2p( context &ctx ) : socket_rw( ctx, ZMQ_P2P ) {}
    };

    /*
    */
    class socket_pub : public socket_w {
      public:
        socket_pub( context &ctx ) : socket_w( ctx, ZMQ_PUB ) {}
    };

    /*
    */
    class socket_sub : public socket_r {
      public:
        socket_sub( context &ctx ) : socket_r( ctx, ZMQ_SUB ) {}

        socket_sub &subscribe( const char *filter ) {
            setsockopt( ZMQ_SUBSCRIBE, (void*)filter, filter ? strlen(filter):0 );
            return *this;
        }
        socket_sub &unsubscribe( const char *filter ) {
            setsockopt( ZMQ_UNSUBSCRIBE, (void*)filter, filter ? strlen(filter):0 );
            return *this;
        }
    };

    /*
    */
    class socket_req : public socket_rw {
      public:
        socket_req( context &ctx ) : socket_rw( ctx, ZMQ_REQ ) {}
    };

    /*
    */
    class socket_rep : public socket_rw {
      public:
        socket_rep( context &ctx ) : socket_rw( ctx, ZMQ_REP ) {}
    };

    /*
    */
    class socket_xreq : public socket_rw {
      public:
        socket_xreq( context &ctx ) : socket_rw( ctx, ZMQ_XREQ ) {}
    };

    /*
    */
    class socket_xrep : public socket_rw {
      public:
        socket_xrep( context &ctx ) : socket_rw( ctx, ZMQ_XREP ) {}
    };

    /*
    */
    class socket_downstream : public socket_w {
      public:
        socket_downstream( context &ctx ) : socket_w( ctx, ZMQ_DOWNSTREAM ) {}
    };

    /*
    */
    class socket_upstream : public socket_r {
      public:
        socket_upstream( context &ctx ) : socket_r( ctx, ZMQ_UPSTREAM ) {}
    };
}

#endif
