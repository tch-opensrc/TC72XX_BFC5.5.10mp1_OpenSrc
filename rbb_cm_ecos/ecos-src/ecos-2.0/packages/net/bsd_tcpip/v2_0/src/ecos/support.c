//==========================================================================
//
//      src/ecos/support.c
//
//==========================================================================
//####BSDCOPYRIGHTBEGIN####
//
// -------------------------------------------
//
// Portions of this software may have been derived from OpenBSD, 
// FreeBSD or other sources, and are covered by the appropriate
// copyright disclaimers included herein.
//
// Portions created by Red Hat are
// Copyright (C) 2002 Red Hat, Inc. All Rights Reserved.
//
// Copyright (C) 2002 Gary Thomas
// -------------------------------------------
//
//####BSDCOPYRIGHTEND####
//==========================================================================

//==========================================================================
//
//      ecos/support.c
//
//      eCos wrapper and support functions
//
//==========================================================================
//####BSDCOPYRIGHTBEGIN####
//
// -------------------------------------------
//
// Portions of this software may have been derived from OpenBSD or other sources,
// and are covered by the appropriate copyright disclaimers included herein.
//
// -------------------------------------------
//
//####BSDCOPYRIGHTEND####
// ----------------------------------------------------------------------
// Portions of this software Copyright (c) 2003-2011 Broadcom Corporation
// ----------------------------------------------------------------------
//==========================================================================
//#####DESCRIPTIONBEGIN####
//
// Author(s):    gthomas, hmt
// Contributors: gthomas, hmt
// Date:         2000-01-10
// Purpose:      
// Description:  
//              
//
//####DESCRIPTIONEND####
//
//==========================================================================


// Support routines, etc., used by network code

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/sockio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/netisr.h>
#include <netinet/if_bcm.h> // cliffmod

#include <cyg/infra/diag.h>
#include <cyg/hal/hal_intr.h>
#include <cyg/kernel/kapi.h>

#include <cyg/infra/cyg_ass.h>

// BRCM ADD - RTE - required for statistics
#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <string.h>

#if !CYGPKG_NET_DRIVER_FRAMEWORK   // Interface
#error At least one network driver framework must be defined!
#else
#include <cyg/io/eth/netdev.h>

// Define table boundaries
CYG_HAL_TABLE_BEGIN( __NETDEVTAB__, netdev );
CYG_HAL_TABLE_END( __NETDEVTAB_END__, netdev );
CYG_HAL_TABLE_BEGIN( __NET_INIT_TAB__, _Net_inits );
CYG_HAL_TABLE_END( __NET_INIT_TAB_END__, _Net_inits );
extern struct init_tab_entry __NET_INIT_TAB__[], __NET_INIT_TAB_END__;

// Used for system-wide "ticks per second"
#undef ticks
int hz = 100;
int tick = 10000;  // usec per "tick"
volatile struct timeval ktime;
int proc = 0;  // unused
int proc0 = 0;  // unused

volatile struct timeval mono_time;

// BRCM ADD - RTE
// This is used to store off socket history that was added
// as a way to find root cause for socket depleters.
typedef struct
{
    int  sockfd;      // socket file descriptor
    char header[32];  // socket header
    char packet[128]; // socket payload
} socket_history_t;   // 160 bytes

#define MAX_HIST 50
socket_history_t sock_hist[MAX_HIST];  // 8k of history
char hist_index = 0;
char hist_one_time = 0;

// Low-level network debugging & logging
#ifdef CYGPKG_NET_FREEBSD_LOGGING
int cyg_net_log_mask = CYGPKG_NET_FREEBSD_LOGGING;
#endif

// begin cliffmod
//#define STACK_SIZE CYGNUM_HAL_STACK_SIZE_TYPICAL
#define STACK_SIZE CYGPKG_NET_THREAD_STACK_SIZE_BYTES
// end cliffmod
static char netint_stack[STACK_SIZE];
static cyg_thread netint_thread_data;
static cyg_handle_t netint_thread_handle;

cyg_flag_t netint_flags;  
#define NETISR_ANY 0xFFFFFFFF  // Any possible bit...

#if defined( CYGIMP_MEMALLOC_MALLOC_BCM ) // BRCM
    // Set to 1 for static "var" pool.  Set to 0 to use heap (malloc/free).
    #define STATIC_POOL 0
    #warning BRCM - Using system heap for variable-size pool!
#else
    #define STATIC_POOL 1
#endif

#if !STATIC_POOL
void *BcmHeapAlloc( size_t size );
void BcmHeapFree( void *p );
#endif

extern void cyg_test_exit(void);  // TEMP

void
cyg_panic(const char *msg, ...)
{
    cyg_uint32 old_ints;
    CYG_FAIL( msg );
    HAL_DISABLE_INTERRUPTS(old_ints);
    diag_printf("PANIC: %s\n", msg);
    cyg_test_exit();  // FIXME
}

// begin cliffmod - experiment with pool alloc ratios
// original code...
//#define NET_MEMPOOL_SIZE  roundup( CYGPKG_NET_MEM_USAGE / 4, MSIZE )
//#define NET_MBUFS_SIZE    roundup( CYGPKG_NET_MEM_USAGE / 4, MSIZE )
//#define NET_CLUSTERS_SIZE roundup( CYGPKG_NET_MEM_USAGE / 2, MCLBYTES )
//
// my mods...
#if STATIC_POOL
    #define NET_MEMPOOL_SIZE  roundup( CYGPKG_NET_MEM_USAGE / 2, MSIZE )
#endif
#define NET_MBUFS_SIZE    roundup( (CYGPKG_NET_MEM_USAGE * 3) / 16, MSIZE )
#define NET_CLUSTERS_SIZE roundup( (CYGPKG_NET_MEM_USAGE * 5) / 16, MCLBYTES )
// end cliffmod - experiment with pool alloc ratios

#if STATIC_POOL
static unsigned char net_mempool_area[NET_MEMPOOL_SIZE];
static cyg_mempool_var net_mem_pool;
static cyg_handle_t    net_mem;
#endif
static unsigned char net_mbufs_area[NET_MBUFS_SIZE];
static cyg_mempool_fix net_mbufs_pool;
static cyg_handle_t    net_mbufs;
static unsigned char net_clusters_area[NET_CLUSTERS_SIZE];
static cyg_mempool_fix net_clusters_pool;
static cyg_handle_t    net_clusters;
static char            net_clusters_refcnt[(NET_CLUSTERS_SIZE/MCLBYTES)+1];
int nmbclusters = (NET_CLUSTERS_SIZE/MCLBYTES);

#ifdef CYGDBG_NET_TIMING_STATS
static struct net_stats  stats_malloc, stats_free, 
    stats_memcpy, stats_memset,
    stats_mbuf_alloc, stats_mbuf_free, stats_cluster_alloc;
extern struct net_stats stats_in_cksum;

// Display a number of ticks as microseconds
// Note: for improved calculation significance, values are kept in ticks*1000
static long rtc_resolution[] = CYGNUM_KERNEL_COUNTERS_RTC_RESOLUTION;
static long ns_per_system_clock;

static void
show_ticks_in_us(cyg_uint32 ticks)
{
    long long ns;
    ns_per_system_clock = 1000000/rtc_resolution[1];
    ns = (ns_per_system_clock * ((long long)ticks * 1000)) / 
        CYGNUM_KERNEL_COUNTERS_RTC_PERIOD;
    ns += 5;  // for rounding to .01us
    diag_printf("%7d.%02d", (int)(ns/1000), (int)((ns%1000)/10));
}

void
show_net_stats(struct net_stats *stats, const char *title)
{
    int ave;
    ave = stats->total_time / stats->count;
    diag_printf("%s:\n", title);
    diag_printf("  count: %6d", stats->count);
    diag_printf(", min: ");
    show_ticks_in_us(stats->min_time);
    diag_printf(", max: ");
    show_ticks_in_us(stats->max_time);
    diag_printf(", total: ");
    show_ticks_in_us(stats->total_time);
    diag_printf(", ave: ");
    show_ticks_in_us(ave);
    diag_printf("\n");
    // Reset stats
    memset(stats, 0, sizeof(*stats));
}

void
show_net_times(void)
{
    show_net_stats(&stats_malloc,        "Net malloc");
    show_net_stats(&stats_free,          "Net free");
    show_net_stats(&stats_mbuf_alloc,    "Mbuf alloc");
    show_net_stats(&stats_mbuf_free,     "Mbuf free");
    show_net_stats(&stats_cluster_alloc, "Cluster alloc");
    show_net_stats(&stats_in_cksum,      "Checksum");
    show_net_stats(&stats_memcpy,        "Net memcpy");
    show_net_stats(&stats_memset,        "Net memset");
}
#endif /* CYGDBG_NET_TIMING_STATS */ 

