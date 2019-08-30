/*********************************************************************
 * Copyright 2014,2015,2016,2017 Cumulus Networks, Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; see the file COPYING; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * bfd.h: implements the BFD protocol.
 */

#ifndef _BFD_H_
#define _BFD_H_

#include <netinet/in.h>

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>

#include "lib/hash.h"
#include "lib/libfrr.h"
#include "lib/qobj.h"
#include "lib/queue.h"

#include "bfdctl.h"

#define ETHERNET_ADDRESS_LENGTH 6

#ifdef BFD_DEBUG
#define BFDD_JSON_CONV_OPTIONS (JSON_C_TO_STRING_PRETTY)
#else
#define BFDD_JSON_CONV_OPTIONS (0)
#endif

DECLARE_MGROUP(BFDD);
DECLARE_MTYPE(BFDD_TMP);
DECLARE_MTYPE(BFDD_CONFIG);
DECLARE_MTYPE(BFDD_LABEL);
DECLARE_MTYPE(BFDD_CONTROL);
DECLARE_MTYPE(BFDD_NOTIFICATION);

struct bfd_timers {
	uint32_t desired_min_tx;
	uint32_t required_min_rx;
	uint32_t required_min_echo;
};

struct bfd_discrs {
	uint32_t my_discr;
	uint32_t remote_discr;
};

/*
 * Format of control packet.  From section 4)
 */
struct bfd_pkt {
	union {
		uint32_t byteFields;
		struct {
			uint8_t diag;
			uint8_t flags;
			uint8_t detect_mult;
			uint8_t len;
		};
	};
	struct bfd_discrs discrs;
	struct bfd_timers timers;
};

/*
 * Format of Echo packet.
 */
struct bfd_echo_pkt {
	union {
		uint32_t byteFields;
		struct {
			uint8_t ver;
			uint8_t len;
			uint16_t reserved;
		};
	};
	uint32_t my_discr;
	uint8_t pad[16];
};


/* Macros for manipulating control packets */
#define BFD_VERMASK 0x03
#define BFD_DIAGMASK 0x1F
#define BFD_GETVER(diag) ((diag >> 5) & BFD_VERMASK)
#define BFD_SETVER(diag, val) ((diag) |= (val & BFD_VERMASK) << 5)
#define BFD_VERSION 1
#define BFD_PBIT 0x20
#define BFD_FBIT 0x10
#define BFD_CBIT 0x08
#define BFD_ABIT 0x04
#define BFD_DEMANDBIT 0x02
#define BFD_DIAGNEIGHDOWN 3
#define BFD_DIAGDETECTTIME 1
#define BFD_DIAGADMINDOWN 7
#define BFD_SETDEMANDBIT(flags, val)                                           \
	{                                                                      \
		if ((val))                                                     \
			flags |= BFD_DEMANDBIT;                                \
	}
#define BFD_SETPBIT(flags, val)                                                \
	{                                                                      \
		if ((val))                                                     \
			flags |= BFD_PBIT;                                     \
	}
#define BFD_GETPBIT(flags) (flags & BFD_PBIT)
#define BFD_SETFBIT(flags, val)                                                \
	{                                                                      \
		if ((val))                                                     \
			flags |= BFD_FBIT;                                     \
	}
#define BFD_GETFBIT(flags) (flags & BFD_FBIT)
#define BFD_SETSTATE(flags, val)                                               \
	{                                                                      \
		if ((val))                                                     \
			flags |= (val & 0x3) << 6;                             \
	}
#define BFD_GETSTATE(flags) ((flags >> 6) & 0x3)
#define BFD_ECHO_VERSION 1
#define BFD_ECHO_PKT_LEN sizeof(struct bfd_echo_pkt)
#define BFD_CTRL_PKT_LEN sizeof(struct bfd_pkt)
#define IP_HDR_LEN 20
#define UDP_HDR_LEN 8
#define ETH_HDR_LEN 14
#define VXLAN_HDR_LEN 8
#define HEADERS_MIN_LEN (ETH_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN)
#define BFD_ECHO_PKT_TOT_LEN                                                   \
	((int)(ETH_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN + BFD_ECHO_PKT_LEN))
#define BFD_VXLAN_PKT_TOT_LEN                                                  \
	((int)(VXLAN_HDR_LEN + ETH_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN          \
	       + BFD_CTRL_PKT_LEN))
#define BFD_RX_BUF_LEN 160

