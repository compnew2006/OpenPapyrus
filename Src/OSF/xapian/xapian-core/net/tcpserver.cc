/** @file
 * @brief class for TCP/IP-based server.
 */
// Copyright 1999,2000,2001 BrightStation PLC
// Copyright 2002 Ananova Ltd
// Copyright 2002,2003,2004,2005,2006,2007,2008,2009,2010,2011,2012,2015,2017,2018 Olly Betts
// @license GNU GPL
//
#include <xapian-internal.h>
#pragma hdrstop
#include "tcpserver.h"
#include "safefcntl.h"
#include "safenetdb.h"
#include "safesyssocket.h"
#include "remoteconnection.h"
#include "resolver.h"
#include "socket_utils.h"
#ifdef __WIN32__
	#include <process.h>    /* _beginthreadex, _endthreadex */
#else
	#include <netinet/in_systm.h>
	#include <netinet/in.h>
	#include <netinet/ip.h>
	#include <netinet/tcp.h>
	#include <arpa/inet.h>
	#include <signal.h>
	#include <sys/wait.h>
#endif
using namespace std;

// Handle older systems.
#if !defined SIGCHLD && defined SIGCLD
	#define SIGCHLD SIGCLD
#endif
#ifdef __WIN32__
	#define CLOSESOCKET(S) closesocket(S) // We must call closesocket() (instead of just close()) under __WIN32__ or else the socket remains in the CLOSE_WAIT state.
#else
	#define CLOSESOCKET(S) close(S)
#endif

/// The XapianTcpServer constructor, taking a database and a listening port.
XapianTcpServer::XapianTcpServer(const std::string & host, int port, bool tcp_nodelay, bool verbose_) : listen_socket(get_listening_socket(host, port, tcp_nodelay
#if defined __CYGWIN__ || defined __WIN32__
	    , mutex
#endif
	    )),
	verbose(verbose_)
{
}

int XapianTcpServer::get_listening_socket(const std::string & host, int port, bool tcp_nodelay
#if defined __CYGWIN__ || defined __WIN32__
    , HANDLE &mutex
#endif
    )
{
	int socketfd = -1;
	int bind_errno = 0;
	for(auto&& r : Resolver(host, port, AI_PASSIVE)) {
		int socktype = r.ai_socktype | SOCK_CLOEXEC;
		int fd = socket(r.ai_family, socktype, r.ai_protocol);
		if(fd == -1)
			continue;

#if !defined __WIN32__ && defined F_SETFD && defined FD_CLOEXEC
		// We can't use a preprocessor check on the *value* of SOCK_CLOEXEC as on
		// Linux SOCK_CLOEXEC is an enum, with '#define SOCK_CLOEXEC SOCK_CLOEXEC'
		// to allow '#ifdef SOCK_CLOEXEC' to work.
		if(SOCK_CLOEXEC == 0)
			(void)fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif

		int retval = 0;

		if(tcp_nodelay) {
			int optval = 1;
			// 4th argument might need to be void* or char* - cast it to char*
			// since C++ allows implicit conversion to void* but not from
			// void*.
			retval = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char *>(&optval), sizeof(optval));
		}
		int optval = 1;
#if defined __CYGWIN__ || defined __WIN32__
		// Windows has screwy semantics for SO_REUSEADDR - it allows the user
		// to bind to a port which is already bound and listening!  That's
		// just not suitable as we don't want multiple processes listening on
		// the same port, so we guard against that by using a named win32 mutex
		// object (and we create it in the 'Global namespace' so that this
		// still works in a Terminal Services environment).
		string name = "Global\\xapian-tcpserver-listening-" + str(port);
		if((mutex = ::CreateMutexA(NULL, TRUE, name.c_str())) == NULL) {
			// We failed to create the mutex, probably the error is
			// ERROR_ACCESS_DENIED, which simply means that XapianTcpServer is
			// already running on this port but as a different user.
		}
		else if(GetLastError() == ERROR_ALREADY_EXISTS) {
			// The mutex already existed, so XapianTcpServer is already running
			// on this port.
			CloseHandle(mutex);
			mutex = NULL;
		}
		if(mutex == NULL) {
			cerr << "Server is already running on port " << port << endl;
			// 69 is EX_UNAVAILABLE.  Scripts can use this to detect if the
			// server failed to bind to the requested port.
			exit(69); // FIXME: calling exit() here isn't ideal...
		}
#endif
		if(retval >= 0) {
			retval = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
				reinterpret_cast<char *>(&optval),
				sizeof(optval));
		}

