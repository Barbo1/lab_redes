/*-----------------------------------------------------------------------------
 * file: sr_pwospf.c
 *
 * Descripción:
 * Este archivo contiene las funciones necesarias para el manejo de los paquetes
 * OSPF.
 *
 *---------------------------------------------------------------------------*/

#include "sr_pwospf.h"
#include "sr_router.h"

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>

#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "sr_utils.h"
#include "sr_protocol.h"
#include "pwospf_protocol.h"
#include "sr_rt.h"
#include "pwospf_neighbors.h"
#include "pwospf_topology.h"
#include "dijkstra.h"

/*pthread_t hello_thread;*/
pthread_t g_hello_packet_thread;
pthread_t g_all_lsu_thread;
pthread_t g_lsu_thread;
pthread_t g_neighbors_thread;
pthread_t g_topology_entries_thread;
pthread_t g_rx_lsu_thread;
pthread_t g_dijkstra_thread;

pthread_mutex_t g_dijkstra_mutex = PTHREAD_MUTEX_INITIALIZER;

struct in_addr g_router_id;
uint8_t g_ospf_multicast_mac[ETHER_ADDR_LEN];
struct ospfv2_neighbor* g_neighbors;
struct pwospf_topology_entry* g_topology;
uint16_t g_sequence_num;

/* -- Declaración de hilo principal de la función del subsistema pwospf --- */
static void* pwospf_run_thread(void* arg);

/*---------------------------------------------------------------------
 * Method: pwospf_init(..)
 *
 * Configura las estructuras de datos internas para el subsistema pwospf
 * y crea un nuevo hilo para el subsistema pwospf.
 *
 * Se puede asumir que las interfaces han sido creadas e inicializadas
 * en este punto.
 *---------------------------------------------------------------------*/

int pwospf_init(struct sr_instance* sr)
{
    assert(sr);

    sr->ospf_subsys = (struct pwospf_subsys*)malloc(sizeof(struct
                                                      pwospf_subsys));

    assert(sr->ospf_subsys);
    pthread_mutex_init(&(sr->ospf_subsys->lock), 0);

    g_router_id.s_addr = 0;

    /* Defino la MAC de multicast a usar para los paquetes HELLO */
    g_ospf_multicast_mac[0] = 0x01;
    g_ospf_multicast_mac[1] = 0x00;
    g_ospf_multicast_mac[2] = 0x5e;
    g_ospf_multicast_mac[3] = 0x00;
    g_ospf_multicast_mac[4] = 0x00;
    g_ospf_multicast_mac[5] = 0x05;

    g_neighbors = NULL;

    g_sequence_num = 0;

    struct in_addr zero;
    zero.s_addr = 0;
    g_neighbors = create_ospfv2_neighbor(zero);
    g_topology = create_ospfv2_topology_entry(zero, zero, zero, zero, zero, 0);

    /* -- start thread subsystem -- */
    if( pthread_create(&sr->ospf_subsys->thread, 0, pwospf_run_thread, sr)) { 
        perror("pthread_create");
        assert(0);
    }

    return 0; /* success */
} /* -- pwospf_init -- */


/*---------------------------------------------------------------------
 * Method: pwospf_lock
 *
 * Lock mutex associated with pwospf_subsys
 *
 *---------------------------------------------------------------------*/

void pwospf_lock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_lock(&subsys->lock) )
    { assert(0); }
}

/*---------------------------------------------------------------------
 * Method: pwospf_unlock
 *
 * Unlock mutex associated with pwospf subsystem
 *
 *---------------------------------------------------------------------*/

void pwospf_unlock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_unlock(&subsys->lock) )
    { assert(0); }
} 

/*---------------------------------------------------------------------
 * Method: pwospf_run_thread
 *
 * Hilo principal del subsistema pwospf.
 *
 *---------------------------------------------------------------------*/