/* BFD session flags */
enum bfd_session_flags {
	BFD_SESS_FLAG_NONE = 0,
	BFD_SESS_FLAG_ECHO = 1 << 0,	/* BFD Echo functionality */
	BFD_SESS_FLAG_ECHO_ACTIVE = 1 << 1, /* BFD Echo Packets are being sent
					     * actively
					     */
	BFD_SESS_FLAG_MH = 1 << 2,	  /* BFD Multi-hop session */
	BFD_SESS_FLAG_VXLAN = 1 << 3,       /* BFD Multi-hop session which is
					     * used to monitor vxlan tunnel
					     */
	BFD_SESS_FLAG_IPV6 = 1 << 4,	/* BFD IPv6 session */
	BFD_SESS_FLAG_SEND_EVT_ACTIVE = 1 << 5, /* send event timer active */
	BFD_SESS_FLAG_SEND_EVT_IGNORE = 1 << 6, /* ignore send event when timer
						 * expires
						 */
	BFD_SESS_FLAG_SHUTDOWN = 1 << 7,	/* disable BGP peer function */
};

#define BFD_SET_FLAG(field, flag) (field |= flag)
#define BFD_UNSET_FLAG(field, flag) (field &= ~flag)
#define BFD_CHECK_FLAG(field, flag) (field & flag)

/* BFD session hash keys */
struct bfd_shop_key {
	struct sockaddr_any peer;
	char port_name[MAXNAMELEN + 1];
};

struct bfd_mhop_key {
	struct sockaddr_any peer;
	struct sockaddr_any local;
	char vrf_name[MAXNAMELEN + 1];
};

struct bfd_session_stats {
	uint64_t rx_ctrl_pkt;
	uint64_t tx_ctrl_pkt;
	uint64_t rx_echo_pkt;
	uint64_t tx_echo_pkt;
	uint64_t session_up;
	uint64_t session_down;
	uint64_t znotification;
};

struct bfd_session_vxlan_info {
	uint32_t vnid;
	uint32_t decay_min_rx;
	uint8_t forwarding_if_rx;
	uint8_t cpath_down;
	uint8_t check_tnl_key;
	uint8_t local_dst_mac[ETHERNET_ADDRESS_LENGTH];
	uint8_t peer_dst_mac[ETHERNET_ADDRESS_LENGTH];
	struct in_addr local_dst_ip;
	struct in_addr peer_dst_ip;
};

/* bfd_session shortcut label forwarding. */
struct peer_label;

/*
 * Session state information
 */
struct bfd_session {

	/* protocol state per RFC 5880*/
	uint8_t ses_state;
	struct bfd_discrs discrs;
	uint8_t local_diag;
	uint8_t demand_mode;
	uint8_t detect_mult;
	uint8_t remote_detect_mult;
	uint8_t mh_ttl;

	/* Timers */
	struct bfd_timers timers;
	struct bfd_timers new_timers;
	uint32_t up_min_tx;
	uint64_t detect_TO;
	struct thread *echo_recvtimer_ev;
	struct thread *recvtimer_ev;
	uint64_t xmt_TO;
	uint64_t echo_xmt_TO;
	struct thread *xmttimer_ev;
	struct thread *echo_xmttimer_ev;
	uint64_t echo_detect_TO;

	/* software object state */
	uint8_t polling;

	/* This and the localDiscr are the keys to state info */
	struct peer_label *pl;
	union {
		struct bfd_shop_key shop;
		struct bfd_mhop_key mhop;
	};
	int sock;

	struct sockaddr_any local_address;
	struct sockaddr_any local_ip;
	int ifindex;
	uint8_t local_mac[ETHERNET_ADDRESS_LENGTH];
	uint8_t peer_mac[ETHERNET_ADDRESS_LENGTH];
	uint16_t ip_id;

	/* BFD session flags */
	enum bfd_session_flags flags;

	uint8_t echo_pkt[BFD_ECHO_PKT_TOT_LEN]; /* Save the Echo Packet
						 * which will be transmitted
						 */
	struct bfd_session_stats stats;
	struct bfd_session_vxlan_info vxlan_info;

	struct timeval uptime;   /* last up time */
	struct timeval downtime; /* last down time */

	/* Remote peer data (for debugging mostly) */
	uint8_t remote_diag;
	struct bfd_timers remote_timers;

	uint64_t refcount; /* number of pointers referencing this. */

	/* VTY context data. */
	QOBJ_FIELDS;
};
DECLARE_QOBJ_TYPE(bfd_session);

