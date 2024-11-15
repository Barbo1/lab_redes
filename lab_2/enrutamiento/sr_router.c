/**********************************************************************
 * file:  sr_router.c
 *
 * Descripción:
 *
 * Este archivo contiene todas las funciones que interactúan directamente
 * con la tabla de enrutamiento, así como el método de entrada principal
 * para el enrutamiento.
 *
 **********************************************************************/

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_arpcache.h"
#include "sr_utils.h"

/*---------------------------------------------------------------------
 * Method: sr_init(void)
 * Scope:  Global
 *
 * Inicializa el subsistema de enrutamiento
 *
 *---------------------------------------------------------------------*/

void sr_init(struct sr_instance* sr)
{
    assert(sr);

    /* Inicializa la caché y el hilo de limpieza de la caché */
    sr_arpcache_init(&(sr->cache));

    /* Inicializa los atributos del hilo */
    pthread_attr_init(&(sr->attr));
    pthread_attr_setdetachstate(&(sr->attr), PTHREAD_CREATE_JOINABLE);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_attr_setscope(&(sr->attr), PTHREAD_SCOPE_SYSTEM);
    pthread_t thread;

    /* Hilo para gestionar el timeout del caché ARP */
    pthread_create(&thread, &(sr->attr), sr_arpcache_timeout, sr);

} /* -- sr_init -- */


struct sr_rt * find_st_entry (struct sr_instance *sr, uint32_t ipDst) {
  struct sr_rt * first = sr->routing_table;
  struct sr_rt * better = 0;
  uint32_t better_matched_bits = 0;

  while (first) {
    uint32_t masked_ip = ipDst & first->mask.s_addr;
    uint32_t matched_bits = ~(masked_ip ^ first->dest.s_addr);
    if (0 < matched_bits && matched_bits > better_matched_bits) {
      better = first;
      better_matched_bits = matched_bits;
    }

    first = first->next;
  }

  return better;
}