void *
cyg_net_malloc(u_long size, int type, int flags)
{
    void *res;

    START_STATS();
    log(LOG_MDEBUG, "cyg_net_malloc() %d bytes @ ", size);
#if STATIC_POOL
    if (flags & M_NOWAIT) {
        res = cyg_mempool_var_try_alloc(net_mem, size);
    } else {
        res = cyg_mempool_var_alloc(net_mem, size);
    }
#else
    res = BcmHeapAlloc( size );
#endif
    if ((flags & M_ZERO) && res) {
      memset(res,0,size);
    }
    FINISH_STATS(stats_malloc);
    log(LOG_MDEBUG, "%p\n", res);
    return (res);
}

void 
cyg_net_free(caddr_t addr, int type)
{
    START_STATS();
    log(LOG_MDEBUG, "cyg_net_free() @ %p of type %d\n", addr, type);

#if STATIC_POOL
    cyg_mempool_var_free(net_mem, addr);
#else
    BcmHeapFree( addr );
#endif
    FINISH_STATS(stats_free);
}

#ifdef CYGDBG_NET_SHOW_MBUFS

struct mbuf *mbinfo[300];

void cyg_net_show_mbufs(void)
{
    int i;
    diag_printf(" MBUF   : TYPE FLGS     DATA[LEN]   NEXT    NEXTPKT\n");
    for( i = 0; i < 300; i++ )
    {
        struct mbuf *m = mbinfo[i];
        char *type;
        if( m == 0 ) continue;

        switch( m->m_hdr.mh_type )
        {
        case MT_FREE: type="FREE"; break;
        case MT_DATA: type="DATA"; break;
        case MT_HEADER: type="HEADER"; break;
        case MT_SONAME: type="SONAME"; break;
        case MT_FTABLE: type="FTABLE"; break;
        case MT_CONTROL: type="CONTROL"; break;
        case MT_OOBDATA: type="OOBDATA"; break;
        default: type="UNKNOWN"; break;
        }

        diag_printf("%08x: %s %04x %08x[%03d] %08x %08x\n",
                    m, type,
                    m->m_hdr.mh_flags,
                    m->m_hdr.mh_data,
                    m->m_hdr.mh_len,
                    m->m_hdr.mh_next,
                    m->m_hdr.mh_nextpkt);
    }
    diag_printf(" MBUF   : TYPE FLGS     DATA[LEN]   NEXT    NEXTPKT\n");    
}

#endif

void *
cyg_net_mbuf_alloc(void)
{
    void *res;    

    START_STATS();
    log(LOG_MDEBUG, "cyg_net_mbuf_alloc() @ ");
    res = cyg_mempool_fix_try_alloc(net_mbufs);
    FINISH_STATS(stats_mbuf_alloc);

#ifdef CYGDBG_NET_SHOW_MBUFS    
    {
        int i;
        for( i = 0; i < (sizeof(mbinfo)/sizeof(mbinfo[0])); i++ )
            if( mbinfo[i] == 0 )
            {
                mbinfo[i] = (struct mbuf *)res;
                break;
            }
    }
#endif

    // Check that this nastiness works OK
    CYG_ASSERT( dtom(res) == res, "dtom failed, base of mbuf" );
    CYG_ASSERT( dtom((char *)res + MSIZE/2) == res, "dtom failed, mid mbuf" );
    log(LOG_MDEBUG, "%p\n", res);
    return (res);
}


void *
cyg_net_cluster_alloc(void)
{
    void *res;

    START_STATS();
    log(LOG_MDEBUG, "cyg_net_cluster_alloc() @ ");
    res = cyg_mempool_fix_try_alloc(net_clusters);
    FINISH_STATS(stats_cluster_alloc);
    log(LOG_MDEBUG, "%p\n", res);
    return res;
}

static struct vm_zone *vm_zones = (struct vm_zone *)NULL;
elem *pfirst_socket_elem = NULL;

vm_zone_t 
zinit(char *name, int size, int nentries, int flags, int zalloc)
{
    void *res;
    vm_zone_t zone = (vm_zone_t)0;
    elem *p;

    log(LOG_MDEBUG, "zinit '%s', size: %d, num: %d, flags: %d, alloc: %d\n", 
        name, size, nentries, flags, zalloc);
#if STATIC_POOL
    res = cyg_mempool_var_try_alloc(net_mem, sizeof(struct vm_zone));
    if (res) {
        zone = (vm_zone_t)res;
        res = cyg_mempool_var_try_alloc(net_mem, size*nentries);
    }
#else
    res = BcmHeapAlloc( sizeof(struct vm_zone) );
    if (res) {
        zone = (vm_zone_t)res;
        res = BcmHeapAlloc( size*nentries );
    }
#endif
    if (!res) {
        log(LOG_MDEBUG, "Can't allocate memory for %s\n", name);
        panic("zinit: Out of memory\n");
    }

    // BRCM ADD - CRE - initialize the socket pool memory
    memset(res,0,size*nentries);

    p = (elem *)res;
    // begin cliffmod
    if( strcmp(name, "socket") == 0 )
    {
        pfirst_socket_elem = p;
    }
    // end cliffmod
    zone->pool = (elem *)0;
    zone->elem_size = size;
    zone->name = name;
    zone->free = zone->total = nentries;
    zone->next = vm_zones;
    zone->alloc_tries = zone->alloc_fails = zone->alloc_frees = 0;
    vm_zones = zone;
    while (nentries-- > 0) {
        p->next = zone->pool;
        zone->pool = p;
        p = (elem *)((char *)p + size);
    }
    p = zone->pool;
#if 0
    while (p) {
        log(LOG_MDEBUG, "p: %p, next: %p\n", p, p->next);
        p = p->next;
    }
#endif
    return zone;
}

void *    
zalloci(vm_zone_t zone)
{
    elem *p;

    p = zone->pool;
    zone->alloc_tries++;
    if (p) {
        zone->pool = p->next;
        zone->free--;        
    } else {
        zone->alloc_fails++;
    }
    log(LOG_MDEBUG, "zalloci from %s => %p\n", zone->name, p);
    return (void *)p;
}

void      
zfreei(vm_zone_t zone, void *item)
{
    elem *p = (elem *)item;

    log(LOG_MDEBUG, "zfreei to %s <= %p\n", zone->name, p);
    p->next = zone->pool;
    zone->pool = p;
    zone->free++;
    zone->alloc_frees++;
}

static void
cyg_kmem_init(void)
{
    unsigned char *p;
#ifdef CYGPKG_NET_DEBUG
    diag_printf("Network stack using %d bytes for misc space\n", NET_MEMPOOL_SIZE);
    diag_printf("                    %d bytes for mbufs\n", NET_MBUFS_SIZE);
    diag_printf("                    %d bytes for mbuf clusters\n", NET_CLUSTERS_SIZE);
#endif
#if STATIC_POOL
    cyg_mempool_var_create(&net_mempool_area,
                           NET_MEMPOOL_SIZE,
                           &net_mem,
                           &net_mem_pool);
#endif
    // Align the mbufs on MSIZE boudaries so that dtom() can work.
    p = (unsigned char *)(((long)(&net_mbufs_area) + MSIZE - 1) & ~(MSIZE-1));
    cyg_mempool_fix_create(p,
                           ((&(net_mbufs_area[NET_MBUFS_SIZE])) - p) & ~(MSIZE-1),
                           MSIZE,
                           &net_mbufs,
                           &net_mbufs_pool);
    cyg_mempool_fix_create(&net_clusters_area,
                           NET_CLUSTERS_SIZE,
                           MCLBYTES,
                           &net_clusters,
                           &net_clusters_pool);
    mbutl = (struct mbuf *)&net_clusters_area;
    mclrefcnt = net_clusters_refcnt;
}

void cyg_kmem_print_stats( void )
{
    cyg_mempool_info info;
    struct vm_zone *zone;

    printf( "Network stack mbuf stats:\n" );
    printf( "   mbufs %d, clusters %d, free clusters %d\n",
            mbstat.m_mbufs,	/* mbufs obtained from page pool */
            mbstat.m_clusters,	/* clusters obtained from page pool */
            /* mbstat.m_spare, */	/* spare field */
            mbstat.m_clfree	/* free clusters */
        );
    printf( "   Failed to get %d times\n"
            "   Waited to get %d times\n"
            "   Drained queues to get %d times\n",
            mbstat.m_drops,	/* times failed to find space */
            mbstat.m_wait, 	/* times waited for space */
            mbstat.m_drain         /* times drained protocols for space */
            /* mbstat.m_mtypes[256]; type specific mbuf allocations */
        );

    zone = vm_zones;
    while (zone) {
        printf("VM zone '%s':\n", zone->name);
        printf("  ElemSize: %d, Total: %d, Free: %d, Allocs: %d, Frees: %d, Fails: %d\n",
               zone->elem_size, zone->total, zone->free,
               zone->alloc_tries, zone->alloc_frees, zone->alloc_fails);
        zone = zone->next;
    }

#if STATIC_POOL
    cyg_mempool_var_get_info( net_mem, &info );
    printf( "Misc mpool: total %7d, free %7d, max free block %d\n",
            info.totalmem,
            info.freemem,
            info.maxfree
        );
#endif

    cyg_mempool_fix_get_info( net_mbufs, &info );
    printf( "Mbufs pool: total %7d, free %7d, blocksize %4d\n",
            info.totalmem,
            info.freemem,
            info.blocksize
        );


    cyg_mempool_fix_get_info( net_clusters, &info );
    printf( "Clust pool: total %7d, free %7d, blocksize %4d\n",
            info.totalmem,
            info.freemem,
            info.blocksize
        );
}


