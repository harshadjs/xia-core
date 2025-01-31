#include <fcntl.h>
#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "controller.h"
#include <iostream>
#include "logger.h"
#include "Xsocket.h"
#include "Xkeys.h"
#include "dagaddr.hpp"
#include "dagaddr.h"
#include "xcache_sock.h"
#include "cid.h"
#include <openssl/sha.h>

#define IGNORE_PARAM(__param) ((void)__param)

#define MAX_XID_SIZE 100

enum {
	RET_FAILED = -1,
	RET_OKSENDRESP = 0,
	RET_OKNORESP = 1,
	RET_REMOVEFD = 2,
};

DEFINE_LOG_MACROS(CTRL)

static int send_response(int fd, const char *buf, size_t len)
{
	int ret = 0;
	uint32_t msg_length = htonl(len);

	ret = send(fd, &msg_length, 4, 0);
	if(ret <= 0)
		return ret;

	ret = send(fd, buf, len, 0);
	return ret;
}

static std::string hex_str(unsigned char *data, int len)
{
	char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
			 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	std::string s(len * 2, ' ');
	for (int i = 0; i < len; ++i) {
		s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
		s[2 * i + 1] = hexmap[data[i] & 0x0F];
	}
	return s;
}

static std::string compute_cid(const char *data, size_t len)
{
	unsigned char digest[SHA_DIGEST_LENGTH];

	SHA1((unsigned char *)data, len, digest);

	return hex_str(digest, SHA_DIGEST_LENGTH);
}

static int xcache_create_lib_socket(void)
{
	struct sockaddr_un my_addr;
	int s;
	char socket_name[512];

	s = socket(AF_UNIX, SOCK_STREAM, 0);

	my_addr.sun_family = AF_UNIX;
	if(get_xcache_sock_name(socket_name, 512) < 0) {
		return -1;
	}

	remove(socket_name);

	strcpy(my_addr.sun_path, socket_name);

	bind(s, (struct sockaddr *)&my_addr, sizeof(my_addr));
	listen(s, 100);

	return s;
}

void xcache_controller::status(void)
{
	LOG_CTRL_INFO("[Status]\n");

}

int xcache_controller::fetch_content_remote(sockaddr_x *addr, socklen_t addrlen, xcache_cmd *resp, xcache_cmd *cmd, int flags)
{
	int ret, sock;
	Graph g(addr);

	sock = Xsocket(AF_XIA, SOCK_STREAM, 0);
	if(sock < 0) {
		return RET_FAILED;
	}

	LOG_CTRL_INFO("Fetching content from remote DAG = %s\n", g.dag_string().c_str());

	if(Xconnect(sock, (struct sockaddr *)addr, addrlen) < 0) {
		return RET_FAILED;
	}

	LOG_CTRL_INFO("Xcache client now connected with remote\n");

	std::string data;
	char buf[XIA_MAXBUF] = {0};

	Node expected_cid(g.get_final_intent());
	xcache_meta *meta = new xcache_meta(expected_cid.id_string());
	
	meta->set_FETCHING();

	do {
		ret = Xrecv(sock, buf, XIA_MAXBUF, 0);
		if(ret <= 0)
			break;

		std::string temp(buf + sizeof(struct cid_header), ret - sizeof(struct cid_header));

		data += temp;
	} while(1);

	std::string computed_cid = compute_cid(data.c_str(), data.length());
	struct xcache_context *context = lookup_context(cmd->context_id());

	if(!context) {
		LOG_CTRL_ERROR("Context Lookup Failed\n");
		delete meta;
		return RET_FAILED;
	}

	LOG_CTRL_INFO("DATA = %s", data.c_str());

	if(!(flags & XCF_SKIPCACHE))
		__store(context, meta, (const std::string *)&data);

	ret = RET_OKNORESP;

	if(resp) {
		resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
		resp->set_data(data);
		ret = RET_OKSENDRESP;
	}

	return ret;
}