static
void* pwospf_run_thread(void* arg)
{
    sleep(5);

    struct sr_instance* sr = (struct sr_instance*)arg;

    /* Set the ID of the router */
    while(g_router_id.s_addr == 0)
    {
        struct sr_if* int_temp = sr->if_list;
        while(int_temp != NULL)
        {
            if (int_temp->ip > g_router_id.s_addr)
            {
                g_router_id.s_addr = int_temp->ip;
            }

            int_temp = int_temp->next;
        }
    }
    Debug("\n\nPWOSPF: Selecting the highest IP address on a router as the router ID\n");
    Debug("-> PWOSPF: The router ID is [%s]\n", inet_ntoa(g_router_id));


    Debug("\nPWOSPF: Detecting the router interfaces and adding their networks to the routing table\n");
    struct sr_if* int_temp = sr->if_list;
    while(int_temp != NULL)
    {
        struct in_addr ip;
        ip.s_addr = int_temp->ip;
        struct in_addr gw;
        gw.s_addr = 0x00000000;
        struct in_addr mask;
        mask.s_addr =  int_temp->mask;
        struct in_addr network;
        network.s_addr = ip.s_addr & mask.s_addr;

        if (check_route(sr, network) == 0)
        {
            Debug("-> PWOSPF: Adding the directly connected network [%s, ", inet_ntoa(network));
            Debug("%s] to the routing table\n", inet_ntoa(mask));
            sr_add_rt_entry(sr, network, gw, mask, int_temp->name, 1);
        }
        int_temp = int_temp->next;
    }
    
    Debug("\n-> PWOSPF: Printing the forwarding table\n");
    sr_print_routing_table(sr);


    pthread_create(&g_hello_packet_thread, NULL, send_hellos, sr);
    pthread_create(&g_all_lsu_thread, NULL, send_all_lsu, sr);
    pthread_create(&g_neighbors_thread, NULL, check_neighbors_life, sr);
    pthread_create(&g_topology_entries_thread, NULL, check_topology_entries_age, sr);

    return NULL;
} /* -- run_ospf_thread -- */

/***********************************************************************************
 * Métodos para el manejo de los paquetes HELLO y LSU
 * SU CÓDIGO DEBERÍA IR AQUÍ
 * *********************************************************************************/

/*---------------------------------------------------------------------
 * Method: check_neighbors_life
 *
 * Chequea si los vecinos están vivos
 *
 *---------------------------------------------------------------------*/

void* check_neighbors_life(void* arg)
{
    struct sr_instance* sr = (struct sr_instance*)arg;

    while(1) {
      usleep(1000000);

      struct ospfv2_neighbor* vecinos_muertos = check_neighbors_alive(g_neighbors);

      if (vecinos_muertos) {
        while (vecinos_muertos) {
          struct sr_if * inter = sr->if_list;
          while (inter->neighbor_id != vecinos_muertos->neighbor_id.s_addr);
          inter->neighbor_id = 0;
          inter->helloint = 0;
          inter->neighbor_ip = 0;
          vecinos_muertos = vecinos_muertos->next;
        }
      }
    }

    return NULL;
} /* -- check_neighbors_life -- */


/*---------------------------------------------------------------------
 * Method: check_topology_entries_age
 *
 * Check if the topology entries are alive 
 * and if they are not, remove them from the topology table
 *
 *---------------------------------------------------------------------*/

void* check_topology_entries_age(void* arg)
{
  struct sr_instance* sr = (struct sr_instance*)arg;

  while(1) {
    usleep(1000000);

    if (check_topology_age(g_topology)) {
      dijkstra_param_t * params = (dijkstra_param_t *)malloc(sizeof(dijkstra_param_t));
      params->sr = sr;
      params->rid = g_router_id;
      params->topology = g_topology;
      params->mutex = g_dijkstra_mutex;

      pwospf_lock(sr->ospf_subsys);

      if (pthread_create(&g_dijkstra_thread, NULL, run_dijkstra, &params)) { 
        perror("pthread_create");
        assert(0);
      } else {
        pthread_detach(g_dijkstra_thread);
      }
      
      pwospf_lock(sr->ospf_subsys);
    }
    print_topolgy_table(g_topology);
    sr_print_routing_table(sr);
  }

  return NULL;
} /* -- check_topology_entries_age -- */


/*---------------------------------------------------------------------
 * Method: send_hellos
 *
 * Para cada interfaz y cada helloint segundos, construye mensaje 
 * HELLO y crea un hilo con la función para enviar el mensaje.
 *
 *---------------------------------------------------------------------*/