char* socket_type_name( unsigned short sock_type )
{
    char* sock_names[] = 
    {
        "none ",
        "strm ",
        "dgram",
        "raw  ",
        "rdm  ",
        "seqp "
    };

    if( sock_type <= SOCK_SEQPACKET )
    {
        return sock_names[ sock_type ];
    }
    return sock_names[ 0 ];
}
    

// BRCM ADD - RTE - CLI to see internal UDP counters
// NOTE: use printf so we can see output on telnet/ssh instead of just the console
void _ecos_stats( void )
{
    // IP STATISTICS
    printf("%s\n", "IP STATS -- Received" );
    printf("%s  : %ld\n", "Total", ipstat.ips_total );

    printf("%s    : %ld\n", "Bad", ipstat.ips_badsum+ipstat.ips_tooshort+ipstat.ips_toosmall+
                                    ipstat.ips_badhlen+ipstat.ips_badlen+ipstat.ips_noproto+ipstat.ips_toolong);

    printf("Reassem: %ld\n",ipstat.ips_reassembled);
    printf("Delverd: %ld\n",ipstat.ips_delivered);

    printf("Bad Sum: %ld\n",ipstat.ips_badsum);
    printf("TooShrt: %ld\n",ipstat.ips_tooshort);
    printf("TooSmll: %ld\n",ipstat.ips_toosmall);

    printf("BadHLen: %ld\n",ipstat.ips_badhlen);
    printf("Bad Len: %ld\n",ipstat.ips_badlen);
    printf("NoProto: %ld\n",ipstat.ips_noproto);
    printf("TooLong: %ld\n",ipstat.ips_toolong);

    printf("\n%s\n", "Sent" );                    
    printf("%s  : %ld\n", "Total",ipstat.ips_localout );
    printf("%s    : %ld\n", "Raw",ipstat.ips_rawout );
    printf("%s : %ld\n", "Fragmt",ipstat.ips_fragmented );

    // TCP STATISTICS
    printf("\n%s\n", "TCP STATS -- Connections" );

    printf("Initatd: %ld\n",tcpstat.tcps_connattempt);
    printf("Acepted: %ld\n",tcpstat.tcps_accepts);
    printf("Estblsh: %ld\n",tcpstat.tcps_connects);
    printf("Closed : %ld\n",tcpstat.tcps_closed);

    printf("\n%s\n", "Received" );                    

    printf("Total  : %ld\n",tcpstat.tcps_rcvtotal);
    printf("Packets: %ld\n",tcpstat.tcps_rcvpack);
    printf("Bytes  : %ld\n",tcpstat.tcps_rcvbyte);


    printf("\n%s\n", "Sent" );                    

    printf("Total  : %ld\n",tcpstat.tcps_sndtotal);
    printf("Packets: %ld\n",tcpstat.tcps_sndpack);
    printf("Bytes  : %ld\n",tcpstat.tcps_sndbyte);

    // UDP STATISTICS
    printf("\n%s\n", "UDP STATS -- Received" );
    printf("%s  : %ld\n", "Total", udpstat.udps_ipackets );
    printf("%s    : %ld\n", "Bad", udpstat.udps_hdrops+udpstat.udps_badsum+udpstat.udps_badlen+
                                    udpstat.udps_noport+udpstat.udps_noportbcast+udpstat.udps_fullsock);
    printf("Drops  : %ld\n",udpstat.udps_hdrops);
    printf("Bad Sum: %ld\n",udpstat.udps_badsum);
    printf("Bad Len: %ld\n",udpstat.udps_badlen);
    printf("No Port: %ld\n",udpstat.udps_noport);
    printf("No Brdc: %ld\n",udpstat.udps_noportbcast);
    printf("Full   : %ld\n",udpstat.udps_fullsock);

    printf("\n%s\n", "Sent" );                    
    printf("%s  : %ld\n", "Total",udpstat.udps_opackets );
}


// BRCM ADD - RTE
// This is an emergency clean up routine to get us out of the trenches.
// This was added to clean up an issue where someone is draining mbufs/clusters
// by not servicing their socket.  Instead of just having a dead eCos stack we
// will attempt to clean up the condition to we still operate.
void _flush_sockets( void )
{
    elem *pelem;
    struct socket *psocket;
    int error;

    int num_sockets = socket_zone->total;
    pelem = pfirst_socket_elem;

    // Walk all our sockets and any that have pending data in
    // either the recv or send buffers we will flush
    while( (num_sockets-- > 0) && (pelem != NULL) )
    {
        psocket = (struct socket*)pelem;

        if (sblock(&psocket->so_rcv, M_WAITOK))
        {
            panic("_flush_sockets rcv");
        }
        cyg_interrupt_disable();
        sbunlock(&psocket->so_rcv);
        // Flush the receive socket buffers
        if (psocket->so_rcv.sb_cc > 0)
        {
            sbflush(&psocket->so_rcv);
        }
        cyg_interrupt_enable();

        // Flush the send socket buffers
        if (sblock(&psocket->so_snd, M_WAITOK))
        {
            panic("_flush_sockets snd");
        }
        cyg_interrupt_disable();
        sbunlock(&psocket->so_snd);
        if (psocket->so_snd.sb_cc > 0)
        {
            sbflush(&psocket->so_snd);
        }
        cyg_interrupt_enable();

        pelem = (elem*)((char*)pelem + socket_zone->elem_size);
    }
}


// BRCM ADD - RTE
// This is an emergency tracking routine to be looked at later to help solve our
// socket depletion issue.
// This was added to clean up an issue where someone is draining mbufs/clusters
// by not servicing their socket.  
// NOTE: use printf so we can see output on telnet/ssh instead of just the console
void _dump_history( void )
{
    int i,t;

    printf("Current Index: %d\n", hist_index);

    for (t=0; t < MAX_HIST; t++)
    {
        printf("\n%d] Socket FD: 0x%x\n", t, sock_hist[t].sockfd );
        printf("%d] Dumping Header (32 bytes)\n", t);

        // Dump the 32 byte header
        for( i = 0; i < 32; i++ )
        {
            // print each data byte as (2) hex digits followed by a space.
            printf( "%02x%s",(unsigned char)sock_hist[t].header[i], " " );

            if( !((i+1) % 16) )
            {       
                // go to new line after 16 bytes...
                printf( "\n" );
            }           
            else if( !((i+1) % 4) )
            {
                // insert an extra space after every 4th byte as visual que.
                printf( "  " );
            }
        }           

        // Dump first 128 bytes of data
        printf("\n%d] Dumping Data (128 bytes)\n", t);

        // Dump the header
        for( i = 0; i < 128; i++ )
        {
            // print each data byte as (2) hex digits followed by a space.
            printf( "%02x%s", (unsigned char)sock_hist[t].packet[i], " " );

            if( !((i+1) % 16) )
            {
                // go to new line after 16 bytes...
                printf( "\n" );
            }
            else if( !((i+1) % 4) )
            {
                // insert an extra space after every 4th byte as visual que.
                printf( "  " );
            }
        }
    }
}



void _store_socket_history( void )
{
    elem *pelem;
    struct socket *psocket;
    struct mbuf *pmbuf;
    int i;

    cyg_scheduler_lock();

    // check if we need to zero out the data (one time)
    if (hist_one_time == 0)
    {
        memset(sock_hist, sizeof(socket_history_t)*MAX_HIST, 0);

        hist_one_time = 1;
    }

    int num_sockets = socket_zone->total;
    pelem = pfirst_socket_elem;
    while( (num_sockets-- > 0) && (pelem != NULL) )
    {
        psocket = (struct socket*)pelem;

        // pull out the mbuf
        pmbuf = psocket->so_rcv.sb_mb;

        // Perform some sanity checking
        if ( (psocket->so_rcv.sb_cc > 0) && (pmbuf > 0) && ( ((int)pmbuf) & 0x80000000) )
        {
            if (pmbuf->m_data > 0)
            {            
                // Flush the receive socket buffers
                diag_printf("\nStoring Socket Handle into slot: %d..\n", hist_index);

                sock_hist[hist_index].sockfd = (unsigned int)psocket->so_pcb;

                diag_printf("Storing Header into slot: %d..\n", hist_index);

                // check for rollover
                if (hist_index == MAX_HIST)
                {
                    hist_index = 0;
                }

                // Dump the 32 byte header
                for (i=0; i<32; i++)
                {
                    sock_hist[hist_index].header[i] = (unsigned char)pmbuf->m_data[i];
                }

                // Dump first 128 bytes of data
                if (pmbuf->m_next)
                {
                    diag_printf("Storing Data into slot: %d..\n", hist_index);
                    pmbuf =  pmbuf->m_next;

                    // Dump the header
                    for (i=0; i<128; i++)
                    {
                        sock_hist[hist_index].packet[i] = pmbuf->m_data[i];
                    }
                }

                // switch to next slot
                hist_index++;
            }
        }

        pelem = (elem*)((char*)pelem + socket_zone->elem_size);
    }
    cyg_scheduler_unlock();
}