int xcache_controller::fetch_content_local(sockaddr_x *addr, socklen_t addrlen, xcache_cmd *resp, xcache_cmd *cmd, int flags)
{
	xcache_meta *meta;
	std::string data;
	Graph g(addr);
	Node expected_cid(g.get_final_intent());
	std::string cid(expected_cid.id_string());
	int ret;

	IGNORE_PARAM(flags);
	IGNORE_PARAM(addrlen);
	IGNORE_PARAM(cmd);

	meta = acquire_meta(cid);
	LOG_CTRL_INFO("Fetching content from local\n");
	
	if(!meta) {
		/* We could not find the content locally */
		return RET_FAILED;
	}

	if(!meta->is_AVAILABLE()) {
		release_meta(meta);
		return RET_FAILED;
	}

	data = meta->get();
	
	ret = RET_OKNORESP;

	if(resp) {
		resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
		resp->set_data(data);
		ret = RET_OKSENDRESP;
	}

	release_meta(meta);

	return ret;
}

int xcache_controller::xcache_notify(struct xcache_context *c, sockaddr_x *addr, socklen_t addrlen, int event)
{
	xcache_notif notif;
	std::string buffer("");

	notif.set_cmd(event);
	notif.set_dag(addr, addrlen);
	notif.SerializeToString(&buffer);

	LOG_CTRL_INFO("Sent Notification to %d\n", c->notifSock);

	return send_response(c->notifSock, buffer.c_str(), buffer.length());
}

struct __fetch_content_args {
	xcache_controller *ctrl;
	xcache_cmd *resp;
	xcache_cmd *cmd;
	int ret;
	int flags;
};

void *xcache_controller::__fetch_content(void *__args)
{
	int ret;
	struct __fetch_content_args *args = (struct __fetch_content_args *)__args;
	sockaddr_x addr;
	size_t daglen;

	daglen = args->cmd->dag().length();
	memcpy(&addr, args->cmd->dag().c_str(), args->cmd->dag().length());

	ret = args->ctrl->fetch_content_local(&addr, daglen, args->resp, args->cmd, args->flags);
	if(ret == RET_FAILED)
		ret = args->ctrl->fetch_content_remote(&addr, daglen, args->resp, args->cmd, args->flags);

	args->ret = ret;
	if(!(args->flags & XCF_BLOCK)) {
		struct xcache_context *c = args->ctrl->lookup_context(args->cmd->context_id());

		if(c) {
			args->ctrl->xcache_notify(c, &addr, daglen, XCE_CHUNKARRIVED);
		}

		delete args->cmd;
		delete args;
	}
	return NULL;
}       

int xcache_controller::xcache_fetch_content(xcache_cmd *resp, xcache_cmd *cmd, int flags)
{
	int ret = RET_OKNORESP;

	if(flags & XCF_BLOCK) {
		struct __fetch_content_args args;

		args.ctrl = this;
		args.resp = resp;
		args.cmd = cmd;
		args.ret = ret;
		args.flags = flags;
		__fetch_content(&args);
		ret = args.ret;
	} else {
		struct __fetch_content_args *args = 
			(struct __fetch_content_args *)malloc(sizeof(struct __fetch_content_args));

		pthread_t fetch_thread;
		args->ctrl = this;
		args->ret = ret;
		args->flags = flags;
		args->resp = NULL;
		args->cmd = new xcache_cmd(*cmd);
		pthread_create(&fetch_thread, NULL, __fetch_content, args);
	}

	return ret;
}

std::string xcache_controller::addr2cid(sockaddr_x *addr)
{
	Graph g(addr);
	Node cid_node(g.get_final_intent());
	return cid_node.id_string();
}


int xcache_controller::chunk_read(xcache_cmd *resp, xcache_cmd *cmd)
{
	xcache_meta *meta = acquire_meta(addr2cid((sockaddr_x *)(cmd->dag().c_str())));
	std::string data;

	if(!meta) {
		resp->set_cmd(xcache_cmd::XCACHE_ERROR);
		resp->set_msg("Invalid address\n");
		return RET_OKSENDRESP;
	}

	data = meta->get(cmd->readoffset(), cmd->readlen());
	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_data(data);

	release_meta(meta);
	return RET_OKSENDRESP;
}