struct peer_label {
	TAILQ_ENTRY(peer_label) pl_entry;

	struct bfd_session *pl_bs;
	char pl_label[MAXNAMELEN];
};
TAILQ_HEAD(pllist, peer_label);

struct bfd_diag_str_list {
	const char *str;
	int type;
};

struct bfd_state_str_list {
	const char *str;
	int type;
};

struct bfd_vrf {
	int vrf_id;
	char name[MAXNAMELEN + 1];
} bfd_vrf;

struct bfd_iface {
	int vrf_id;
	char ifname[MAXNAMELEN + 1];
} bfd_iface;


/* States defined per 4.1 */
#define PTM_BFD_ADM_DOWN 0
#define PTM_BFD_DOWN 1
#define PTM_BFD_INIT 2
#define PTM_BFD_UP 3


/* Various constants */
/* Retrieved from ptm_timer.h from Cumulus PTM sources. */
#define MSEC_PER_SEC 1000L
#define NSEC_PER_MSEC 1000000L

#define BFD_DEF_DEMAND 0
#define BFD_DEFDETECTMULT 3
#define BFD_DEFDESIREDMINTX (300 * MSEC_PER_SEC)
#define BFD_DEFREQUIREDMINRX (300 * MSEC_PER_SEC)
#define BFD_DEF_REQ_MIN_ECHO (50 * MSEC_PER_SEC)
#define BFD_DEF_SLOWTX (2000 * MSEC_PER_SEC)
#define BFD_DEF_MHOP_TTL 5
#define BFD_PKT_LEN 24 /* Length of control packet */
#define BFD_TTL_VAL 255
#define BFD_RCV_TTL_VAL 1
#define BFD_TOS_VAL 0xC0
#define BFD_PKT_INFO_VAL 1
#define BFD_IPV6_PKT_INFO_VAL 1
#define BFD_IPV6_ONLY_VAL 1
#define BFD_SRCPORTINIT 49142
#define BFD_SRCPORTMAX 65536
#define BFD_DEFDESTPORT 3784
#define BFD_DEF_ECHO_PORT 3785
#define BFD_DEF_MHOP_DEST_PORT 4784
#define BFD_CMD_STRING_LEN (MAXNAMELEN + 50)
#define BFD_BUFFER_LEN (BFD_CMD_STRING_LEN + MAXNAMELEN + 1)

/*
 * control.c
 *
 * Daemon control code to speak with local consumers.
 */

/* See 'bfdctrl.h' for client protocol definitions. */

struct bfd_control_buffer {
	size_t bcb_left;
	size_t bcb_pos;
	union {
		struct bfd_control_msg *bcb_bcm;
		uint8_t *bcb_buf;
	};
};

struct bfd_control_queue {
	TAILQ_ENTRY(bfd_control_queue) bcq_entry;

	struct bfd_control_buffer bcq_bcb;
};
TAILQ_HEAD(bcqueue, bfd_control_queue);

struct bfd_notify_peer {
	TAILQ_ENTRY(bfd_notify_peer) bnp_entry;

	struct bfd_session *bnp_bs;
};
TAILQ_HEAD(bnplist, bfd_notify_peer);

struct bfd_control_socket {
	TAILQ_ENTRY(bfd_control_socket) bcs_entry;

	int bcs_sd;
	struct thread *bcs_ev;
	struct thread *bcs_outev;
	struct bcqueue bcs_bcqueue;

	/* Notification data */
	uint64_t bcs_notify;
	struct bnplist bcs_bnplist;

	enum bc_msg_version bcs_version;
	enum bc_msg_type bcs_type;

	/* Message buffering */
	struct bfd_control_buffer bcs_bin;
	struct bfd_control_buffer *bcs_bout;
};
TAILQ_HEAD(bcslist, bfd_control_socket);

int control_init(const char *path);
void control_shutdown(void);
int control_notify(struct bfd_session *bs);
int control_notify_config(const char *op, struct bfd_session *bs);
int control_accept(struct thread *t);


/*
 * bfdd.c
 *
 * Daemon specific code.
 */
struct bfd_global {
	int bg_shop;
	int bg_mhop;
	int bg_shop6;
	int bg_mhop6;
	int bg_echo;
	int bg_vxlan;
	struct thread *bg_ev[6];

	int bg_csock;
	struct thread *bg_csockev;
	struct bcslist bg_bcslist;