void _show_sockets( void )
{
    cyg_scheduler_lock();

    struct vm_zone *zone = socket_zone;
    cyg_mempool_info info;
    elem *pelem;
    struct socket *psocket;

    int num_sockets = socket_zone->total;

    // summary info formatting copied from cyg_kmem_print_stats()
    diag_printf("VM zone '%s':\n", zone->name);
    diag_printf("  Total: %d, Free: %d, Allocs: %d, Frees: %d, Fails: %d\n",
        zone->total, zone->free, zone->alloc_tries, zone->alloc_frees, zone->alloc_fails);

#if STATIC_POOL
    cyg_mempool_var_get_info( net_mem, &info );
    diag_printf( "Misc mpool: total %7d, free %7d, max free block %d\n",
                 info.totalmem,
                 info.freemem,
                 info.maxfree
        );
#endif

    // socket detail info.
    diag_printf( "Socket Info:\n" );
    diag_printf( "Type    State         Owning PCB    LingerTO    ConnTO    Gen Count\n" );   
    diag_printf( "=====   ==========    ==========    ========    ======    =========\n" );   

    pelem = pfirst_socket_elem;
    while( (num_sockets-- > 0) && (pelem != NULL) )
    {
        psocket = (struct socket*)pelem;

        diag_printf( "%s   0x%08x    0x%08x      %06d    %06d     %08d\n", socket_type_name( psocket->so_type ),
            (int)psocket->so_state, (unsigned int)psocket->so_pcb,
            (short)psocket->so_linger, (short)psocket->so_timeo, (unsigned int)psocket->so_gencnt );

        pelem = (elem*)((char*)pelem + socket_zone->elem_size);
    }
    cyg_scheduler_unlock();

}



// This API is for our own automated network tests.  It's not in any header
// files because it's not at all supported.
int cyg_net_get_mem_stats( int which, cyg_mempool_info *p )
{
    CYG_CHECK_DATA_PTR( p, "Bad pointer to mempool_info" );
    CYG_ASSERT( 0 <= which, "Mempool selector underflow" );
    CYG_ASSERT( 2 >=which, "Mempool selector overflow" );
    
    if ( p )
        switch ( which ) {
        case 0:
#if STATIC_POOL
            cyg_mempool_var_get_info( net_mem, p );
#else
            return 0;
#endif
            break;
        case 1:
            cyg_mempool_fix_get_info( net_mbufs, p );
            break;
        case 2:
            cyg_mempool_fix_get_info( net_clusters, p );
            break;
        default:
            return 0;
        }
    return (int)p;
}

int
cyg_mtocl(u_long x)
{
    int res;
    res = (((u_long)(x) - (u_long)mbutl) >> MCLSHIFT);
    return res;
}

struct mbuf *
cyg_cltom(u_long x)
{
    struct mbuf *res;
    res = (struct mbuf *)((caddr_t)((u_long)mbutl + ((u_long)(x) << MCLSHIFT)));
    return res;
}

externC void 
net_memcpy(void *d, void *s, int n)
{
    START_STATS();
    memcpy(d, s, n);
    FINISH_STATS(stats_memcpy);
}

externC void 
net_memset(void *s, int v, int n)
{
    START_STATS();
    memset(s, v, n);
    FINISH_STATS(stats_memset);
}

// Rather than bring in the whole BSD 'random' code...
int
arc4random(void)
{
    cyg_uint32 res;
    static unsigned long seed = 0xDEADB00B;
    HAL_CLOCK_READ(&res);  // Not so bad... (but often 0..N where N is small)
    seed = ((seed & 0x007F00FF) << 7) ^
        ((seed & 0x0F80FF00) >> 8) ^ // be sure to stir those low bits
        (res << 13) ^ (res >> 9);    // using the clock too!
    return (int)seed;
}

void 
get_random_bytes(void *buf, size_t len)
{
    unsigned long ranbuf, *lp;
    lp = (unsigned long *)buf;
    while (len > 0) {
        ranbuf = arc4random();
        *lp++ = ranbuf;
        len -= sizeof(ranbuf);
    }
}

void
read_random_unlimited(void *buf, size_t len)
{
    get_random_bytes(buf, len);
}

void 
microtime(struct timeval *tp)
{
    *tp = ktime;
    log(LOG_DEBUG, "%s: = %d.%d\n", __FUNCTION__, tp->tv_sec, tp->tv_usec);
    ktime.tv_usec++;  // In case clock isn't running yet
}

void 
getmicrotime(struct timeval *tp)
{
    *tp = ktime;
    log(LOG_DEBUG, "%s: = %d.%d\n", __FUNCTION__, tp->tv_sec, tp->tv_usec);
    ktime.tv_usec++;  // In case clock isn't running yet
}

void 
getmicrouptime(struct timeval *tp)
{
    *tp = ktime;
    log(LOG_DEBUG, "%s: = %d.%d\n", __FUNCTION__, tp->tv_sec, tp->tv_usec);
    ktime.tv_usec++;  // In case clock isn't running yet
}

// Taken from kern/kern_clock.c
/*
 * Compute number of ticks in the specified amount of time.
 */
#ifndef LONG_MAX
#define LONG_MAX 0x7FFFFFFF
#endif
int
tvtohz(struct timeval *tv)
{
        register unsigned long ticks;
        register long sec, usec;

        /*
         * If the number of usecs in the whole seconds part of the time
         * difference fits in a long, then the total number of usecs will
         * fit in an unsigned long.  Compute the total and convert it to
         * ticks, rounding up and adding 1 to allow for the current tick
         * to expire.  Rounding also depends on unsigned long arithmetic
         * to avoid overflow.
         *
         * Otherwise, if the number of ticks in the whole seconds part of
         * the time difference fits in a long, then convert the parts to
         * ticks separately and add, using similar rounding methods and
         * overflow avoidance.  This method would work in the previous
         * case but it is slightly slower and assumes that hz is integral.
         *
         * Otherwise, round the time difference down to the maximum
         * representable value.
         *
         * If ints have 32 bits, then the maximum value for any timeout in
         * 10ms ticks is 248 days.
         */
        sec = tv->tv_sec;
        usec = tv->tv_usec;
        if (usec < 0) {
                sec--;
                usec += 1000000;
        }
        if (sec < 0) {
#ifdef DIAGNOSTIC
                if (usec > 0) {
                        sec++;
                        usec -= 1000000;
                }
                printf("tvotohz: negative time difference %ld sec %ld usec\n",
                       sec, usec);
#endif
                ticks = 1;
        } else if (sec <= LONG_MAX / 1000000)
                ticks = (sec * 1000000 + (unsigned long)usec + (tick - 1))
                        / tick + 1;
        else if (sec <= LONG_MAX / hz)
                ticks = sec * hz
                        + ((unsigned long)usec + (tick - 1)) / tick + 1;
        else
                ticks = LONG_MAX;
        if (ticks > INT_MAX)
                ticks = INT_MAX;
        return ((int)ticks);
}

void
get_mono_time(void)
{
    panic("get_mono_time");
}

void 
csignal(pid_t pgid, int signum, uid_t uid, uid_t euid)
{
    panic("csignal");
}

int
bcmp(const void *_p1, const void *_p2, size_t len)
{
    int res = 0;
    unsigned char *p1 = (unsigned char *)_p1;
    unsigned char *p2 = (unsigned char *)_p2;
    while (len-- > 0) {
        res = *p1++ - *p2++;
        if (res) break;
    }
    return res;
}

int
copyout(const void *s, void *d, size_t len)
{
    memcpy(d, s, len);
    return 0;
}

int
copyin(const void *s, void *d, size_t len)
{
    memcpy(d, s, len);
    return 0;
}

void
ovbcopy(const void *s, void *d, size_t len)
{
    memmove(d, s, len);
}


// ------------------------------------------------------------------------
// THE NETWORK THREAD ITSELF
//
// Network software interrupt handler
//   This function is run as a separate thread to allow
// processing of network events (mostly incoming packets)
// at "user level" instead of at interrupt time.
//
// The actual handlers are 'registered' at system startup
//

// The set of handlers
static netisr_t *_netisr_handlers[NETISR_MAX+1];
struct ifqueue  ipintrq;
#ifdef INET6
struct ifqueue  ip6intrq;
#endif
char *hostname = "eCos_node";