void* send_hellos(void* arg)
{
  struct sr_instance* sr = (struct sr_instance*)arg;

  /* While true */
  while(1) {
    /* Se ejecuta cada 1 segundo */
    usleep(1000000);

    /* Bloqueo para evitar mezclar el envío de HELLOs y LSUs */
    pwospf_lock(sr->ospf_subsys);

    struct sr_if * inter = sr->if_list;
    while (inter) {
      Debug("\n\n%%%%%%%%%%- Iterating over %s interface: \n", inter->name);

      if ((inter->helloint)++ < OSPF_NEIGHBOR_TIMEOUT) {
        powspf_hello_lsu_param_t * params = (powspf_hello_lsu_param_t *)malloc(sizeof(powspf_hello_lsu_param_t));
        params->interface = inter;
        params->sr = sr;
        if (pthread_create(&g_hello_packet_thread, NULL, send_hello_packet, params)) { 
          perror("pthread_create");
          assert(0);
        } else {
          pthread_detach(g_hello_packet_thread);
        }
        inter->helloint = 0;
      }
      inter = inter->next;
    }
    /* Chequeo todas las interfaces para enviar el paquete HELLO */
    /* Cada interfaz matiene un contador en segundos para los HELLO*/
    /* Reiniciar el contador de segundos para HELLO */

    /* Desbloqueo */
    pwospf_unlock(sr->ospf_subsys);
  };

  return NULL;
} /* -- send_hellos -- */


/*---------------------------------------------------------------------
 * Method: send_hello_packet
 *
 * Recibe un mensaje HELLO, agrega cabezales y lo envía por la interfaz
 * correspondiente.
 *
 *---------------------------------------------------------------------*/

void* send_hello_packet(void* arg) {
  powspf_hello_lsu_param_t* hello_param = ((powspf_hello_lsu_param_t*)(arg));

  Debug("\n\nPWOSPF: Constructing HELLO packet for interface %s: \n", hello_param->interface->name);

  unsigned int len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t);
  uint8_t * packet = (uint8_t *)malloc(len * sizeof(uint8_t));

  sr_ethernet_hdr_t * ether_hdr = (sr_ethernet_hdr_t *)packet;
  memcpy(ether_hdr->ether_shost, hello_param->interface->addr, ETHER_ADDR_LEN);
  memset(ether_hdr->ether_dhost, 0xFF, ETHER_ADDR_LEN);
  ether_hdr->ether_type = htons(ethertype_ip);

  /* Inicializo cabezal IP */
  sr_ip_hdr_t * ip_hdr = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
  ip_hdr->ip_v = 4;
  ip_hdr->ip_hl = 5;
  ip_hdr->ip_tos = 0;
  ip_hdr->ip_len = htons(sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t));
  ip_hdr->ip_p = ip_protocol_ospfv2;
  ip_hdr->ip_id = 0;
  ip_hdr->ip_dst = htonl(OSPF_AllSPFRouters);
  ip_hdr->ip_src = hello_param->interface->ip;
  ip_hdr->ip_off = htons(IP_DF);
  ip_hdr->ip_ttl = 64;
  ip_hdr->ip_sum = 0;
  ip_hdr->ip_sum = ip_cksum(ip_hdr, sizeof(sr_ip_hdr_t));

  /* Inicializo cabezal de PWOSPF con version 2 y tipo HELLO */
  ospfv2_hdr_t * ospf_hdr = (ospfv2_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
  ospfv2_hello_hdr_t * ospf_hello_hdr = (ospfv2_hello_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));
  ospf_hdr->version = OSPF_V2;
  ospf_hdr->type = OSPF_TYPE_HELLO;
  ospf_hdr->len = htons(sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t));
  ospf_hdr->rid = g_router_id.s_addr;
  ospf_hdr->aid = 0;
  ospf_hdr->autype = 0;
  ospf_hdr->audata = 0;
  ospf_hdr->csum = 0;
  ospf_hello_hdr->nmask = hello_param->interface->mask;
  ospf_hello_hdr->padding = 0;
  ospf_hello_hdr->helloint = OSPF_DEFAULT_HELLOINT;

  unsigned size = sizeof(ospfv2_hdr_t) + sizeof(ospfv2_hello_hdr_t);
  ospf_hdr->csum = ospfv2_cksum(ospf_hdr, size);

  printf("$$$$ -> Packet Completed.\n");
  printf("$$$ -> Packet Info:\n");
  print_hdr_eth(packet);
  print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
  print_hdr_ospf(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));

  /* envio del paquete.
   * */
  sr_send_packet (
    hello_param->sr, 
    packet, 
    len, 
    hello_param->interface->name
  );

  free(packet);

  printf("$$$$ -> Packet Sent.\n");

  return NULL;
} /* -- send_hello_packet -- */

