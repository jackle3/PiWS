#pragma once

#include "nrf.h"

/**
 * Routing table for the router:
 * - RCP address 0 is the router itself
 * - RCP address 1 is the first server
 * - RCP address 2 is the second server
 */
static uint32_t rtable[256] = {0, server_addr, server_addr_2};

static uint32_t rtable[256] 