// Register a 'netisr' handler for a given level
int 
register_netisr(int level, netisr_t *fun)
{
    CYG_ASSERT(level <= NETISR_MAX, "invalid netisr level");
    CYG_ASSERT(_netisr_handlers[level] == 0, "re-registered netisr");
    _netisr_handlers[level] = fun;
    return 0;  // ignored
}

//int unregister_netisr __P((int));

static void
cyg_netint(cyg_addrword_t param)
{
    cyg_flag_value_t curisr;
    int lvl, spl;

    while (true) {
        curisr = cyg_flag_wait(&netint_flags, NETISR_ANY, 
                               CYG_FLAG_WAITMODE_OR|CYG_FLAG_WAITMODE_CLR);
        spl = splsoftnet(); // Prevent any overlapping "stack" processing
        for (lvl = NETISR_MIN;  lvl <= NETISR_MAX;  lvl++) {
            if (curisr & (1<<lvl)) {
                CYG_ASSERT(_netisr_handlers[lvl] != 0, "unregistered netisr handler");
                (*_netisr_handlers[lvl])();
            }
        }
        splx(spl);
    }
}


// This just sets one of the pseudo-ISR bits used above.
void
setsoftnet(void)
{
    // This is called if we are out of MBUFs - it doesn't do anything, and
    // that situation is handled OK, so don't bother with the diagnostic:

    // diag_printf("setsoftnet\n");

    // No need to do this because it is ignored anyway:
    // schednetisr(NETISR_SOFTNET);
}


/* Update the kernel globel ktime. */
static void 
cyg_ktime_func(cyg_handle_t alarm,cyg_addrword_t data)
{
    cyg_tick_count_t now = cyg_current_time();

    ktime.tv_usec = (now % hz) * tick;
    ktime.tv_sec = 1 + now / hz;
}

static void
cyg_ktime_init(void)
{
    cyg_handle_t ktime_alarm_handle;
    static cyg_alarm ktime_alarm;
    cyg_handle_t counter;

    // Do not start at 0 - net stack thinks 0 an invalid time;
    // Have a valid time available from right now:
    ktime.tv_usec = 0;
    ktime.tv_sec = 1;

    cyg_clock_to_counter(cyg_real_time_clock(),&counter);
    cyg_alarm_create(counter,
                     cyg_ktime_func,
                     0,
                     &ktime_alarm_handle,
                     &ktime_alarm);

    /* We want one alarm every 10ms. */
    cyg_alarm_initialize(ktime_alarm_handle,cyg_current_time()+1,1);
    cyg_alarm_enable(ktime_alarm_handle);
}

int
cyg_ticks(void)
{
    cyg_tick_count_t now = cyg_current_time();
    return (int)now;
}

//
// Network initialization
//   This function is called during system initialization to setup the whole
// networking environment.

// Linker magic to execute this function as 'init'
extern void cyg_do_net_init(void);

extern void ifinit(void);
extern void loopattach(int);
extern void bridgeattach(int);

// Internal init functions:
extern void cyg_alarm_timeout_init(void);
extern void cyg_tsleep_init(void);

static void
cyg_net_init_devs(void *ignored)
{
    // begin cliffmod
//    diag_printf( "enter cyg_net_init_devs()\n" );
    // end cliffmod
    cyg_netdevtab_entry_t *t;
    // Initialize all network devices
    for (t = &__NETDEVTAB__[0]; t != &__NETDEVTAB_END__; t++) {
        log(LOG_INIT, "Init device '%s'\n", t->name); 
        if (t->init(t)) {
            t->status = CYG_NETDEVTAB_STATUS_AVAIL;
        } else {
            // What to do if device init fails?
            t->status = 0;  // Device not [currently] available
        }
    }
#if 0  // Bridge code not available yet
#if NBRIDGE > 0
    bridgeattach(0);
#endif
#endif
    // begin cliffmod
//    diag_printf( "exit cyg_net_init_devs()\n" );
    // end cliffmod
}
SYSINIT(devs, SI_SUB_DEVICES, SI_ORDER_FIRST, cyg_net_init_devs, NULL)

void
cyg_net_init(void)
{
    static int _init = false;
    struct init_tab_entry *init_entry;

    // begin cliffmod
//    diag_printf( "enter cyg_net_init()\n" );
    // end cliffmod

    if (_init) return;

    cyg_do_net_init();  // Just forces linking in the initializer/constructor
    // Initialize interrupt "flags"
    cyg_flag_init(&netint_flags);
    // Initialize timeouts and net service thread (pseudo-DSRs)
    cyg_alarm_timeout_init();
    // Initialize tsleep/wakeup support
    cyg_tsleep_init();
    // Initialize network memory system
    cyg_kmem_init();
    // Initialize network time
    cyg_ktime_init();
    // Create network background thread
    cyg_thread_create(CYGPKG_NET_THREAD_PRIORITY, // Priority
                      cyg_netint,               // entry
                      0,                        // entry parameter
                      "Network support",        // Name
                      &netint_stack[0],         // Stack
                      STACK_SIZE,               // Size
                      &netint_thread_handle,    // Handle
                      &netint_thread_data       // Thread data structure
        );
    cyg_thread_resume(netint_thread_handle);    // Start it

    // Run through dynamic initializers
    for (init_entry = __NET_INIT_TAB__; init_entry != &__NET_INIT_TAB_END__;  init_entry++) {
        log(LOG_INIT, "[%s] Init: %s(%p)\n", __FUNCTION__, init_entry->name, init_entry->data);
        (*init_entry->fun)(init_entry->data);
    }
    log(LOG_INIT, "[%s] Done\n", __FUNCTION__);   // cliffmod

    // Done
    _init = true;

    // begin cliffmod
//    diag_printf( "exit cyg_net_init()\n" );
    // end cliffmod
}


#include <net/if.h>
#include <net/netdb.h>
#include <net/route.h>
externC void if_indextoname(int indx, char *buf, int len);

typedef void pr_fun(char *fmt, ...);

// begin cliffmod
// adapt some debug code from openbsd netinet/if_ether.c
void _show_ifa( struct ifaddr *ifa, pr_fun *pr );
void _show_llinfo( caddr_t li, pr_fun *pr );
int _show_radix_node( struct radix_node *rn, void *vpr );
int _show_arp_ip_to_mac_assoc( struct radix_node *rn, pr_fun *pr );
void _show_sockaddr_in( struct sockaddr_in *sin, pr_fun *pr );

#ifdef INET6
void _show_sockaddr_in6( struct sockaddr_in6 *sin6, pr_fun *pr );
void _dump_in6_addr(struct in6_addr * in6_addr, pr_fun *pr);
#endif

void _show_sockaddr_dl( struct sockaddr_dl *sdl, pr_fun *pr );
void _show_sockaddr( struct sockaddr *sa, pr_fun *pr );
void _show_smart_sockaddr( struct sockaddr *sa, pr_fun *pr );
void _show_summary_arp_table( pr_fun *pr );
void _show_routing_table_details( pr_fun *pr );

void _show_mbuf( struct mbuf *pmbuf, pr_fun *pr );
void _show_single_mbuf( struct mbuf *pmbuf, pr_fun *pr );
void _show_mbuf_hdr( struct mbuf *pmbuf, pr_fun *pr );

void _show_ifa( struct ifaddr *ifa, pr_fun *pr )
{
	if( !ifa )
    {
		(*pr)("[NULL]\n");
		return;
    }

    (*pr)("    ifa_addr:   ");
    _show_smart_sockaddr( ifa->ifa_addr, pr );

    (*pr)("    ifa_dstaddr:");
    _show_smart_sockaddr( ifa->ifa_dstaddr, pr );

    (*pr)("    ifa_netmask:");
    _show_smart_sockaddr( ifa->ifa_netmask, pr );

    (*pr)("    ifa_flags=0x%x  ifa_refcnt=%d  ifa_metric=%d\n",
	    ifa->ifa_flags, ifa->ifa_refcnt, ifa->ifa_metric);
}

// cliffmod UNFINISHED - move struct declaration to header file!!!!
struct llinfo_arp {
	LIST_ENTRY(llinfo_arp) la_le;
	struct	rtentry *la_rt;
	struct	mbuf *la_hold[32];	/* last 32 packets until resolved/timeout */
	long	la_asked;		/* last time we QUERIED for this addr */
#define la_timer la_rt->rt_rmx.rmx_expire /* deletion time in seconds */
};

void _show_llinfo( caddr_t li, pr_fun *pr )
{
	struct llinfo_arp *la;

	if( !li )
    {
		(*pr)("[NULL]\n");
		return;
    }

	la = (struct llinfo_arp *)li;
	(*pr)("  la_rt=%p  la_hold=%p  la_asked=0x%lx\n", la->la_rt, la->la_hold[0], la->la_asked);
}