#if defined SO_EXCLUSIVEADDRUSE
		// NT4 sp4 and later offer SO_EXCLUSIVEADDRUSE which nullifies the
		// security issues from SO_REUSEADDR (which affect *any* listening
		// process, even if doesn't use SO_REUSEADDR itself).  There's still no
		// way of addressing the issue of not being able to listen on a port
		// which has closed connections in TIME_WAIT state though.
		//
		// Note: SO_EXCLUSIVEADDRUSE requires admin privileges prior to XP SP2.
		// Because of this and the lack support on older versions, we don't
		// currently check the return value.
		if(retval >= 0) {
			(void)setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
			    reinterpret_cast<char *>(&optval),
			    sizeof(optval));
		}
#endif

		if(retval < 0) {
			int saved_errno = socket_errno(); // note down in case close hits an error
			CLOSESOCKET(fd);
			throw Xapian::NetworkError("setsockopt failed", saved_errno);
		}

		if(::bind(fd, r.ai_addr, r.ai_addrlen) == 0) {
			socketfd = fd;
			break;
		}

		// Note down the error code for the first address we try, which seems
		// likely to be more helpful than the last in the case where they
		// differ.
		if(bind_errno == 0)
			bind_errno = socket_errno();

		CLOSESOCKET(fd);
	}

	if(socketfd == -1) {
		if(bind_errno == EADDRINUSE) {
			cerr << host << ':' << port << " already in use" << endl;
			// 69 is EX_UNAVAILABLE.  Scripts can use this to detect if the
			// server failed to bind to the requested port.
			exit(69); // FIXME: calling exit() here isn't ideal...
		}
		if(bind_errno == EACCES) {
			cerr << "Can't bind to privileged port " << port << endl;
			// 77 is EX_NOPERM.  Scripts can use this to detect if
			// xapian-tcpsrv failed to bind to the requested port.
			exit(77); // FIXME: calling exit() here isn't ideal...
		}
		throw Xapian::NetworkError("bind failed", bind_errno);
	}

	if(listen(socketfd, 5) < 0) {
		int saved_errno = socket_errno(); // note down in case close hits an error
		CLOSESOCKET(socketfd);
		throw Xapian::NetworkError("listen failed", saved_errno);
	}
	return socketfd;
}

int XapianTcpServer::accept_connection()
{
	struct sockaddr_storage remote_address;
	SOCKLEN_T remote_address_size = sizeof(remote_address);
	// accept connections
	int con_socket = accept(listen_socket,
		reinterpret_cast<sockaddr *>(&remote_address),
		&remote_address_size);

	if(con_socket < 0) {
#ifdef __WIN32__
		if(WSAGetLastError() == WSAEINTR) {
			// Our CtrlHandler function closed the socket.
			if(mutex) CloseHandle(mutex);
			mutex = NULL;
			return -1;
		}
#endif
		throw Xapian::NetworkError("accept failed", socket_errno());
	}

	if(verbose) {
		char host[PRETTY_IP6_LEN];
		int port = pretty_ip6(&remote_address, host);
		if(port >= 0) {
			cout << "Connection from " << host << " port " << port << endl;
		}
		else {
			cout << "Connection from unknown host" << endl;
		}
	}

	return con_socket;
}

XapianTcpServer::~XapianTcpServer()
{
	CLOSESOCKET(listen_socket);
#if defined __CYGWIN__ || defined __WIN32__
	if(mutex) CloseHandle(mutex);
#endif
}

#ifdef HAVE_FORK
// A fork() based implementation.

extern "C" {
[[noreturn]] static void on_SIGTERM(int /*sig*/)
{
	signal(SIGTERM, SIG_DFL);
	/* terminate all processes in my process group */
#ifdef HAVE_KILLPG
	killpg(0, SIGTERM);
#else
	kill(0, SIGTERM);
#endif
	exit(0);
}

#ifdef HAVE_WAITPID
static void on_SIGCHLD(int /*sig*/)
{
	int status;
	while(waitpid(-1, &status, WNOHANG) > 0);
}

#endif
}

void XapianTcpServer::run()
{
	// Handle connections until shutdown.

	// Set up signal handlers.
#ifdef HAVE_WAITPID
	signal(SIGCHLD, on_SIGCHLD);
#else
	signal(SIGCHLD, SIG_IGN);
#endif
	signal(SIGTERM, on_SIGTERM);

	while(true) {
		try {
			int connected_socket = accept_connection();
			pid_t pid = fork();
			if(pid == 0) {
				// Child process.
				close(listen_socket);

				handle_one_connection(connected_socket);
				close(connected_socket);

				if(verbose) cout << "Connection closed." << endl;
				exit(0);
			}

			// Parent process.

			if(pid < 0) {
				// fork() failed.

				// Note down errno from fork() in case close() hits an error.
				int saved_errno = socket_errno();
				close(connected_socket);
				throw Xapian::NetworkError("fork failed", saved_errno);
			}

			close(connected_socket);
		} catch(const Xapian::Error &e) {
			// FIXME: better error handling.
			cerr << "Caught " << e.get_description() << endl;
		} catch(...) {
			// FIXME: better error handling.
			cerr << "Caught exception." << endl;
		}
	}
}