int xcache_controller::handle_cmd(int fd, xcache_cmd *resp, xcache_cmd *cmd)
{
	int ret = RET_OKNORESP;
	xcache_cmd response;
	struct xcache_context *c;

	switch(cmd->cmd()) {
	case xcache_cmd::XCACHE_ALLOC_CONTEXT:
		LOG_CTRL_INFO("Received ALLOC_CONTEXTID\n");
		ret = alloc_context(resp, cmd);
		break;
	case xcache_cmd::XCACHE_FLAG_DATASOCKET:
		c = lookup_context(cmd->context_id());
		if(!c) {
			LOG_CTRL_ERROR("Should Not Happen.\n");
		} else {
			c->xcacheSock = fd;
		}
		break;
	case xcache_cmd::XCACHE_FLAG_NOTIFSOCK:
		c = lookup_context(cmd->context_id());
		if(!c) {
			LOG_CTRL_ERROR("Should Not Happen.\n");
		} else {
			c->notifSock = fd;
			ret = RET_REMOVEFD;
		}
		break;
	case xcache_cmd::XCACHE_STORE:
		LOG_CTRL_INFO("Received Store command\n");
		ret = store(resp, cmd);
		break;
	case xcache_cmd::XCACHE_FETCHCHUNK:
		LOG_CTRL_INFO("Received Getchunk command\n");
		ret = xcache_fetch_content(resp, cmd, cmd->flags());
		break;
	case xcache_cmd::XCACHE_GET_STATUS:
		LOG_CTRL_INFO("Received Status command\n");
		status();
		break;
	case xcache_cmd::XCACHE_READ:
		LOG_CTRL_INFO("Received READ command\n");
		ret = chunk_read(resp, cmd);
		break;
	default:
		LOG_CTRL_ERROR("Unknown message received\n");
	}

	return ret;
}


struct xcache_context *xcache_controller::lookup_context(int context_id)
{
	std::map<uint32_t, struct xcache_context *>::iterator i = context_map.find(context_id);

	if(i != context_map.end()) {
		LOG_CTRL_INFO("Context found.\n");
		return i->second;
	} else {
		LOG_CTRL_INFO("Context %d NOT found.\n", context_id);
		return NULL;
	}
}

int xcache_controller::alloc_context(xcache_cmd *resp, xcache_cmd *cmd)
{
	struct xcache_context *c = (struct xcache_context *)malloc(sizeof(struct xcache_context));
	IGNORE_PARAM(cmd);
	++context_id;

	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	resp->set_context_id(context_id);

	memset(c, 0, sizeof(struct xcache_context));
	c->contextID = context_id;
	context_map[context_id] = c;
	LOG_CTRL_INFO("Allocated Context %d\n", resp->context_id());

	return RET_OKSENDRESP;
}

int xcache_controller::__store_policy(xcache_meta *meta)
{
	IGNORE_PARAM(meta);
	//FIXME: Add code
	return 0;
}

int xcache_controller::__store(struct xcache_context *context, xcache_meta *meta, const std::string *data)
{
	lock_meta_map();
	meta->lock();

	meta_map[meta->get_cid()] = meta;

	if(!context) {
		LOG_CTRL_ERROR("NULL context provided.\n");
		return -1;
	}

	if(__store_policy(meta) < 0) {
		LOG_CTRL_ERROR("Context store failed\n");
		meta->unlock();
		unlock_meta_map();
		return RET_FAILED;
	}

	register_meta(meta);
	store_manager.store(meta, data);

	LOG_CTRL_INFO("STORING %s\n", data->c_str());

	meta->set_AVAILABLE();
	meta->unlock();
	unlock_meta_map();

	return RET_OKSENDRESP;
}

sockaddr_x xcache_controller::cid2addr(std::string cid)
{
	sockaddr_x addr;
	std::string myCid("CID:");

	myCid += cid;

	dag_add_nodes(&addr, 3, myAD, myHID, myCid.c_str());
	dag_set_intent(&addr, 2);
	dag_add_path(&addr, 3, 0, 1, 2);

	return addr;
}

int xcache_controller::store(xcache_cmd *resp, xcache_cmd *cmd)
{
	struct xcache_context *context;
	xcache_meta *meta;
	std::string cid = compute_cid(cmd->data().c_str(), cmd->data().length());

	meta = acquire_meta(cid);

	/* FIXME: Add proper errnos */
	if(meta) {
		LOG_CTRL_ERROR("Meta Exists.\n");
		resp->set_cmd(xcache_cmd::XCACHE_ERROR);
		resp->set_status(xcache_cmd::XCACHE_ERR_EXISTS);
		release_meta(meta);
		return RET_OKSENDRESP;
	}

	/* New object - Allocate a meta */
	meta = new xcache_meta(cid);

	context = lookup_context(cmd->context_id());
	if(!context) {
		return RET_FAILED;
	}

	if(__store(context, meta, &cmd->data()) == RET_FAILED) {
		return RET_FAILED;
	}

	resp->set_cmd(xcache_cmd::XCACHE_RESPONSE);
	sockaddr_x addr = cid2addr(cid);
	resp->set_dag((char *)&addr, sizeof(sockaddr_x));
	Graph g(&addr);
	printf("--------------------\n");
	g.print_graph();

	LOG_CTRL_INFO("Store Finished\n");

	return RET_OKSENDRESP;
}