int _show_radix_node( struct radix_node *rn, void *vpr )
{
	struct rtentry *rt = (struct rtentry *)rn;
    pr_fun *pr = (pr_fun *)vpr;
    int column_last_flag_digit = 14;

    if( !rn )
    {
		(*pr)("[NULL]\n");
        return 0;
    }

	(*pr)("rtentry@%p\n", rt);

	(*pr)("  flags=0x%x ( ", rt->rt_flags );  
    if( rt->rt_flags & RTF_UP )
    {
        (*pr)("UP ");
        column_last_flag_digit += 3;    // max 14
    }
    if( rt->rt_flags & RTF_GATEWAY )
    {
        (*pr)("GATEWAY ");
        column_last_flag_digit += 8;    // max 22
    }
    if( rt->rt_flags & RTF_HOST )
    {
        (*pr)("HOST ");
        column_last_flag_digit += 5;    // max 37
    }
    if( rt->rt_flags & RTF_REJECT )
    {
        (*pr)("REJECT ");
        column_last_flag_digit += 7;    // max 44
    }
    if( rt->rt_flags & RTF_DYNAMIC )
    {
        (*pr)("DYNAMIC ");
        column_last_flag_digit += 8;    // max 52
    }
    if( rt->rt_flags & RTF_MODIFIED )
    {
        (*pr)("MODIFIED ");
        column_last_flag_digit += 9;    // max 61
    }
    if( rt->rt_flags & RTF_DONE )
    {
        (*pr)("DONE ");
        column_last_flag_digit += 5;    // max 66
    }
    if( rt->rt_flags & RTF_DELCLONE )
    {
        (*pr)("DELCLONE ");
        column_last_flag_digit += 9;    // max 75
    }
    if( rt->rt_flags & RTF_CLONING )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("CLONING ");
        column_last_flag_digit += 8;    // max 22
    }
    if( rt->rt_flags & RTF_XRESOLVE )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("XRESOLVE ");
        column_last_flag_digit += 9;    // max 31
    }
    if( rt->rt_flags & RTF_LLINFO )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("LLINFO ");
        column_last_flag_digit += 7;    // max 38
    }
    if( rt->rt_flags & RTF_STATIC )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("STATIC ");
        column_last_flag_digit += 7;    // max 45
    }
    if( rt->rt_flags & RTF_BLACKHOLE )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("BLACKHOLE ");
        column_last_flag_digit += 10;    // max 55
    }
    if( rt->rt_flags & RTF_PROTO2 )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("PROTO2 ");
        column_last_flag_digit += 7;    // max 62
    }
    if( rt->rt_flags & RTF_PROTO1 )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("PROTO1 ");
        column_last_flag_digit += 7;    // max 75
    }
    if( rt->rt_flags & RTF_PRCLONING )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("PRCLONING ");
        column_last_flag_digit += 10;    // max 24
    }
    if( rt->rt_flags & RTF_WASCLONED )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("WASCLONED ");
        column_last_flag_digit += 10;    // max 34
    }
    if( rt->rt_flags & RTF_PROTO3 )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("PROTO3 ");
        column_last_flag_digit += 7;    // max 41
    }
    if( rt->rt_flags & RTF_PINNED )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("PINNED ");
        column_last_flag_digit += 7;    // max 48
    }
    if( rt->rt_flags & RTF_LOCAL )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("LOCAL ");
        column_last_flag_digit += 6;    // max 54
    }
    if( rt->rt_flags & RTF_BROADCAST )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("BROADCAST ");
        column_last_flag_digit += 10;    // max 64
    }
    if( rt->rt_flags & RTF_MULTICAST )
    {
        if( column_last_flag_digit > 70 )
        {
            (*pr)("\n              ");
            column_last_flag_digit = 14;
        }
        (*pr)("MULTICAST ");
        column_last_flag_digit += 10;    // max 74
    }

    (*pr)(")\n");
    

    (*pr)("  refcnt=%d  use=%ld\n",
	    rt->rt_refcnt, rt->rt_use);

	(*pr)("  rt_key="); 
//    _show_sa(rt_key(rt), pr);
    _show_smart_sockaddr(rt_key(rt), pr);

	(*pr)("  rt_mask="); 
//    _show_sa(rt_mask(rt), pr);
    _show_smart_sockaddr(rt_mask(rt), pr);

	(*pr)("  rt_gateway="); 
//    _show_sa(rt->rt_gateway, pr);
    _show_smart_sockaddr(rt->rt_gateway, pr);

	(*pr)("  rt_ifp@%p ", rt->rt_ifp);
	if( rt->rt_ifp->if_name )
    {
		(*pr)("(%s%d)", rt->rt_ifp->if_name, rt->rt_ifp->if_unit);
    }
    (*pr)("\n");

	(*pr)("  rt_ifa@%p\n", rt->rt_ifa);
    if( rt->rt_ifa )
    {
	    _show_ifa(rt->rt_ifa, pr);
//	_show_smart_sockaddr(rt->rt_ifa, pr);
    }

	(*pr)("  rt_genmask="); 
//    _show_sa(rt->rt_genmask, pr);
    _show_smart_sockaddr(rt->rt_genmask, pr);

	(*pr)("  rt_gwroute@%p\n", rt->rt_gwroute);
	(*pr)("  rt_llinfo@%p\n", rt->rt_llinfo);
    if( rt->rt_llinfo )
    {
	    _show_llinfo(rt->rt_llinfo, pr);
    }

    (*pr)("\n");
	return 0;
}


/*----------------------------------------------------------------------
 * Print an IPv4 socket address.
 ----------------------------------------------------------------------*/
void _show_sockaddr_in( struct sockaddr_in *sin, pr_fun *pr )
{
    int i;
    char paddr_buf[128];
    if( !sin ) 
    {
		(*pr)("[NULL]\n");
        return;
    }
    else
    {
        (*pr)("\n");
    }

//#ifdef SIN_LEN
    (*pr)("    sin_len=%d", sin->sin_len);
//#endif /* SIN_LEN */
    (*pr)("  sin_family=%d  sin_port=%d", 
        sin->sin_family, htons(sin->sin_port), htons(sin->sin_port));

    (*pr)("  sin_addr=");
    _inet_ntop( (struct sockaddr*)sin, paddr_buf, sizeof(paddr_buf));
    (*pr)("%-15s ", paddr_buf);
    (*pr)("\n");

    (*pr)("    sin_zero=");
    for( i=0; i<8; i++)
    {
//        (*pr)("0x%02x ",sin->sin_zero[i]);
        (*pr)("%02x ",sin->sin_zero[i]);
    }
    (*pr)("\n");
}


/*----------------------------------------------------------------------
 * Print an IPv6 socket address.
 ----------------------------------------------------------------------*/
#ifdef INET6
void _show_sockaddr_in6( struct sockaddr_in6 *sin6, pr_fun *pr )
{
    int i;
//    char paddr_buf[128];
    if( !sin6 ) 
    {
		(*pr)("[NULL]\n");
        return;
    }
    else
    {
        (*pr)("\n");
    }

    (*pr)("    sin6_len=%d", sin6->sin6_len);
    (*pr)("  sin6_family=%d  sin6_port=%d", 
             sin6->sin6_family, htons(sin6->sin6_port), htons(sin6->sin6_port));

    (*pr)("  sin6_addr=");
    _dump_in6_addr(&sin6->sin6_addr, pr);
    (*pr)("\n");

    (*pr)("    sin6_flowinfo=%d sin6_scope_id=%d",
            sin6->sin6_flowinfo, sin6->sin6_scope_id );
    (*pr)("\n");
}

void _dump_in6_addr(struct in6_addr * in6_addr, pr_fun *pr)
{
    u_short *shorts = (u_short *)in6_addr;
    int i = 0;

    if (!in6_addr) {
        printf("Dereference a NULL in6_addr? I don't think so.\n");
        return;
     }

    while (i < 7)
    {
        (*pr)("%4x:",htons(shorts[i++]));
    }
        
    (*pr)("%4x\n",htons(shorts[7]));
}
#endif /* INET6 */
 

/*----------------------------------------------------------------------
 * Print a generic socket address.  Use if no family-specific routine is
 * available.
 ----------------------------------------------------------------------*/
void _show_sockaddr( struct sockaddr *sa, pr_fun *pr )
{
    int i;
    if( !sa ) 
    {
		(*pr)("[NULL]\n");
        return;
    }
    else
    {
        (*pr)("\n");
    }

    (*pr)("    sa_len=%d  ", sa->sa_len);
    (*pr)("sa_family=%d  ", sa->sa_family);

    if( sa->sa_len > 2 )
    {
        (*pr)("remaining bytes are:\n    ");
        for( i = 0; i < (sa->sa_len - 2); i++ )
        {
//            (*pr)("0x%02x ",(unsigned char)sa->sa_data[i]);
            (*pr)("%02x ",(unsigned char)sa->sa_data[i]);
        }
    }
    (*pr)("\n");
}

/*----------------------------------------------------------------------
 * Print a link-layer socket address.  (Not that there are user-level link
 * layer sockets, but there are plenty of link-layer addresses in the kernel.)
 ----------------------------------------------------------------------*/