	struct pllist bg_pllist;
};
extern struct bfd_global bglobal;
extern struct bfd_diag_str_list diag_list[];
extern struct bfd_state_str_list state_list[];

void socket_close(int *s);


/*
 * config.c
 *
 * Contains the code related with loading/reloading configuration.
 */
int parse_config(const char *fname);
int config_request_add(const char *jsonstr);
int config_request_del(const char *jsonstr);
char *config_response(const char *status, const char *error);
char *config_notify(struct bfd_session *bs);
char *config_notify_config(const char *op, struct bfd_session *bs);

typedef int (*bpc_handle)(struct bfd_peer_cfg *, void *arg);
int config_notify_request(struct bfd_control_socket *bcs, const char *jsonstr,
			  bpc_handle bh);

struct peer_label *pl_new(const char *label, struct bfd_session *bs);
struct peer_label *pl_find(const char *label);
void pl_free(struct peer_label *pl);


/*
 * log.c
 *
 * Contains code that does the logging procedures. Might implement multiple
 * backends (e.g. zebra log, syslog or other logging lib).
 */
enum blog_level {
	/* level vs syslog equivalent */
	BLOG_DEBUG = 0,   /* LOG_DEBUG */
	BLOG_INFO = 1,    /* LOG_INFO */
	BLOG_WARNING = 2, /* LOG_WARNING */
	BLOG_ERROR = 3,   /* LOG_ERR */
	BLOG_FATAL = 4,   /* LOG_CRIT */
};

void log_init(int foreground, enum blog_level level,
	      struct frr_daemon_info *fdi);