/*---------------------------------------------------------------------
 * Method: send_all_lsu
 *
 * Construye y envía LSUs cada 30 segundos
 *
 *---------------------------------------------------------------------*/

void* send_all_lsu(void* arg) {
  struct sr_instance* sr = (struct sr_instance*)arg;

  /* while true*/
  while(1) {
    /* Se ejecuta cada OSPF_DEFAULT_LSUINT segundos */
    usleep(OSPF_DEFAULT_LSUINT * 1000000);

    /* Bloqueo para evitar mezclar el envío de HELLOs y LSUs */
    pwospf_lock(sr->ospf_subsys);

    struct sr_if * inter = sr->if_list;
    while (inter) {
      powspf_hello_lsu_param_t * lsu_param = (powspf_hello_lsu_param_t *)malloc(sizeof(powspf_hello_lsu_param_t));
      lsu_param->interface = inter;
      lsu_param->sr = sr;

      if (pthread_create(&g_all_lsu_thread, NULL, send_lsu, lsu_param)) { 
        perror("pthread_create");
        assert(0);
      } else {
        pthread_detach(g_all_lsu_thread);
      }
    }

    /* Desbloqueo */
    pwospf_unlock(sr->ospf_subsys);
  };

  return NULL;
} /* -- send_all_lsu -- */

unsigned construir_packete_lsu (uint8_t ** packet, struct sr_instance* sr, struct sr_if* interface, uint8_t ttl) {
  unsigned lsas = count_routes(sr);

  unsigned ospf_size = sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t) + lsas * sizeof(ospfv2_lsa_t);
  unsigned len = sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + ospf_size;
  *packet = (uint8_t *)malloc(len);

  sr_ethernet_hdr_t * ether_hdr = (sr_ethernet_hdr_t *)*packet;
  ether_hdr->ether_type = htons(ethertype_ip);

  /* Inicializo cabezal IP */
  sr_ip_hdr_t * ip_hdr = (sr_ip_hdr_t *)(*packet + sizeof(sr_ethernet_hdr_t));
  ip_hdr->ip_v = 4;
  ip_hdr->ip_hl = 5;
  ip_hdr->ip_tos = 0;
  ip_hdr->ip_len = htons(len - sizeof(sr_ethernet_hdr_t));
  ip_hdr->ip_p = ip_protocol_ospfv2;
  ip_hdr->ip_id = 0;
  ip_hdr->ip_dst = interface->neighbor_ip;
  ip_hdr->ip_src = interface->ip;
  ip_hdr->ip_off = IP_DF;
  ip_hdr->ip_ttl = ttl;
  ip_hdr->ip_sum = 0;
  ip_hdr->ip_sum = ip_cksum(ip_hdr, sizeof(sr_ip_hdr_t));

  /* Inicializo cabezal de PWOSPF con version 2 y tipo HELLO */
  ospfv2_hdr_t * ospf_hdr = (ospfv2_hdr_t *)(*packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
  ospf_hdr->version = OSPF_V2;
  ospf_hdr->type = OSPF_TYPE_LSU;
  ospf_hdr->len = htons(ospf_size);
  ospf_hdr->rid = g_router_id.s_addr;
  ospf_hdr->aid = 0;
  ospf_hdr->autype = 0;
  ospf_hdr->audata = 0;

  ospfv2_lsu_hdr_t * lsu_hdr = (ospfv2_lsu_hdr_t *)(*packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));
  lsu_hdr->unused = 0;
  lsu_hdr->seq = g_sequence_num++;
  lsu_hdr->ttl = 64;
  lsu_hdr->num_adv = lsas;

  ospfv2_lsa_t * lsa_part = (ospfv2_lsa_t *)(*packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t) + sizeof(ospfv2_lsu_hdr_t));

  struct sr_rt * elem = sr->routing_table;
  while (elem && (elem->admin_dst <= 1)) {
    lsa_part->subnet = elem->dest.s_addr;
    lsa_part->mask = elem->mask.s_addr;
    lsa_part->rid = sr_get_interface(sr, elem->interface)->neighbor_id;

    elem = elem->next;
    lsa_part++;
  }

  ospf_hdr->csum = ospfv2_cksum(ospf_hdr, ospf_size);
  Debug("->-->>--->> 4\n");

  return len;
}