void _show_sockaddr_dl( struct sockaddr_dl *sdl, pr_fun *pr )
{
    char buf[256];
    int i;

    if( !sdl ) 
    {
		(*pr)("[NULL]\n");
        return;
    }
    else
    {
        (*pr)("\n");
    }

    (*pr)("    sdl_len=%d  sdl_family=%d  sdl_index=%d  sdl_type=%d\n",
	    sdl->sdl_len, sdl->sdl_family, sdl->sdl_index, sdl->sdl_type);


    if( (sdl->sdl_nlen == 0) && (sdl->sdl_alen == 0) && (sdl->sdl_slen == 0) )
    {
        // special case: lotsa 0's...
        (*pr)("    sdl_nlen=0  sdl_alen=0  sdl_slen=0\n");
    }
    else
    {

        buf[sdl->sdl_nlen] = 0;
        if( sdl->sdl_nlen )
        {
            bcopy(sdl->sdl_data,buf,sdl->sdl_nlen);
            (*pr)("    sdl_nlen=%d  (name='%s')\n",sdl->sdl_nlen,buf);
        }

        (*pr)("    sdl_alen=%d  ", sdl->sdl_alen);
        if (sdl->sdl_alen)
        {
            (*pr)("(addr=");
            for (i = 0; i<sdl->sdl_alen; i++)
            {            
//	            (*pr)("0x%2x ",(unsigned char)sdl->sdl_data[i+sdl->sdl_nlen]);
	            (*pr)("%02x ",(unsigned char)sdl->sdl_data[i+sdl->sdl_nlen]);
            }
            (*pr)(")");
        }
        (*pr)("\n");
        
        (*pr)("    sdl_slen=%d  ",sdl->sdl_slen);
        if (sdl->sdl_slen)
        {
            (*pr)("(addr=");
            for (i = 0; i<sdl->sdl_slen; i++)
            {
//	            (*pr)("0x%2x ", (unsigned char)sdl->sdl_data[i+sdl->sdl_nlen+sdl->sdl_alen]);
	            (*pr)("%02x", (unsigned char)sdl->sdl_data[i+sdl->sdl_nlen+sdl->sdl_alen]);
            }
        }
        (*pr)("\n");
        
    } // end if ( (sdl->sdl_nlen == 0) && (sdl->sdl_alen == 0) && (sdl->sdl_slen == 0) )
}       


/*----------------------------------------------------------------------
 * Print a socket address, calling a family-specific routine if available.
 ----------------------------------------------------------------------*/
void _show_smart_sockaddr( struct sockaddr *sa, pr_fun *pr )
{
    if( !sa ) 
    {
		(*pr)("[NULL]\n");
        return;
    }

    switch (sa->sa_family)
    {
#ifdef INET6
        case AF_INET6:
            _show_sockaddr_in6((struct sockaddr_in6 *)sa, pr);
            break;
#endif /* INET6 */

        case AF_INET:
            _show_sockaddr_in((struct sockaddr_in *)sa, pr);
            break;

        case AF_LINK:
            _show_sockaddr_dl((struct sockaddr_dl *)sa, pr);
            break;

        default:
            _show_sockaddr( sa, pr );
            break;
    }
}



int _show_arp_ip_to_mac_assoc( struct radix_node *rn, pr_fun *pr )
{
    struct sockaddr *pip_sock_addr;
    struct sockaddr *pmac_sock_addr;
    struct rtentry *prt; 
    unsigned char *pchr_mac_addr = 0;
    struct in_addr _in_addr;
    unsigned char buf[128], *pbuf;

    pbuf = buf;

    prt = (struct rtentry *)rn;

    pip_sock_addr = rt_key(prt);     
    pmac_sock_addr = prt->rt_gateway; 

    if( pmac_sock_addr == 0 )
    {
       return 0;
    }

    if( (pmac_sock_addr->sa_family != AF_LINK) 
        || (!(prt->rt_flags & RTF_HOST)) 
        || (((struct sockaddr_dl *)pmac_sock_addr)->sdl_alen == 0) )
    {
        // routing table entry is NOT an ARP entry...
	    return 0; 
    }

    pchr_mac_addr = (unsigned char *)(LLADDR((struct sockaddr_dl *)pmac_sock_addr));
    _in_addr.s_addr = (unsigned int)(((struct sockaddr_in *)pip_sock_addr)->sin_addr.s_addr);

    if( (prt->rt_flags & RTF_UP) ) *pbuf++ = 'U';
    if( (prt->rt_flags & RTF_GATEWAY) ) *pbuf++ = 'G';
    if( (prt->rt_flags & RTF_HOST) ) *pbuf++ = 'H';
    if( (prt->rt_flags & RTF_STATIC) ) *pbuf++ = 'S';
    if( (prt->rt_flags & RTF_DYNAMIC) ) *pbuf++ = 'D';
    if( (prt->rt_flags & RTF_PROTO1) ) *pbuf++ = '1';
    if( (prt->rt_flags & RTF_PROTO2) ) *pbuf++ = '2';
    if( (prt->rt_flags & RTF_PROTO3) ) *pbuf++ = '3';
    if( (prt->rt_flags & RTF_WASCLONED) ) *pbuf++ = 'C';
    if( (prt->rt_flags & RTF_LOCAL) ) *pbuf++ = 'L';
    *pbuf = '\0';

    (*pr)( "%-15s    %18s    %-10s", inet_ntoa( _in_addr ), 
        ethmac_ntoa( pchr_mac_addr ), buf );

//    if_indextoname( prt->rt_ifp->if_index, buf, 32 );
//    (*pr)( "  %-8s ", buf );
    (*pr)( "\n" ); 

	return 0;
}


void _show_routing_table_details( pr_fun *pr )
{
    int error;
	struct radix_node_head *rnh;
	rnh = rt_tables[AF_INET];
    if( rnh != NULL )
    {
	    (*pr)("Route tree for AF_INET\n");
	    if (rnh == NULL) {
	    	(*pr)(" (not initialized)\n");
	    	return;
	    }
	    error = rnh->rnh_walktree(rnh, _show_radix_node, (void *)pr);
    }
    else
    {
		(*pr)("[NULL]\n");
    }
}


void _show_summary_arp_table( pr_fun *pr )
{
    int error;
	struct radix_node_head *rnh;
	rnh = rt_tables[AF_INET];
    if( rnh != NULL )
    {
        (*pr)("IP addr             MAC addr             Flags\n"
               "----------------------------------------------\n");

	    error = rnh->rnh_walktree(rnh, _show_arp_ip_to_mac_assoc, (void *)pr);
    }
    else
    {
		(*pr)("[NULL]\n");
    }
}

// end cliffmod

static void
_mask(struct sockaddr *sa, unsigned char *buf, int _len)
{
//    char *cp = ((char *)sa) + 4;
    unsigned char *cp = ((unsigned char *)sa) + 4;
    int len = sa->sa_len - 4;
    int tot = 0;

    while (len-- > 0) {
        if (tot) *buf++ = '.';
        buf += diag_sprintf(buf, "%d", *cp++);
        tot++;
    }

    while (tot < 4) {
        if (tot) *buf++ = '.';
        buf += diag_sprintf(buf, "%d", 0);
        tot++;
    }
}




#ifdef CYGPKG_NET_INET6
static void
_mask6(struct sockaddr *sa, char *buf, int _len)
{
  struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sa;
  int addrword = 0;
  int bits = 0;
  int index;

  while (addrword < 4) {
    if (sin6->sin6_addr.s6_addr32[addrword] == 0) {
      break;
    }
    HAL_LSBIT_INDEX(index, sin6->sin6_addr.s6_addr32[addrword++]);
    bits += (32-index);
    if (index != 0) {
      break;
    }
  }
  diag_sprintf(buf, "%d", bits);
}
#endif