#elif defined __WIN32__

// A threaded, Windows specific, implementation.

/** The socket which will be closed by CtrlHandler.
 *
 *  FIXME - is there any way to avoid using a global variable here?
 */
static const int * pShutdownSocket = NULL;

/// Console interrupt handler.
static BOOL CtrlHandler(DWORD fdwCtrlType)
{
	switch(fdwCtrlType) {
		case CTRL_C_EVENT:
		case CTRL_CLOSE_EVENT:
		//  Console is about to die.
		// CTRL_CLOSE_EVENT gives us 5 seconds before displaying a
		// confirmation dialog asking if we really are sure.
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
		    // These 2 will probably need to change when we get service
		    // support - the service will prevent these being seen, so only
		    // apply interactively.
		    cout << "Shutting down..." << endl;
		    break; // default behaviour
		case CTRL_BREAK_EVENT:
		    // This (probably) means the developer is struggling to get
		    // things to behave, and really wants to shutdown so let the OS
		    // handle Ctrl+Break in the default way.
		    cout << "Ctrl+Break: aborting process" << endl;
		    return FALSE;
		default:
		    cerr << "unexpected CtrlHandler: " << fdwCtrlType << endl;
		    return FALSE;
	}

	// Note: close() does not cause a blocking accept() call to terminate.
	// However, it appears closesocket() does.  This is much easier than trying
	// to setup a non-blocking accept().
	if(!pShutdownSocket || closesocket(*pShutdownSocket) == SOCKET_ERROR) {
		// We failed to close the socket, so just let the OS handle the
		// event in the default way.
		return FALSE;
	}
	pShutdownSocket = NULL;
	return TRUE; // Tell the OS that we've handled the event.
}

/// Structure which is used to pass parameters to the new threads.
struct thread_param {
	thread_param(XapianTcpServer * s, int c) : server(s), connected_socket(c) 
	{
	}
	XapianTcpServer * server;
	int connected_socket;
};

/// The thread entry-point.
static unsigned __stdcall run_thread(void * param_)
{
	thread_param * param(reinterpret_cast<thread_param *>(param_));
	int socket = param->connected_socket;

	param->server->handle_one_connection(socket);
	closesocket(socket);

	delete param;

	_endthreadex(0);
	return 0;
}

void XapianTcpServer::run()
{
	// Handle connections until shutdown.

	// Set up the shutdown handler - this is a bit hacky, and sadly involves
	// a global variable.
	pShutdownSocket = &listen_socket;
	if(!::SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE))
		throw Xapian::NetworkError("Failed to install shutdown handler");

	while(true) {
		try {
			int connected_socket = accept_connection();
			if(connected_socket == -1)
				return; // Shutdown has happened

			// Spawn a new thread to handle the connection.
			// (This seems like lots of hoops just to end up calling
			// this->handle_one_connection() on a new thread. There might be a
			// better way...)
			thread_param * param = new thread_param(this, connected_socket);
			HANDLE hthread = (HANDLE)_beginthreadex(NULL, 0, ::run_thread, param, 0, NULL);
			if(hthread == 0) {
				// errno holds the error code from _beginthreadex, and
				// closesocket() doesn't set errno.
				closesocket(connected_socket);
				throw Xapian::NetworkError("_beginthreadex failed", errno);
			}

			// FIXME: keep track of open thread handles so we can gracefully
			// close each thread down.  OTOH, when we want to kill them all its
			// likely to mean the process is on its way down, so it doesn't
			// really matter...
			CloseHandle(hthread);
		} catch(const Xapian::Error &e) {
			// FIXME: better error handling.
			cerr << "Caught " << e.get_description() << endl;
		} catch(...) {
			// FIXME: better error handling.
			cerr << "Caught exception." << endl;
		}
	}
}

#else
#error Neither HAVE_FORK nor __WIN32__ are defined.
#endif

void XapianTcpServer::run_once()
{
	// Run a single request in the current process/thread.
	int fd = accept_connection();
	handle_one_connection(fd);
	CLOSESOCKET(fd);
}

#ifdef DISABLE_GPL_LIBXAPIAN
#error GPL source we cannot relicense included in libxapian
#endif