/*---------------------------------------------------------------------
 * Method: send_lsu
 *
 * Construye y envía paquetes LSU a través de una interfaz específica
 *
 *---------------------------------------------------------------------*/

void* send_lsu(void* arg)
{
  powspf_hello_lsu_param_t* lsu_param = ((powspf_hello_lsu_param_t*)(arg));
  Debug("\n\n()()()()()() -> Constructing and sending a LSU packet for interface %s: \n", lsu_param->interface->name);

  /* Solo envío LSUs si del otro lado hay un router */
  if (lsu_param->interface->neighbor_id == 0) {
    Debug("\nERROR: the interface goes to no router.\n");
    return NULL;
  }

  uint32_t ipDst = lsu_param->interface->neighbor_ip;

  /* Construcción del paquete. */
  uint8_t * packet;
  unsigned len = construir_packete_lsu (&packet, lsu_param->sr, lsu_param->interface, 64);
  sr_ethernet_hdr_t * ether_hdr = (sr_ethernet_hdr_t *)packet;
  
  Debug("\n\nPWOSPF: LSU packet constructed\n");

  /* envio del paquete.
   * */
  struct sr_arpentry * entrada_cache = sr_arpcache_lookup (&(lsu_param->sr->cache), ipDst);

  /* Se conoce la MAC. 
   * */
  if (entrada_cache) {
    printf("#### -> Found MAC in the cache\n");

    memcpy(ether_hdr->ether_shost, lsu_param->interface->addr, ETHER_ADDR_LEN);
    memcpy(ether_hdr->ether_dhost, entrada_cache->mac, ETHER_ADDR_LEN);

    /* envio del paquete.
     * */
    sr_send_packet (
      lsu_param->sr, 
      packet, 
      len, 
      lsu_param->interface->name
    );

    free(entrada_cache);

  /* NO se conoce la MAC. 
   * */
  } else {
    printf("#### -> MAC not found\n");

    struct sr_arpreq * req = sr_arpcache_queuereq (
      &(lsu_param->sr->cache), 
      ipDst,
      packet, 
      len, 
      lsu_param->interface->name
    );
    handle_arpreq (lsu_param->sr, req);
  }

  free(packet);
  printf("#### -> Packet Sent.\n");
  
  return NULL;
} /* -- send_lsu -- */


/*---------------------------------------------------------------------
 * Method: sr_handle_pwospf_hello_packet
 *
 * Gestiona los paquetes HELLO recibidos
 *
 *---------------------------------------------------------------------*/

void sr_handle_pwospf_hello_packet(struct sr_instance* sr, uint8_t* packet, unsigned int length, struct sr_if* rx_if)
{
  Debug("-> Entering hello handling.\n");
  sr_ip_hdr_t * ip_hdr = (sr_ip_hdr_t * )(packet + sizeof(sr_ethernet_hdr_t));
  ospfv2_hdr_t * ospf_hdr = (ospfv2_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t));
  ospfv2_hello_hdr_t * hello_hdr = (ospfv2_hello_hdr_t * )(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t) + sizeof(ospfv2_hdr_t));

  unsigned size = sizeof (ospfv2_hdr_t) + sizeof (ospfv2_hello_hdr_t);
  if (ospf_hdr->csum != ospfv2_cksum(ospf_hdr, size)) {
    Debug("-> PWOSPF: HELLO Packet dropped, invalid checksum\n");
    return;
  }

  if (hello_hdr->nmask != rx_if->mask) {
    Debug("-> PWOSPF: HELLO Packet dropped, invalid hello network mask\n");
    return;
  }

  /* Chequeo del intervalo de HELLO */
  if (hello_hdr->helloint != OSPF_DEFAULT_HELLOINT) {
    Debug("-> PWOSPF: HELLO Packet dropped, invalid hello interval\n");
    return;
  }
  
  struct in_addr res;
  res.s_addr = ospf_hdr->rid;
  refresh_neighbors_alive(g_neighbors, res);

  struct in_addr rid, net, mask, nid, nip;
  rid.s_addr = ospf_hdr->rid;
  net.s_addr = ip_hdr->ip_src & hello_hdr->nmask;
  mask.s_addr = hello_hdr->nmask;
  nid.s_addr = rx_if->neighbor_id;
  nip.s_addr = rx_if->neighbor_ip;

  refresh_topology_entry(
    g_topology, 
    rid, 
    net, 
    mask,
    nid, 
    nip, 
    g_sequence_num
  );
  sr_print_routing_table(sr);

  if (rx_if->neighbor_id == 0) {
    rx_if->neighbor_id = ospf_hdr->rid;
    rx_if->neighbor_ip = ip_hdr->ip_src;

    struct sr_if * elem = sr->if_list;
    while (elem) {
      if (elem->ip != rx_if->ip) {
        powspf_hello_lsu_param_t * lsu_param = (powspf_hello_lsu_param_t *)malloc(sizeof(powspf_hello_lsu_param_t));
        lsu_param->interface = elem;
        lsu_param->sr = sr;

        if (pthread_create(&g_all_lsu_thread, NULL, send_lsu, lsu_param)) { 
          perror("pthread_create");
          assert(0);
        } else {
          pthread_detach(g_all_lsu_thread);
        }
      }
      elem = elem->next;
    }
  }

  Debug("-> Has finished.\n");
} /* -- sr_handle_pwospf_hello_packet -- */