static void
_show_ifp(struct ifnet *ifp, pr_fun *pr)
{
    struct ifaddr *ifa;
    char addr[64], netmask[64], broadcast[64];  // cliffmod

    if( !ifp )
    {
		(*pr)("[NULL]\n");
        return;
    }
// begin cliffmod - orig code...
//    if_indextoname(ifp->if_index, name, 64);
//    (*pr)("%-8s", name);
    if( ifp->if_name )
    {
        (*pr)("IFP: %s%d\n", ifp->if_name, ifp->if_unit);
    }
// end cliffmod
    TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list) {
        if (ifa->ifa_addr->sa_family != AF_LINK) {
            (*pr)("IFP: %s%d\n", ifp->if_name, ifp->if_unit);
//            (*pr)("%-8s", name);
            getnameinfo (ifa->ifa_addr, ifa->ifa_addr->sa_len, addr, sizeof(addr), 0, 0, 0);
	    if (ifa->ifa_addr->sa_family == AF_INET) {
            getnameinfo (ifa->ifa_dstaddr, ifa->ifa_dstaddr->sa_len, broadcast, sizeof(broadcast), 0, 0, 0);
            _mask(ifa->ifa_netmask, netmask, 64);
            (*pr)("IP: %s, Broadcast: %s, Netmask: %s\n", addr, broadcast, netmask);
	    }
#ifdef CYGPKG_NET_INET6
	    if (ifa->ifa_addr->sa_family == AF_INET6) {
	      _mask6(ifa->ifa_netmask, netmask, 64);
	      (*pr)("IP: %s/%s\n", addr,netmask);
	    }
#endif
            (*pr)("        ");
            if ((ifp->if_flags & IFF_UP)) (*pr)("UP ");
            if ((ifp->if_flags & IFF_BROADCAST)) (*pr)("BROADCAST ");
            if ((ifp->if_flags & IFF_LOOPBACK)) (*pr)("LOOPBACK ");
            if ((ifp->if_flags & IFF_RUNNING)) (*pr)("RUNNING ");
            if ((ifp->if_flags & IFF_PROMISC)) (*pr)("PROMISC ");
            if ((ifp->if_flags & IFF_MULTICAST)) (*pr)("MULTICAST ");
            if ((ifp->if_flags & IFF_ALLMULTI)) (*pr)("ALLMULTI ");
            (*pr)("MTU: %d, Metric: %d\n", ifp->if_mtu, ifp->if_metric);
            (*pr)("        Rx - Packets: %d, Bytes: %d", ifa->if_data.ifi_ipackets, ifa->if_data.ifi_ibytes);
            (*pr)(", Tx - Packets: %d, Bytes: %d\n", ifa->if_data.ifi_opackets, ifa->if_data.ifi_obytes);
        }
    }
}

static int
_dumpentry(struct radix_node *rn, void *vw)
{
    struct rtentry *rt = (struct rtentry *)rn;
    struct sockaddr *dst, *gate, *netmask, *genmask;
    unsigned char addr[128], *cp;    // cliffmod
    pr_fun *pr = (pr_fun *)vw;

    if( !rn )
    {
		(*pr)("[NULL]\n");
        return 0;
    }

    dst = rt_key(rt);
    gate = rt->rt_gateway;
    netmask = rt_mask(rt);
    genmask = rt->rt_genmask;
    if ((rt->rt_flags & (RTF_UP | RTF_WASCLONED)) == RTF_UP) {
/*
        if (netmask == NULL) {
            return 0;
        }
*/
        _inet_ntop(dst, addr, sizeof(addr));
        (*pr)("%-15s ", addr);
        if (gate != NULL) {
            _inet_ntop(gate, addr, sizeof(addr));
            (*pr)("%-15s ", addr);
        } else {
            (*pr)("%-15s ", " ");
        }
        if (netmask != NULL) {
            _mask(netmask, addr, sizeof(addr));
            (*pr)("%-15s ", addr);
        } else {
            (*pr)("%-15s ", " ");
        }
        cp = addr;
        if ((rt->rt_flags & RTF_UP)) *cp++ = 'U';
        if ((rt->rt_flags & RTF_GATEWAY)) *cp++ = 'G';
        if ((rt->rt_flags & RTF_STATIC)) *cp++ = 'S';
        if ((rt->rt_flags & RTF_DYNAMIC)) *cp++ = 'D';
        *cp = '\0';
        (*pr)("%-8s ", addr);  // Flags
        if_indextoname(rt->rt_ifp->if_index, addr, 64);
        (*pr)("%-8s ", addr);
        (*pr)("\n");
    }
    return 0;
}


void
show_network_tables(pr_fun *pr)
{
    int i, error;
    struct radix_node_head *rnh;
    struct ifnet *ifp;

// scheduler lock/unlock may cause trouble for CM app.  try without...
//    cyg_scheduler_lock();
    (*pr)("\nRouting table\n");    // cliffmod - leading \n
    (*pr)("Destination     Gateway         Mask            Flags    Interface\n");
    for (i = 1; i <= AF_MAX; i++) {
        if ((rnh = rt_tables[i]) != NULL) {
            error = rnh->rnh_walktree(rnh, _dumpentry, pr);
        }
    }

    (*pr)("\nArp table\n");
    _show_summary_arp_table( pr );

    if( cyg_net_log_mask & BCM_LOG_ROUTING_TABLE_DETAILS )
    {
        (*pr)("\nRouting table details\n");
        _show_routing_table_details( pr );
    }

    (*pr)("\nInterface statistics\n");  // cliffmod - leading \n
    for (ifp = ifnet.tqh_first; ifp; ifp = ifp->if_link.tqe_next) {
        _show_ifp(ifp, pr);
    }

//    cyg_scheduler_unlock();
}


void
show_routing_table(pr_fun *pr)
{
    int i, error;
    struct radix_node_head *rnh;
    struct ifnet *ifp;

// scheduler lock/unlock may cause trouble for CM app.  try without...
//    cyg_scheduler_lock();

    (*pr)("\nRouting table\n");    // cliffmod - leading \n
    (*pr)("Destination     Gateway         Mask            Flags    Interface\n");
    for (i = 1; i <= AF_MAX; i++) {
        if ((rnh = rt_tables[i]) != NULL) {
            error = rnh->rnh_walktree(rnh, _dumpentry, pr);
        }
    }

    if( cyg_net_log_mask & BCM_LOG_ROUTING_TABLE_DETAILS )
    {
        (*pr)("\nRouting table details\n");
        _show_routing_table_details( pr );
    }

//    cyg_scheduler_unlock();
}


void
show_arp_table(pr_fun *pr)
{
    int i, error;
    struct radix_node_head *rnh;
    struct ifnet *ifp;

// scheduler lock/unlock may cause trouble for CM app.  try without...
//    cyg_scheduler_lock();

    (*pr)("\nArp table\n");
    _show_summary_arp_table( pr );

    if( cyg_net_log_mask & BCM_LOG_ROUTING_TABLE_DETAILS )
    {
        (*pr)("\nRouting table details\n");
        _show_routing_table_details( pr );
    }

//    cyg_scheduler_unlock();
}


void _show_mbuf( struct mbuf *pmbuf, pr_fun *pr )
{
    struct mbuf *ploc_mbuf;
    int mbufs_printed = 0;

    if( !pmbuf || !pr )
    {
        return;
    }

    for( ploc_mbuf = pmbuf; ploc_mbuf != NULL; ploc_mbuf = ploc_mbuf->m_next )
    {
        // print next mbuf in chain.
        _show_single_mbuf( ploc_mbuf, pr );

        mbufs_printed++;
        if( mbufs_printed > 10 )
        {
            // printed 10 mbufs...seems excessive.  print message and break
            // out of loop.
            (*pr)( "_show_mbuf() - printed limit of 10 mbuf's in this chain.  break out of loop.\n" );
            break;
        }
    }
}

void _show_single_mbuf( struct mbuf *pmbuf, pr_fun *pr )
{
    int i;

    if( !pmbuf || !pr )
    {
        return;
    }
  
    _show_mbuf_hdr( pmbuf, pr );

    (*pr)( "m_data:\n" );

    for( i = 0; i < pmbuf->m_len; i++ )
    {
        // print each data byte as (2) hex digits followed by a space.
        (*pr) ( "%02x%s", (unsigned char)pmbuf->m_data[ i ], " " );
        if( !((i+1) % 16) )
        {
            // go to new line after 16 bytes...
            (*pr)( "\n" );
        }
        else if( !((i+1) % 4) )
        {
            // insert an extra space after every 4th byte as visual que.
            (*pr)( "  " );
        }
    }

    // printed all data bytes.  if we didn't just print a "\n", then print one now.
    if( i % 16 )
    {
        // didn't just print a "\n" (a.k.a. go to the next line).
        // --> do it now.
        (*pr)( "\n" );
    }
}


void _show_mbuf_hdr( struct mbuf *pmbuf, pr_fun *pr )
{
    char *type;

    if( !pmbuf || !pr )
    {
        return;
    }

    (*pr)(" MBUF   : TYPE    FLGS DATA[LEN]     NEXT     NEXTPKT\n");

    switch( pmbuf->m_hdr.mh_type )
    {
        case MT_FREE: type=   "FREE   "; break;
        case MT_DATA: type=   "DATA   "; break;
        case MT_HEADER: type= "HEADER "; break;
        case MT_SONAME: type= "SONAME "; break;
        case MT_FTABLE: type= "FTABLE "; break;
        case MT_CONTROL: type="CONTROL"; break;
        case MT_OOBDATA: type="OOBDATA"; break;
        default: type=        "UNKNOWN"; break;
    }

    (*pr)("%08x: %s %04x %08x[%03d] %08x %08x\n",
        pmbuf, type, pmbuf->m_hdr.mh_flags, pmbuf->m_hdr.mh_data,
        pmbuf->m_hdr.mh_len, pmbuf->m_hdr.mh_next, pmbuf->m_hdr.mh_nextpkt);
}




#endif // CYGPKG_NET_DRIVER_FRAMEWORK

// EOF support.c