/* Envía un paquete ICMP de error */
void sr_send_icmp_error_packet (
    uint8_t type, uint8_t code, 
    struct sr_instance *sr, 
    uint32_t ipDst, 
    uint8_t *ipPacket) 
{

  printf("### -> Sending a ICMP error packet.\n");

  /* Creacion del paquete. 
   * */
  int packet_size = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
  uint8_t * packet = (uint8_t *)malloc(packet_size);

  /* Busqueda en la routing table. */
  struct sr_rt * matched_rt = find_st_entry (sr, ipDst);
  if (!matched_rt) {
    printf("#### -> The IP to send was not found.\n");
  }
  struct sr_if * mine_interface = sr_get_interface (sr, matched_rt->interface);
  
  /* Headers de ethernet. 
   * */
  sr_ethernet_hdr_t * packet_ether = (sr_ethernet_hdr_t *)(packet);
  packet_ether->ether_type = htons(ethertype_ip);

  /* Headers de ip. 
   * */
  sr_ip_hdr_t * packet_ip = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
  memcpy ((uint8_t *)packet_ip, ipPacket, sizeof(sr_ip_hdr_t));
  packet_ip->ip_ttl = 32;
  packet_ip->ip_src = mine_interface->ip; 
  packet_ip->ip_dst = ipDst;
  packet_ip->ip_sum = ip_cksum ((sr_ip_hdr_t *)packet_ip, sizeof(sr_ip_hdr_t));

  /* Headers de icmp. 
   * */
  sr_icmp_t3_hdr_t * packet_icmp = (sr_icmp_t3_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
  packet_icmp->icmp_code = code;
  packet_icmp->icmp_type = type;
  packet_icmp->icmp_sum = 0;
  packet_icmp->next_mtu = 0;
  packet_icmp->unused = 0;
  memcpy (packet_icmp->data, ipPacket, sizeof (sr_ip_hdr_t) + 8);
  packet_icmp->icmp_sum = icmp3_cksum (packet_icmp, sizeof (sr_icmp_t3_hdr_t));
  
  /* envio del paquete.
   * */
  struct sr_arpentry * entrada_cache = sr_arpcache_lookup (&(sr->cache), matched_rt->gw.s_addr);

  /* Se conoce la MAC. 
   * */
  if (entrada_cache) {
    printf("#### -> Found MAC in the cache\n");

    memcpy (packet_ether->ether_shost, mine_interface->addr, ETHER_ADDR_LEN);
    memcpy (packet_ether->ether_dhost, (uint8_t *)entrada_cache->mac, ETHER_ADDR_LEN);

    /* envio del paquete.
     * */
    sr_send_packet (
      sr, 
      packet, 
      packet_size, 
      mine_interface->name
    );

    printf("#### -> Packet Sent.\n");
    
    printf("### -> Packet Info:\n");
    print_hdr_eth(packet);
    print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
    print_hdr_icmp(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    printf("--------------------------------------\n");

    free(entrada_cache);

  /* NO se conoce la MAC. 
   * */
  } else {
    printf("#### -> MAC not found\n");

    struct sr_arpreq * req = sr_arpcache_queuereq (
      &(sr->cache), 
      matched_rt->gw.s_addr, 
      packet, 
      packet_size, 
      mine_interface->name
    );

    printf("### -> Packet Info:\n");
    print_hdr_eth(packet);
    print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
    print_hdr_icmp(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));

    handle_arpreq (sr, req);
  }

  free(packet);
} /* -- sr_send_icmp_error_packet -- */

void sr_send_icmp_echo_message (uint8_t type, uint8_t code, struct sr_instance *sr, uint32_t ipDst, uint8_t *ipPacket) {
  printf("### -> Sending a ICMP echo message\n");

  /* Creacion del paquete. 
   * */
  int packet_size = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(sr_icmp_t3_hdr_t);
  uint8_t * packet = (uint8_t *)malloc(packet_size);

  /* Busqueda en la routing table. */
  struct sr_rt * matched_rt = find_st_entry (sr, ipDst);
  if (!matched_rt) {
    printf("#### -> The IP to send was not found.\n");
  }
  struct sr_if * mine_interface = sr_get_interface (sr, matched_rt->interface);
  
  /* Headers de ethernet. 
   * */
  sr_ethernet_hdr_t * packet_ether = (sr_ethernet_hdr_t *)(packet);
  packet_ether->ether_type = htons(ethertype_ip);

  /* Headers de ip. 
   * */
  sr_ip_hdr_t * packet_ip = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
  memcpy ((uint8_t *)packet_ip, ipPacket, sizeof(sr_ip_hdr_t));
  packet_ip->ip_ttl = 32;
  packet_ip->ip_src = mine_interface->ip; 
  packet_ip->ip_dst = ipDst;
  packet_ip->ip_sum = ip_cksum ((sr_ip_hdr_t *)packet_ip, sizeof(sr_ip_hdr_t));

  /* Headers de icmp. */
  sr_icmp_hdr_t * packet_icmp = (sr_icmp_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
  packet_icmp->icmp_code = code;
  packet_icmp->icmp_type = type;
  packet_icmp->icmp_sum = icmp_cksum (packet_icmp, sizeof (sr_icmp_hdr_t));
  
  /* envio del paquete.
   * */
  struct sr_arpentry * entrada_cache = sr_arpcache_lookup (&(sr->cache), matched_rt->gw.s_addr);

  /* Se conoce la MAC. 
   * */
  if (entrada_cache) {
    printf("#### -> Found MAC in the cache\n");

    memcpy (packet_ether->ether_dhost, entrada_cache->mac, ETHER_ADDR_LEN);
    memcpy (packet_ether->ether_shost, (uint8_t *)mine_interface->addr, ETHER_ADDR_LEN);

    /* envio del paquete.
     * */
    sr_send_packet (
      sr, 
      packet, 
      packet_size, 
      mine_interface->name
    );

    printf("#### -> Packet Sent.\n");
    
    printf("### -> Packet Info:\n");
    print_hdr_eth(packet);
    print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
    print_hdr_icmp(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
    printf("--------------------------------------\n");

    free(entrada_cache);

  /* NO se conoce la MAC. 
   * */
  } else {
    printf("#### -> MAC not found\n");

    struct sr_arpreq * req = sr_arpcache_queuereq (
      &(sr->cache), 
      matched_rt->gw.s_addr, 
      packet, 
      packet_size, 
      mine_interface->name
    );

    print_hdr_eth(packet);
    print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
    print_hdr_icmp(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));

    handle_arpreq (sr, req);
  }

  free(packet);
}

void sr_handle_ip_packet(struct sr_instance *sr,
        uint8_t *packet /* lent */,
        unsigned int len,
        uint8_t *srcAddr,
        uint8_t *destAddr,
        char *interface /* lent */,
        sr_ethernet_hdr_t *eHdr) {

  printf("### -> Handling a IP packet\n");

  sr_ip_hdr_t * ip_headers = (sr_ip_hdr_t *) (packet + sizeof (sr_ethernet_hdr_t));
  uint32_t ip_to_packet = ip_headers->ip_dst;
  
  printf("the IP to evaluate is: \n");
  print_hdr_eth(packet);
  print_hdr_ip(packet + sizeof (sr_ethernet_hdr_t));

  /* El TLL no es el adecuado. */
  uint8_t ttl = ip_headers->ip_ttl - 1;
  if (ttl <= 0) {
    printf("#### -> Insuficient TTL. Sending a ICMP error: 'time to live exceeded'.\n");

    sr_send_icmp_error_packet (
      11, 
      0, 
      sr, 
      ip_headers->ip_src, 
      packet + sizeof (sr_ethernet_hdr_t)
    );
    return;
  }

  /* el paquete que llego es ospf. */
  if (ip_headers->ip_p == ip_protocol_ospfv2) {
    printf("###### -> Its a ospf packet.\n");

    sr_handle_pwospf_packet(
      sr, 
      packet, 
      len, 
      struct sr_if* rx_if /* que es esto? */
    );

    return;
  }

  struct sr_if * mine_interface = sr_get_interface_given_ip(sr, ip_to_packet);

  /* El paquete es para una de mis interfaces. */
  if (mine_interface != 0) {
    printf("#### -> Its a packet for one of my interfaces.\n");

    /* el paquete que llego es icmp. */
    if (ip_headers->ip_p == ip_protocol_icmp) {
      printf("##### -> Its a ICMP packet.\n");

      uint8_t type = ((uint8_t *) (packet + sizeof (sr_ethernet_hdr_t) + sizeof (sr_ip_hdr_t)))[0];
      if (type == 8) {
        printf("###### -> Its an echo reply.\n");

        sr_send_icmp_echo_message (
          0, 
          0, 
          sr, 
          ip_headers->ip_src, 
          (uint8_t *)ip_headers
        );
      } else {
        printf("###### -> Its NOT an echo reply.\n");
      }

    } else {
      printf("##### -> Its NOT a recognized packet.\n");
    }
    return; /* discard packet PREGUNTAR */
  }
  
  printf("#### -> Niether of my interfaces match.\n");

  struct sr_rt * matched_rt = find_st_entry (sr, ip_to_packet);

  /* No pertenece a la routing table. */
  if (!matched_rt) {
    printf("#### -> Not found a match in the forwarding table. Sending a ICMP error: 'host unreachable'.\n");

    sr_send_icmp_error_packet (
      3, 
      0, 
      sr, 
      ip_headers->ip_src, 
      packet + sizeof (sr_ethernet_hdr_t)
    );
  }
    
  printf("#### -> Found a match in the forwarding table.\n");
  sr_print_routing_entry(matched_rt);
  
  uint32_t next_hop_ip = matched_rt->gw.s_addr;
  printf("#### -> Constructing the packet.\n");

  /* Creacion del paquete para el envio. */
  uint8_t * new_packet = (uint8_t *)malloc(sizeof(uint8_t) * len);
  sr_ethernet_hdr_t * new_packet_header_part_ether = (sr_ethernet_hdr_t *) new_packet;
  sr_ip_hdr_t * new_packet_header_part_ip = (sr_ip_hdr_t *) (new_packet + sizeof(sr_ethernet_hdr_t));
  memcpy(new_packet, packet, sizeof(uint8_t) * len);

  new_packet_header_part_ether->ether_type = eHdr->ether_type;
  new_packet_header_part_ip->ip_ttl = ttl;
  new_packet_header_part_ip->ip_sum = ip_cksum (new_packet_header_part_ip, sizeof (sr_ip_hdr_t));
  
  print_hdr_eth(new_packet);
  print_hdr_ip(new_packet + sizeof(sr_ethernet_hdr_t));

  struct sr_arpentry * entrada_cache = sr_arpcache_lookup(&(sr->cache), next_hop_ip);
  struct sr_if * out_interface = sr_get_interface (sr, matched_rt->interface);

  /* Se conoce la MAC. */
  if (entrada_cache) {
    printf("#### -> Found MAC in the cache.\n");

    memcpy(new_packet_header_part_ether->ether_dhost, entrada_cache->mac, ETHER_ADDR_LEN);
    memcpy(new_packet_header_part_ether->ether_shost, out_interface->addr, ETHER_ADDR_LEN);

    /* envio del paquete.
     * */
    sr_send_packet (
      sr, 
      new_packet, 
      len, 
      out_interface->name
    );

    free(entrada_cache);

  /* NO se conoce la MAC. */
  } else {
    printf("#### -> MAC not found.\n");

    struct sr_arpreq * req = sr_arpcache_queuereq(
      &(sr->cache), 
      next_hop_ip, 
      new_packet, 
      len,
      out_interface->name
    );

    handle_arpreq (sr, req);
  }

  free(new_packet);
  return;
}

/* 
* ***** A partir de aquí no debería tener que modificar nada ****
*/

/* Envía todos los paquetes IP pendientes de una solicitud ARP */
void sr_arp_reply_send_pending_packets(struct sr_instance *sr, struct sr_arpreq *arpReq, uint8_t *dhost, uint8_t *shost, struct sr_if *iface) {

  struct sr_packet *currPacket = arpReq->packets;
  sr_ethernet_hdr_t *ethHdr;
  uint8_t *copyPacket;

  while (currPacket != NULL) {
     ethHdr = (sr_ethernet_hdr_t *) currPacket->buf;
     memcpy(ethHdr->ether_shost, dhost, sizeof(uint8_t) * ETHER_ADDR_LEN);
     memcpy(ethHdr->ether_dhost, shost, sizeof(uint8_t) * ETHER_ADDR_LEN);

     copyPacket = malloc(sizeof(uint8_t) * currPacket->len);
     memcpy(copyPacket, ethHdr, sizeof(uint8_t) * currPacket->len);

     print_hdrs(copyPacket, currPacket->len);
     sr_send_packet(sr, copyPacket, currPacket->len, iface->name);
     currPacket = currPacket->next;
  }
}

/* Gestiona la llegada de un paquete ARP*/
void sr_handle_arp_packet(struct sr_instance *sr,
        uint8_t *packet /* lent */,
        unsigned int len,
        uint8_t *srcAddr,
        uint8_t *destAddr,
        char *interface /* lent */,
        sr_ethernet_hdr_t *eHdr) {

  /* Imprimo el cabezal ARP */
  printf("*** -> It is an ARP packet. Print ARP header.\n");
  print_hdr_arp(packet + sizeof(sr_ethernet_hdr_t));

  /* Obtengo el cabezal ARP */
  sr_arp_hdr_t *arpHdr = (sr_arp_hdr_t *) (packet + sizeof(sr_ethernet_hdr_t));

  /* Obtengo las direcciones MAC */
  unsigned char senderHardAddr[ETHER_ADDR_LEN], targetHardAddr[ETHER_ADDR_LEN];
  memcpy(senderHardAddr, arpHdr->ar_sha, ETHER_ADDR_LEN);
  memcpy(targetHardAddr, arpHdr->ar_tha, ETHER_ADDR_LEN);

  /* Obtengo las direcciones IP */
  uint32_t senderIP = arpHdr->ar_sip;
  uint32_t targetIP = arpHdr->ar_tip;
  unsigned short op = ntohs(arpHdr->ar_op);

  /* Verifico si el paquete ARP es para una de mis interfaces */
  struct sr_if *myInterface = sr_get_interface_given_ip(sr, targetIP);

  if (op == arp_op_request) {  /* Si es un request ARP */
    printf("**** -> It is an ARP request.\n");

    /* Si el ARP request es para una de mis interfaces */
    if (myInterface != 0) {
      printf("***** -> ARP request is for one of my interfaces.\n");

      /* Agrego el mapeo MAC->IP del sender a mi caché ARP */
      printf("****** -> Add MAC->IP mapping of sender to my ARP cache.\n");
      sr_arpcache_insert(&(sr->cache), senderHardAddr, senderIP);

      /* Construyo un ARP reply y lo envío de vuelta */
      printf("****** -> Construct an ARP reply and send it back.\n");
      memcpy(eHdr->ether_shost, (uint8_t *) myInterface->addr, sizeof(uint8_t) * ETHER_ADDR_LEN);
      memcpy(eHdr->ether_dhost, (uint8_t *) senderHardAddr, sizeof(uint8_t) * ETHER_ADDR_LEN);
      memcpy(arpHdr->ar_sha, myInterface->addr, ETHER_ADDR_LEN);
      memcpy(arpHdr->ar_tha, senderHardAddr, ETHER_ADDR_LEN);
      arpHdr->ar_sip = targetIP;
      arpHdr->ar_tip = senderIP;
      arpHdr->ar_op = htons(arp_op_reply);

      /* Imprimo el cabezal del ARP reply creado */
      print_hdrs(packet, len);

      sr_send_packet(sr, packet, len, myInterface->name);
    }

    printf("******* -> ARP request processing complete.\n");

  } else if (op == arp_op_reply) {  /* Si es un reply ARP */

    printf("**** -> It is an ARP reply.\n");

    /* Agrego el mapeo MAC->IP del sender a mi caché ARP */
    printf("***** -> Add MAC->IP mapping of sender to my ARP cache.\n");
    struct sr_arpreq *arpReq = sr_arpcache_insert(&(sr->cache), senderHardAddr, senderIP);
    
    if (arpReq != NULL) { /* Si hay paquetes pendientes */

    	printf("****** -> Send outstanding packets.\n");
    	sr_arp_reply_send_pending_packets(sr, arpReq, (uint8_t *) myInterface->addr, (uint8_t *) senderHardAddr, myInterface);
    	sr_arpreq_destroy(&(sr->cache), arpReq);

    }
    printf("******* -> ARP reply processing complete.\n");
  }
}

/*---------------------------------------------------------------------
 * Method: sr_handlepacket(uint8_t* p,char* interface)
 * Scope:  Global
 *
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Both the packet buffer and the character's memory are handled
 * by sr_vns_comm.c that means do NOT delete either.  Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 *
 *---------------------------------------------------------------------*/

void sr_handlepacket(struct sr_instance* sr,
        uint8_t * packet/* lent */,
        unsigned int len,
        char* interface/* lent */)
{
  assert(sr);
  assert(packet);
  assert(interface);

  printf("*** -> Received packet of length %d \n",len);

  /* Obtengo direcciones MAC origen y destino */
  sr_ethernet_hdr_t *eHdr = (sr_ethernet_hdr_t *) packet;
  uint8_t *destAddr = malloc(sizeof(uint8_t) * ETHER_ADDR_LEN);
  uint8_t *srcAddr = malloc(sizeof(uint8_t) * ETHER_ADDR_LEN);
  memcpy(destAddr, eHdr->ether_dhost, sizeof(uint8_t) * ETHER_ADDR_LEN);
  memcpy(srcAddr, eHdr->ether_shost, sizeof(uint8_t) * ETHER_ADDR_LEN);
  uint16_t pktType = ntohs(eHdr->ether_type);

  if (is_packet_valid(packet, len)) {
    if (pktType == ethertype_arp) {
      sr_handle_arp_packet(sr, packet, len, srcAddr, destAddr, interface, eHdr);
    } else if (pktType == ethertype_ip) {
      sr_handle_ip_packet(sr, packet, len, srcAddr, destAddr, interface, eHdr);
    }
  }

}/* end sr_ForwardPacket */
