
//****************************************************************************
//
// Copyright (c) 2003-2010 Broadcom Corporation
//
// This file being released as part of eCos, the Embedded Configurable Operating System.
//
// eCos is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License version 2, available at 
// http://www.broadcom.com/licenses/GPLv2.php (the "GPL").
//
// As a special exception, if you compile this file and link it
// with other works to produce a work based on this file, this file does not
// by itself cause the resulting work to be covered by the GNU General Public
// License. However the source code for this file must still be made available
// in accordance with section (3) of the GNU General Public License.
//
//****************************************************************************
//
//  Description:
//      Declarations for Broadcom network interface.
//
//****************************************************************************

//********************** Include Files ***************************************

#ifndef __IF_BCM_H__
#define __IF_BCM_H__

#include <stdio.h>

    
#ifdef __cplusplus
extern "C" {
#endif

#if 1 //0
#define stack_debug printf
#else
static void DummyPrint(const char *pString, ...) {}

#define stack_debug DummyPrint
#endif

struct ip_moptions;
struct route;
struct mbuf;
struct ifnet;
struct ifaddr;
struct sockaddr;
struct inpcb;
struct sockaddr_in;
struct ip;

#define DMP_DEBUG_IP_OUTPUT             0x0001
#define DMP_DEBUG_IP_OUTPUT_DIRECT      0x0002
#define DMP_DEBUG_RIP_OUTPUT            0x0004

#define DMP_STACK_DEBUG 0 //(DMP_DEBUG_IP_OUTPUT | DMP_DEBUG_IP_OUTPUT_DIRECT | DMP_DEBUG_RIP_OUTPUT)

#define BCM_MAX_NET_INTERFACES 8 
#define BCM_MIN_NETIF_NUM 1
#define BCM_MAX_NETIF_NUM BCM_MAX_NET_INTERFACES

#define BCM_NETIF_SOCKOPT_BITS  0xE800  //      1 1 1 x  1 x x x  x x x x  x x x x
#define BCM_NETIF_ANY           0x0000  //      0 0 0 x  0 x x x  x x x x  x x x x
#define BCM_NETIF_1             0x0800  //      0 0 0 x  1 x x x  x x x x  x x x x
#define BCM_NETIF_2             0x2000  //      0 0 1 x  0 x x x  x x x x  x x x x
#define BCM_NETIF_3             0x4000  //      0 1 0 x  0 x x x  x x x x  x x x x
#define BCM_NETIF_4             0x6000  //      0 1 1 x  0 x x x  x x x x  x x x x
#define BCM_NETIF_5             0x8000  //      1 0 0 x  0 x x x  x x x x  x x x x
#define BCM_NETIF_6             0xA000  //      1 0 1 x  0 x x x  x x x x  x x x x
#define BCM_NETIF_7             0xC000  //      1 1 0 x  0 x x x  x x x x  x x x x
#define BCM_NETIF_8             0xE000  //      1 1 1 x  0 x x x  x x x x  x x x x



struct pifnet_result
{
    // function specific result code.
    int result_code;

    // if function is successful, pointer to network interface result.
    struct ifnet* pifnet;

    // if function is successful, pointer to network interface address result.
    struct ifaddr* pifaddr;        
};

int BcmTestAndComputeLocalNetIf( register struct inpcb *pinpcb, 
    struct ifnet* pifnet, unsigned long* pif_ipaddr );


unsigned int BcmTestAndApplyUdpOutputRouting( register struct inpcb *pinpcb, 
    struct mbuf *pmbuf, int *perror );
    

unsigned int BcmTestAndApplyRawIpOutputRouting( register struct inpcb *pinpcb, 
    struct mbuf *pmbuf, int *perror );
                                 

int BcmAltIpOutputRouting( struct mbuf *m0, struct mbuf *opt, struct route *ro, 
    int flags, struct ip_moptions *imo, unsigned long src_addr, 
    struct ifnet *iifp );



#define RTMODS_NO_CHANGE 0
#define RTMODS_FAILED    1 
#define RTMODS_DIFF_NET_IF 2
struct pifnet_result BcmDefIpOutputRoutingMods( struct ip* pip_packet, struct route* proute );


int BcmComputeDefaultGatewayIpAndMacAddr( register struct ifnet *pifnet,
	struct mbuf *ppacket_buf, struct sockaddr_in *pgateway_ip_addr, 
    char *pgateway_mac_addr );


unsigned short BcmNetIfNameToSockOpt( const char* net_if_name );


unsigned short BcmNetIfNumToSockOpt( unsigned int net_if_num );


unsigned int BcmNetIfNameToNetIfNum( const char* net_if_name );


unsigned int BcmNetIfNumIsValid( unsigned int net_if_num );


unsigned int BcmNetIfSpecified( unsigned short socket_options );


struct ifnet* BcmNetIfNumToNetIfPtr( unsigned int net_if_num );


void BcmSetNetIfPtr( unsigned int net_if_num, struct ifnet* pifnet );


void BcmSetNetIfIpAddr( unsigned int net_if_num, unsigned long net_if_ip_addr );


void BcmSetNetIfPtrAndIpAddr( unsigned int net_if_num, unsigned long net_if_ip_addr );


char* BcmNetIfNumToName( unsigned int net_if_num );


unsigned int BcmSockOptToNetIfNum( unsigned short socket_options );


unsigned long BcmNetIfGetFirstIpAddrViaIoctl( char* pnet_if_name );


#define MAX_DEFAULT_GATEWAYS 8

void BcmSetDefaultIpGateway(unsigned int stackIndex, unsigned long routerAddress);


unsigned long BcmGetDefaultIpGateway(unsigned int stackIndex);


void BcmSetDefaultIpGatewayMacAddr( unsigned int stackIndex, const char* gateway_mac_addr );


int BcmGetDefaultIpGatewayMacAddr( unsigned int stackIndex, char* gateway_mac_addr );


void BcmDeleteDefaultIpGatewayMacAddr( unsigned int stackIndex );


struct ifaddr* BcmIsIpAddrOnThisLocalNetIf( unsigned long target_ip_u32, 
    struct ifnet *pifnet );


struct pifnet_result BcmIsIpAddrOnAnyLocalNetIf( unsigned long target_ip_u32 ); 
    
    
void BcmShowDefaultIpGateway(void);


void BcmShowNetworkTables(void);


void BcmShowRoutingTable(void);


void BcmShowArpTable(void);


void BcmShowNetworkMemStats(void);


void BcmShowMbuf( struct mbuf *pmbuf );


unsigned int BcmFindArpEntry( unsigned int targetIpAddress, char *targetMacAddr );


int BcmAddArpIpToMacAssoc( unsigned long ipAddress, unsigned char* macAddress );


int BcmDeleteArpIpToMacAssoc( unsigned long ipAddress, unsigned char* macAddress );


char* ethmac_ntoa( char* ethmac_addr );


unsigned int BcmNetLogMaskGet();
unsigned int BcmNetLogMaskSet( unsigned int newValue );
unsigned int BcmNetLogMaskSetBits( unsigned int bitMask );
unsigned int BcmNetLogMaskClearBits( unsigned int bitMask );
#define BCM_LOG_MBUF_SORCV              0x00010000  // soreceive() 
#define BCM_LOG_MBUF_TCP_INPUT          0x00020000  // tcp_input()
#define BCM_LOG_MBUF_IP_INPUT           0x00080000  // ip_input()
#define BCM_LOG_ROUTING_TABLE_DETAILS   0x00100000


#ifdef __cplusplus
}
#endif


#endif /* __IF_BCM_H__ */