void log_info(const char *fmt, ...);
void log_debug(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_error(const char *fmt, ...);
void log_fatal(const char *fmt, ...);


/*
 * bfd_packet.c
 *
 * Contains the code related with receiving/seding, packing/unpacking BFD data.
 */
int bp_set_ttlv6(int sd);
int bp_set_ttl(int sd);
int bp_set_tosv6(int sd);
int bp_set_tos(int sd);
int bp_bind_dev(int sd, const char *dev);

int bp_udp_shop(void);
int bp_udp_mhop(void);
int bp_udp6_shop(void);
int bp_udp6_mhop(void);
int bp_peer_socket(struct bfd_peer_cfg *bpc);
int bp_peer_socketv6(struct bfd_peer_cfg *bpc);

void ptm_bfd_snd(struct bfd_session *bfd, int fbit);
void ptm_bfd_echo_snd(struct bfd_session *bfd);

int bfd_recv_cb(struct thread *t);

uint16_t checksum(uint16_t *buf, int len);


/*
 * event.c
 *
 * Contains the code related with event loop.
 */
typedef void (*bfd_ev_cb)(struct thread *t);

void bfd_recvtimer_update(struct bfd_session *bs);
void bfd_echo_recvtimer_update(struct bfd_session *bs);
void bfd_xmttimer_update(struct bfd_session *bs, uint64_t jitter);
void bfd_echo_xmttimer_update(struct bfd_session *bs, uint64_t jitter);

void bfd_xmttimer_delete(struct bfd_session *bs);
void bfd_echo_xmttimer_delete(struct bfd_session *bs);
void bfd_recvtimer_delete(struct bfd_session *bs);
void bfd_echo_recvtimer_delete(struct bfd_session *bs);

void bfd_recvtimer_assign(struct bfd_session *bs, bfd_ev_cb cb, int sd);
void bfd_echo_recvtimer_assign(struct bfd_session *bs, bfd_ev_cb cb, int sd);
void bfd_xmttimer_assign(struct bfd_session *bs, bfd_ev_cb cb);
void bfd_echo_xmttimer_assign(struct bfd_session *bs, bfd_ev_cb cb);


/*
 * bfd.c
 *
 * BFD protocol specific code.
 */
struct bfd_session *ptm_bfd_sess_new(struct bfd_peer_cfg *bpc);
int ptm_bfd_ses_del(struct bfd_peer_cfg *bpc);
void ptm_bfd_ses_dn(struct bfd_session *bfd, uint8_t diag);
void ptm_bfd_ses_up(struct bfd_session *bfd);
void fetch_portname_from_ifindex(int ifindex, char *ifname, size_t ifnamelen);
void ptm_bfd_echo_stop(struct bfd_session *bfd, int polling);
void ptm_bfd_echo_start(struct bfd_session *bfd);
void ptm_bfd_xmt_TO(struct bfd_session *bfd, int fbit);
void ptm_bfd_start_xmt_timer(struct bfd_session *bfd, bool is_echo);
struct bfd_session *ptm_bfd_sess_find(struct bfd_pkt *cp, char *port_name,
				      struct sockaddr_any *peer,
				      struct sockaddr_any *local,
				      char *vrf_name, bool is_mhop);

struct bfd_session *bs_peer_find(struct bfd_peer_cfg *bpc);
int bfd_session_update_label(struct bfd_session *bs, const char *nlabel);
void bfd_set_polling(struct bfd_session *bs);
const char *satostr(struct sockaddr_any *sa);
const char *diag2str(uint8_t diag);
int strtosa(const char *addr, struct sockaddr_any *sa);
void integer2timestr(uint64_t time, char *buf, size_t buflen);
const char *bs_to_string(struct bfd_session *bs);

/* BFD hash data structures interface */
void bfd_initialize(void);
void bfd_shutdown(void);
struct bfd_session *bfd_id_lookup(uint32_t id);
struct bfd_session *bfd_shop_lookup(struct bfd_shop_key shop);
struct bfd_session *bfd_mhop_lookup(struct bfd_mhop_key mhop);
struct bfd_vrf *bfd_vrf_lookup(int vrf_id);
struct bfd_iface *bfd_iface_lookup(const char *ifname);

struct bfd_session *bfd_id_delete(uint32_t id);
struct bfd_session *bfd_shop_delete(struct bfd_shop_key shop);
struct bfd_session *bfd_mhop_delete(struct bfd_mhop_key mhop);
struct bfd_vrf *bfd_vrf_delete(int vrf_id);
struct bfd_iface *bfd_iface_delete(const char *ifname);

bool bfd_id_insert(struct bfd_session *bs);
bool bfd_shop_insert(struct bfd_session *bs);
bool bfd_mhop_insert(struct bfd_session *bs);
bool bfd_vrf_insert(struct bfd_vrf *vrf);
bool bfd_iface_insert(struct bfd_iface *iface);

typedef void (*hash_iter_func)(struct hash_backet *hb, void *arg);
void bfd_id_iterate(hash_iter_func hif, void *arg);
void bfd_shop_iterate(hash_iter_func hif, void *arg);
void bfd_mhop_iterate(hash_iter_func hif, void *arg);
void bfd_vrf_iterate(hash_iter_func hif, void *arg);
void bfd_iface_iterate(hash_iter_func hif, void *arg);

/* Export callback functions for `event.c`. */
extern struct thread_master *master;

int bfd_recvtimer_cb(struct thread *t);
int bfd_echo_recvtimer_cb(struct thread *t);
int bfd_xmt_cb(struct thread *t);
int bfd_echo_xmt_cb(struct thread *t);


/*
 * bfdd_vty.c
 *
 * BFD daemon vty shell commands.
 */
void bfdd_vty_init(void);


/*
 * ptm_adapter.c
 */
void bfdd_zclient_init(struct zebra_privs_t *bfdd_priv);
void bfdd_zclient_stop(void);

int ptm_bfd_notify(struct bfd_session *bs);


/*
 * OS compatibility functions.
 */
struct udp_psuedo_header {
	uint32_t saddr;
	uint32_t daddr;
	uint8_t reserved;
	uint8_t protocol;
	uint16_t len;
};

#define UDP_PSUEDO_HDR_LEN sizeof(struct udp_psuedo_header)

#if defined(BFD_LINUX) || defined(BFD_BSD)
int ptm_bfd_fetch_ifindex(const char *ifname);
void ptm_bfd_fetch_local_mac(const char *ifname, uint8_t *mac);
void fetch_portname_from_ifindex(int ifindex, char *ifname, size_t ifnamelen);
int ptm_bfd_echo_sock_init(void);
int ptm_bfd_vxlan_sock_init(void);
#endif /* BFD_LINUX || BFD_BSD */

#ifdef BFD_LINUX
uint16_t udp4_checksum(struct iphdr *iph, uint8_t *buf, int len);
#endif /* BFD_LINUX */

#ifdef BFD_BSD
uint16_t udp4_checksum(struct ip *ip, uint8_t *buf, int len);
ssize_t bsd_echo_sock_read(int sd, uint8_t *buf, ssize_t *buflen,
			   struct sockaddr_storage *ss, socklen_t *sslen,
			   uint8_t *ttl, uint32_t *id);
#endif /* BFD_BSD */

#endif /* _BFD_H_ */