/*---------------------------------------------------------------------
 * Method: sr_handle_pwospf_lsu_packet
 *
 * Gestiona los paquetes LSU recibidos y actualiza la tabla de topología
 * y ejecuta el algoritmo de Dijkstra
 *
 *---------------------------------------------------------------------*/

void* sr_handle_pwospf_lsu_packet(void* arg)
{
  Debug("-> Entering lsu handling.\n");
  powspf_rx_lsu_param_t* rx_lsu_param = ((powspf_rx_lsu_param_t*)(arg));

  unsigned size = sizeof(sr_ethernet_hdr_t);
  size += sizeof(sr_ip_hdr_t);
  ospfv2_hdr_t * ospf_hdr = (ospfv2_hdr_t *)(rx_lsu_param->packet + size);
  size += sizeof(ospfv2_hdr_t);
  ospfv2_lsu_hdr_t * lsu_hdr = (ospfv2_lsu_hdr_t *)(rx_lsu_param->packet + size);
  size += sizeof(ospfv2_lsu_hdr_t);
  ospfv2_lsa_t * lsa_hdr = (ospfv2_lsa_t *)(rx_lsu_param->packet + size);
  
  if (ospf_hdr->csum != ospfv2_cksum(ospf_hdr, size)) {
    Debug("-> PWOSPF: LSU Packet dropped, invalid checksum\n");
    return NULL;
  }

  if (ospf_hdr->rid == g_router_id.s_addr) {
    Debug("-> PWOSPF: LSU Packet dropped, originated by this router\n");
    return NULL;
  }
  
  struct in_addr rid;
  rid.s_addr = ospf_hdr->rid;
  if (!check_sequence_number(g_topology, rid, lsu_hdr->seq)) {
    Debug("-> PWOSPF: LSU Packet dropped, repeated sequence number\n");
    return NULL;
  }

  int i = 0;
  while (i++ < lsu_hdr->num_adv) {
    if (ospf_hdr->rid != 0) {
      struct in_addr router_id, subnet, mask, neighbor_id, next_hop;
      router_id.s_addr = ospf_hdr->rid;
      neighbor_id.s_addr = lsa_hdr->rid;
      subnet.s_addr = lsa_hdr->subnet;
      mask.s_addr = lsa_hdr->mask;
      next_hop.s_addr = rx_lsu_param->rx_if->neighbor_ip;

      refresh_topology_entry(
        g_topology, 
        router_id,
        subnet,
        mask,
        neighbor_id,
        next_hop,
        lsu_hdr->seq
      );
    }

    lsa_hdr++;
  }

  dijkstra_param_t params;
  params.sr = rx_lsu_param->sr;
  params.rid.s_addr = ospf_hdr->rid;
  params.topology = g_topology;
  params.mutex = g_dijkstra_mutex;

  if (pthread_create(&g_dijkstra_thread, NULL, run_dijkstra, &params)) { 
    perror("pthread_create");
    assert(0);
  } else {
    pthread_detach(g_dijkstra_thread);
  }

  print_topolgy_table (g_topology);

  struct sr_if * elem = rx_lsu_param->sr->if_list;
  while (elem) {
    if (elem->ip != rx_lsu_param->rx_if->ip && elem->neighbor_ip != 0) {

      uint32_t ipDst = elem->neighbor_ip;

      /* Construcción del paquete. */
      uint8_t * packet;
      unsigned len = construir_packete_lsu (&packet, rx_lsu_param->sr, elem, 64);
      
      sr_ip_hdr_t * ip_hdr_new = (sr_ip_hdr_t *)(packet + sizeof(sr_ethernet_hdr_t));
      ip_hdr_new->ip_src = elem->ip;
      ip_hdr_new->ip_dst = elem->neighbor_ip;
      sr_ethernet_hdr_t * ether_hdr = (sr_ethernet_hdr_t *)packet;

      Debug("\n\nPWOSPF: LSU packet constructed\n");

      /* envio del paquete.
       * */
      struct sr_arpentry * entrada_cache = sr_arpcache_lookup (&(rx_lsu_param->sr->cache), ipDst);

      /* Se conoce la MAC. 
       * */
      if (entrada_cache) {
        printf("#### -> Found MAC in the cache\n");

        memcpy(ether_hdr->ether_shost, elem->addr, ETHER_ADDR_LEN);
        memcpy(ether_hdr->ether_dhost, entrada_cache->mac, ETHER_ADDR_LEN);

        /* envio del paquete.
         * */
        sr_send_packet (
          rx_lsu_param->sr, 
          packet, 
          len, 
          elem->name
        );

        free(entrada_cache);

        /* NO se conoce la MAC. 
         * */
      } else {
        printf("#### -> MAC not found\n");

        struct sr_arpreq * req = sr_arpcache_queuereq (
          &(rx_lsu_param->sr->cache), 
          ipDst,
          packet, 
          len, 
          elem->name
        );
        handle_arpreq (rx_lsu_param->sr, req);
      }

      free(packet);
    }

    elem = elem->next;
  }
  sr_print_routing_table(rx_lsu_param->sr);
          
  return NULL;
} /* -- sr_handle_pwospf_lsu_packet -- */