void xcache_controller::send_content_remote(int sock, sockaddr_x *mypath)
{
	Graph g(mypath);
	Node cid(g.get_final_intent());
	xcache_cmd resp;

	LOG_CTRL_INFO("CID = %s\n", cid.id_string().c_str());

	if(fetch_content_local(mypath, sizeof(sockaddr_x), &resp, NULL, 0)) {
		LOG_CTRL_ERROR("This should not happen.\n");
	}

	size_t remaining = resp.data().length();
	size_t offset = 0;

	while(remaining > 0) {
		char buf[XIA_MAXBUF];
		size_t to_send = MIN(XIA_MAXBUF, remaining);
		struct cid_header header;
		size_t header_size = sizeof(header);

		if(to_send + header_size > XIA_MAXBUF)
			to_send -= header_size;

		header.offset = offset;
		header.length = to_send;
		header.total_length = resp.data().length();
		strcpy(header.cid, cid.id_string().c_str());

		memcpy(buf, &header, header_size);
		memcpy(buf + header_size, resp.data().c_str() + offset, to_send);

		remaining -= to_send;

		Xsend(sock, buf, to_send + header_size, 0);

		offset += to_send;
	}
}

int xcache_controller::create_sender(void)
{
	char sid_string[strlen("SID:") + XIA_SHA_DIGEST_STR_LEN];
	int xcache_sock;
	struct addrinfo *ai;

	if((xcache_sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		return -1;

	if(XreadLocalHostAddr(xcache_sock,
			      myAD, sizeof(myAD), myHID, sizeof(myHID),
			      my4ID, sizeof(my4ID)) < 0)
		return -1;

	if(XmakeNewSID(sid_string, sizeof(sid_string))) {
		LOG_CTRL_ERROR("XmakeNewSID failed\n");
		return -1;
	}

	if(XsetXcacheSID(xcache_sock, sid_string, strlen(sid_string)) < 0)
		return -1;

	LOG_CTRL_DEBUG("XcacheSID is %s\n", sid_string);

	if (Xgetaddrinfo(NULL, sid_string, NULL, &ai) != 0)
		return -1;

	sockaddr_x *dag = (sockaddr_x *)ai->ai_addr;

	if (Xbind(xcache_sock, (struct sockaddr*)dag, sizeof(dag)) < 0) {
		Xclose(xcache_sock);
		return -1;
	}

	Xlisten(xcache_sock, 5);

	Graph g(dag);
	LOG_CTRL_INFO("Listening on dag: %s\n", g.dag_string().c_str());

	return xcache_sock;
}

int xcache_controller::register_meta(xcache_meta *meta)
{
	int rv;
	std::string empty_str("");
	std::string temp_cid("CID:");

	temp_cid += meta->get_cid();
	
	LOG_CTRL_DEBUG("Setting Route for %s.\n", temp_cid.c_str());
	rv = xr.setRoute(temp_cid, DESTINED_FOR_LOCALHOST, empty_str, 0);
	LOG_CTRL_DEBUG("Route Setting Returned %d\n", rv);

	return rv;
}

void xcache_controller::run(void)
{
	fd_set fds, allfds;
	int max, libsocket, rc, sendersocket;
	struct cache_args args;

	std::vector<int> active_conns;
	std::vector<int>::iterator iter;

	libsocket = xcache_create_lib_socket();

	args.cache = &cache;
	args.cache_in_port = getXcacheInPort();
	args.cache_out_port = getXcacheOutPort();
	args.ctrl = this;
	
	cache.spawn_thread(&args);

	if((rc = xr.connect()) != 0) {
		LOG_CTRL_ERROR("Unable to connect to click %d \n", rc);
		return;
	}

	xr.setRouter(hostname); 
	sendersocket = create_sender();

	FD_ZERO(&fds);
	FD_SET(libsocket, &allfds);
	FD_SET(sendersocket, &allfds);

	LOG_CTRL_INFO("Entering The Loop\n");

repeat:
	memcpy(&fds, &allfds, sizeof(fd_set));

	max = libsocket;
	for(iter = active_conns.begin(); iter != active_conns.end(); ++iter) {
		max = MAX(max, *iter);
	}

	Xselect(max + 1, &fds, NULL, NULL, NULL);

	int something;
	something = 0;
	LOG_CTRL_INFO("Broken\n");

	if(FD_ISSET(libsocket, &fds)) {
		something = 1;
		int new_connection = accept(libsocket, NULL, 0);
		LOG_CTRL_INFO("Action on libsocket\n");
		active_conns.push_back(new_connection);
		FD_SET(new_connection, &allfds);
	}

	if(FD_ISSET(sendersocket, &fds)) {
		int accept_sock;
		sockaddr_x mypath;
		socklen_t mypath_len = sizeof(mypath);

		if((accept_sock = XacceptAs(sendersocket, (struct sockaddr *)&mypath, &mypath_len, NULL, NULL)) < 0) {
			LOG_CTRL_ERROR("XacceptAs failed\n");
		} else {
			send_content_remote(accept_sock, &mypath);
			Xclose(accept_sock);
		}
	}

	for(iter = active_conns.begin(); iter != active_conns.end(); ) {
		if(!FD_ISSET(*iter, &fds)) {
			++iter;
			continue;
		}
		something = 1;

		char buf[512] = "";
		std::string buffer("");
		xcache_cmd resp, cmd;
		int ret;
		uint32_t msg_length, remaining;

		ret = recv(*iter, &msg_length, 4, 0);
		if(ret <= 0)
			goto disconnected;

		remaining = ntohl(msg_length);

		while(remaining > 0) {
			ret = recv(*iter, buf, MIN(512, remaining), 0);
			LOG_CTRL_INFO("Recv returned %d, remaining = %d\n", ret, remaining);
			if(ret <= 0)
				break;

			buffer.append(buf, ret);
			remaining -= ret;
		}

		if(msg_length == 0 && ret <= 0) {
			goto disconnected;
		} else {
			bool parse_success = cmd.ParseFromString(buffer);
			LOG_CTRL_INFO("Controller received %lu bytes\n", buffer.length());
			if(!parse_success) {
				LOG_CTRL_ERROR("[ERROR] Protobuf could not parse\n");
			} else if((ret = handle_cmd(*iter, &resp, &cmd)) == RET_OKSENDRESP) {
				resp.SerializeToString(&buffer);
				if(send_response(*iter, buffer.c_str(), buffer.length()) < 0) {
					LOG_CTRL_ERROR("FIXME: handle return value of write\n");
				} else {
					LOG_CTRL_INFO("Data sent to client\n");
				}
			} else if(ret == RET_REMOVEFD) {
				goto removefd;
			}
		}
		++iter;
		continue;
disconnected:
		LOG_CTRL_DEBUG("Client %d disconnected.\n", *iter);
		close(*iter);
removefd:
		FD_CLR(*iter, &allfds);
		active_conns.erase(iter);
	}

	if(!something) {
		sleep(5);
	}


	goto repeat;
}

xcache_meta *xcache_controller::acquire_meta(std::string cid)
{
	lock_meta_map();

	std::map<std::string, xcache_meta *>::iterator i = meta_map.find(cid);
	if(i == meta_map.end()) {
		/* We could not find the content locally */
		unlock_meta_map();
		return NULL;
	}

	xcache_meta *meta = i->second;
	meta->lock();

	return meta;

}

void xcache_controller::release_meta(xcache_meta *meta)
{
	unlock_meta_map();
	if(meta)
		meta->unlock();
}

inline int xcache_controller::lock_meta_map(void)
{
	return pthread_mutex_lock(&meta_map_lock);
}

inline int xcache_controller::unlock_meta_map(void)
{
	return pthread_mutex_unlock(&meta_map_lock);
}

void xcache_controller::add_meta(xcache_meta *meta)
{
	lock_meta_map();
	meta_map[meta->get_cid()] = meta;
	unlock_meta_map();
}

void xcache_controller::set_conf(struct xcache_conf *conf)
{
	hostname = std::string(conf->hostname);

	threads = (pthread_t *)malloc(sizeof(pthread_t) * conf->threads);
}
