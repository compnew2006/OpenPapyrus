/** @file
 *  @brief TCP/IP replication client class.
 */
// Copyright (C) 2008,2010,2011,2015 Olly Betts
// @license GNU GPL
//
#include <xapian-internal.h>
#pragma hdrstop
#include "replicatetcpclient.h"
#include "api/replication.h"
#include "socket_utils.h"
#include "tcpclient.h"

using namespace std;

ReplicateTcpClient::ReplicateTcpClient(const string & hostname, int port,
    double timeout_connect,
    double socket_timeout)
	: socket(open_socket(hostname, port, timeout_connect)),
	remconn(-1, socket)
{
	set_socket_timeouts(socket, socket_timeout);
}

int ReplicateTcpClient::open_socket(const string & hostname, int port,
    double timeout_connect)
{
	return TcpClient::open_socket(hostname, port, timeout_connect, false);
}

void ReplicateTcpClient::update_from_master(const std::string & path,
    const std::string & masterdb,
    Xapian::ReplicationInfo & info,
    double reader_close_time,
    bool force_copy)
{
	Xapian::DatabaseReplica replica(path);
	remconn.send_message('R',
	    force_copy ? string() : replica.get_revision_info(),
	    0.0);
	remconn.send_message('D', masterdb, 0.0);
	replica.set_read_fd(socket);
	info.clear();
	bool more;
	do {
		Xapian::ReplicationInfo subinfo;
		more = replica.apply_next_changeset(&subinfo, reader_close_time);
		info.changeset_count += subinfo.changeset_count;
		info.fullcopy_count += subinfo.fullcopy_count;
		if(subinfo.changed)
			info.changed = true;
	} while(more);
}

ReplicateTcpClient::~ReplicateTcpClient()
{
	remconn.shutdown();
}