/**********************************************************************************
 * SU CÓDIGO DEBERÍA TERMINAR AQUÍ
 * *********************************************************************************/

/*---------------------------------------------------------------------
 * Method: sr_handle_pwospf_packet
 *
 * Gestiona los paquetes PWOSPF
 *
 *---------------------------------------------------------------------*/

void sr_handle_pwospf_packet(struct sr_instance* sr, uint8_t* packet, unsigned int length, struct sr_if* rx_if)
{
  /*Si aún no terminó la inicialización, se descarta el paquete recibido*/
  if (g_router_id.s_addr == 0) {
    return;
  }

  ospfv2_hdr_t* rx_ospfv2_hdr = ((ospfv2_hdr_t*)(packet + sizeof(sr_ethernet_hdr_t) + sizeof(sr_ip_hdr_t)));
  powspf_rx_lsu_param_t* rx_lsu_param = ((powspf_rx_lsu_param_t*)(malloc(sizeof(powspf_rx_lsu_param_t))));

  Debug("-> PWOSPF: Detecting PWOSPF Packet\n");
  Debug("      [Type = %d]\n", rx_ospfv2_hdr->type);

  switch(rx_ospfv2_hdr->type) {
    case OSPF_TYPE_HELLO:
      sr_handle_pwospf_hello_packet(sr, packet, length, rx_if);
      break;
    case OSPF_TYPE_LSU:
      rx_lsu_param->sr = sr;
      unsigned int i;
      for (i = 0; i < length; i++)
      {
        rx_lsu_param->packet[i] = packet[i];
      }
      rx_lsu_param->length = length;
      rx_lsu_param->rx_if = rx_if;
      pthread_attr_t attr;
      pthread_attr_init(&attr);
      pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
      pthread_t pid;
      pthread_create(&pid, &attr, sr_handle_pwospf_lsu_packet, rx_lsu_param);
      break;
  }
} /* -- sr_handle_pwospf_packet -- */
