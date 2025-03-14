#pragma once

#include "nrf.h"

/**
 * Router's rtable: maps from RCP address to NRF address
 * - RCP address 0 is the router itself
 * - RCP address 1 should route to the first user (receiver)
 * - RCP address 2 should route to the second user (receiver))
 */
static uint32_t router_rtable[256] = {
    [0] = 0,            /* Router itself */
    [1] = server_addr,  /* First server */
    [2] = server_addr_2 /* Second server */
};

/**
 * First user's rtable: maps from RCP address to NRF address
 * - RCP address 0 should route to the router's server (receiver)
 * - RCP address 1 is the user itself
 * - RCP address 2 should route to the router's server (receiver)
 */
static uint32_t user1_rtable[256] = {
    [0] = router_server_addr, /* Router itself */
    [1] = 0,                  /* First server */
    [2] = server_addr_2       /* Second server */
};

/**
 * Second user's rtable: maps from RCP address to NRF address
 * - RCP address 0 should route to the router's server (receiver)
 * - RCP address 1 should route to the router's server (receiver)
 * - RCP address 2 is the user itself
 */
static uint32_t user2_rtable[256] = {
    [0] = router_server_addr, /* Router itself */
    [1] = server_addr,        /* First server */
    [2] = 0                   /* Second server */
};

/**
 * Routing table mapping: maps from RCP address to the corresponding routing table
 * - RCP address 0 maps to router_rtable
 * - RCP address 1 maps to user1_rtable
 * - RCP address 2 maps to user2_rtable
 */
static uint32_t *rtable_map[256] = {
    [0] = router_rtable,
    [1] = user1_rtable,
    [2] = user2_rtable
};