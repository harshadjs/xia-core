#ifndef __CACHE_H__
#define __CACHE_H__

#include "controller.h"

/**
 * Opportunistic caching.
 */

#define CACHE_PORT 1444

class xcache_cache;
class xcache_controller;

struct cache_args {
	xcache_cache *cache;
	xcache_controller *ctrl;
	int cache_in_port;
	int cache_out_port;
};

class xcache_cache {
private:
	static xcache_controller *ctrl;
	std::map<std::string, std::string *> ongoing_downloads;

public:
	/**
	 * A Constructor.
	 */
	xcache_cache() {
	}

	void process_pkt(xcache_controller *ctrl, char *pkt, size_t len);
	static void *run(void *);
	static int create_click_socket(int);
	static void spawn_thread(struct cache_args *);
};


#endif
