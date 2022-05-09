#include <IPAddress.h>
#include <lwip/init.h>
#include <lwip/netif.h>
#include <netif/ethernet.h>
#include <netif/etharp.h>
#include <lwip/dhcp.h>
#include <lwip/dns.h>

#ifdef ESP8266
#include <user_interface.h> // wifi_get_macaddr()
#endif
#include "w5500-lwIP.h"

uint16_t mtu = 1500;
IPAddress _local_ip;
IPAddress _subnet;
IPAddress _gateway;
IPAddress _dns_server;

// standard definitions: /home/manuel/Arduino/hardware/espressif/esp32/tools/sdk/include/lwip/lwipopts.h
//#define LWIP_DHCP 0

extern "C"
{
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp32-hal.h>
#include <lwip/ip_addr.h>
#include "lwip/err.h"
#include "lwip/dns.h"
#include <esp_smartconfig.h>
#include <tcpip_adapter.h>
}

#define IFNAME0 'e'
#define IFNAME1 '0'

/* Netif global configuration structure */
struct netif eth_netif;

static void _startThread(void *ctx)
{
  while (42)
  {
    ((Wiznet5500lwIP *)ctx)->loop();
    yield();
  }
}

static void _tcpcallback(void *ctx)
{
  Wiznet5500lwIP *ths = (Wiznet5500lwIP *)ctx;
  ths->lwiptcpip_callback();
}
/*
void esp32_dns_init(IPAddress *dns)
{
  ip_addr_t dns;
  ip4_addr_set_u32(&dns, _dns);

    // DNS initialized by DHCP when call dhcp_start()
    dns_init();
    dns_setserver(0, &dns);
}
*/

void _linkcallback(struct netif *netif)
{
  // enble in opt.h LWIP_NETIF_LINK_CALLBACK 1
  if (netif_is_link_up(netif))
  { // Ethernet link up

    ESP_LOGI("w5500-lwip", "link_callback=UP");
    // dhcp_start(&fsl_netif0);
    // netif_set_up(&fsl_netif0);
  }
  else
  {
    ESP_LOGI("w5500-lwip", "link_callback=UP");

    // dhcp_stop(&fsl_netif0);
    // netif_set_down(&fsl_netif0);
  }
}

void Wiznet5500lwIP::lwiptcpip_callback()
{
  pbuf *p;
  err_t err = ERR_OK;

  while ((xQueueGenericReceive(_pbufqueue, &p, 0, false) == pdTRUE) && (err == ERR_OK))
  {
    err = _netif.input(p, &_netif);
  }

  if (err != ERR_OK)
  {
    ESP_LOGW("w5500-lwip", "_netif.input (ret=%d)", err);
    pbuf_free(p);
  }
}

boolean Wiznet5500lwIP::begin(uint8_t *macAddress)
{
  static uint8_t initDone = 0;
  uint8_t zeros[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  if (!macAddress)
    macAddress = zeros;

  _pbufqueue = xQueueCreate(100, sizeof(pbuf *));

  ESP_LOGI("w5500-lwip", "NO_SYS=%d", NO_SYS);

  if (!Wiznet5500::begin(macAddress))
    return false;
  _mtu = mtu;

  switch (start_with_dhclient())
  // switch ( start_with_static_address () )
  {
  case ERR_OK:

    if (netif_is_link_up(&_netif))
    {
      // When the netif is fully configured this function must be called
      netif_set_up(&_netif);
      ESP_LOGW("w5500-lwip", "SET_UP Ethernet w5500");
    }
    else
    {
      // When the netif link is down this function must be called
      netif_set_down(&_netif);
      ESP_LOGW("w5500-lwip", "SET_DOWN Ethernet w5500");
    }
    netif_set_default(&_netif);

    // tcpip_init( xTaskCreatePinnedToCore, "");

    ESP_LOGW("w5500-lwip", "Creada la IP");
    xTaskCreatePinnedToCore(
        _startThread, // Function to implement the task
        "w5500-lwip", // Name of the task
        10000,        // Stack size in words
        this,         // Task input parameter
        1,            // Priority of the task (higher priority task cause crash. I dont know why :/)
        NULL,         // Task handle.
        0);           // Core where the task should run
    return true;

  case ERR_IF:
    return false;

  default:
    netif_remove(&_netif);
    return false;
  }
}

boolean Wiznet5500lwIP::begin(uint8_t *mac_address, IPAddress local_ip, IPAddress subnet, IPAddress gateway, IPAddress dns_server)
{

  static uint8_t initDone = 0;
  ESP_LOGI("w5500-lwip", "NO_SYS=%d", NO_SYS);

  if (!Wiznet5500::begin(mac_address))
    return false;
  _mtu = mtu;
  _local_ip = local_ip;
  _subnet = subnet;
  _gateway = gateway;
  _dns_server = dns_server;

  _pbufqueue = xQueueCreate(100, sizeof(pbuf *));

  switch (start_with_static_address())
  {
  case ERR_OK:

    if (netif_is_link_up(&_netif))
    {
      // When the netif is fully configured this function must be called
      netif_set_up(&_netif);
      ESP_LOGW("w5500-lwip", "SET_UP Ethernet w5500");
    }
    else
    {
      // When the netif link is down this function must be called
      netif_set_down(&_netif);
      ESP_LOGW("w5500-lwip", "SET_DOWN Ethernet w5500");
    }
    netif_set_default(&_netif);

    ESP_LOGW("w5500-lwip", "Creada la IP");
    xTaskCreatePinnedToCore(
        _startThread, // Function to implement the task
        "w5500-lwip", // Name of the task
        10000,        // Stack size in words
        this,         // Task input parameter
        1,            // Priority of the task (higher priority task cause crash. I dont know why :/)
        NULL,         // Task handle.
        0);           // Core where the task should run
    return true;

  case ERR_IF:
    return false;

  default:
    netif_remove(&_netif);
    return false;
  }
}

err_t Wiznet5500lwIP::start_with_dhclient()
{
  ip4_addr_t ip, mask, gw;

  ip4_addr_set_zero(&ip);
  ip4_addr_set_zero(&mask);
  ip4_addr_set_zero(&gw);

  _netif.hwaddr_len = sizeof(_mac_address);
  memcpy(_netif.hwaddr, _mac_address, sizeof(_mac_address));

  _callbackmsg = tcpip_callbackmsg_new(_tcpcallback, this);

  if (!netif_add(&_netif, &ip, &mask, &gw, this, netif_init_s, ethernet_input))
    return ERR_IF;

  _netif.flags |= NETIF_FLAG_UP;

  return dhcp_start(&_netif);
}

// Static Ip Address

err_t Wiznet5500lwIP::start_with_static_address()
{
  ip4_addr_t local_ip, subnet, gateway;
  ip_addr_t dns_server;
  dns_server.type = IPADDR_TYPE_V4;

  ip4_addr_set_u32(&local_ip, _local_ip);
  ip4_addr_set_u32(&subnet, _subnet);
  ip4_addr_set_u32(&gateway, _gateway);

  if (_dns_server != (uint32_t)0x00000000)
  {
    // Set DNS1-Server
    dns_server.u_addr.ip4.addr = static_cast<uint32_t>(_dns_server);
  }

  _netif.hwaddr_len = sizeof _mac_address;
  memcpy(_netif.hwaddr, _mac_address, sizeof _mac_address);
  _callbackmsg = tcpip_callbackmsg_new(_tcpcallback, this);

  if (!netif_add(&_netif, &local_ip, &subnet, &gateway, this, netif_init_s, ethernet_input))
  {
    return ERR_IF;
  }
  else
  {
// netif_set_status_callback
#if LWIP_NETIF_LINK_CALLBACK
// netif_set_link_callback(&_netif, _linkcallback );
#endif /* LWIP_NETIF_LINK_CALLBACK */
    netif_set_default(&_netif);

    dns_init();
    // dns_setserver(0, (const ip_addr_t *)&dns_server);
    dns_setserver(0, &dns_server);
  }

  ESP_LOGW("w5500-lwip", "Current ip: %d.%d.%d.%d", localIP()[0], localIP()[1], localIP()[2], localIP()[3]);
  ESP_LOGW("w5500-lwip", "Current mask: %d.%d.%d.%d", subnetMask()[0], subnetMask()[1], subnetMask()[2], subnetMask()[3]);
  ESP_LOGW("w5500-lwip", "Current gateway: %d.%d.%d.%d", gatewayIP()[0], gatewayIP()[1], gatewayIP()[2], gatewayIP()[3]);

  _netif.flags |= NETIF_FLAG_UP;
  return ERR_OK;
}

err_t Wiznet5500lwIP::linkoutput_s(netif *netif, struct pbuf *pbuf)
{
  Wiznet5500lwIP *ths = (Wiznet5500lwIP *)netif->state;

  uint16_t len = ths->sendFrame((const uint8_t *)pbuf->payload, pbuf->len);

  return len == pbuf->len ? ERR_OK : ERR_MEM;
}

err_t Wiznet5500lwIP::netif_init_s(struct netif *netif)
{
  netif->hostname = "PeanutPay";
  return ((Wiznet5500lwIP *)netif->state)->netif_init();
}

err_t Wiznet5500lwIP::netif_init()
{
  _netif.name[0] = 'e';
  _netif.name[1] = '0' + _netif.num;
  _netif.mtu = _mtu;
  //_netif.chksum_flags = NETIF_CHECKSUM_ENABLE_ALL;
  _netif.flags =
      NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP | NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP;

  // lwIP's doc: This function typically first resolves the hardware
  // address, then sends the packet.  For ethernet physical layer, this is
  // usually lwIP's etharp_output()
  _netif.output = etharp_output;

  // lwIP's doc: This function outputs the pbuf as-is on the link medium
  // (this must points to the raw ethernet driver, meaning: us)
  _netif.linkoutput = linkoutput_s;

  return ERR_OK;
}

err_t Wiznet5500lwIP::loop()
{
  uint16_t eth_data_count = readFrameSize();
  if (!eth_data_count)
    return ERR_OK;

  /* Allocate pbuf from pool (avoid using heap in interrupts) */
  struct pbuf *p = pbuf_alloc(PBUF_RAW, eth_data_count, PBUF_POOL);
  if (!p || p->len < eth_data_count)
  {
    ESP_LOGW("w5500-lwip", "pbuf_alloc (pbuf=%p, len=%d)", p, (p) ? p->len : 0);

    if (p)
      pbuf_free(p);
    // discardFrame(eth_data_count);
    return ERR_BUF;
  }

  /* Copy ethernet frame into pbuf */
  uint16_t len = readFrameData((uint8_t *)p->payload, eth_data_count);
  if (len != eth_data_count)
  {
    // eth_data_count is given by readFrameSize()
    // and is supposed to be honoured by readFrameData()
    // todo: ensure this test is unneeded, remove the print
    ESP_LOGW("w5500-lwip", "readFrameData (tot_len=%d, len=%d)", eth_data_count, len);
    pbuf_free(p);
    return ERR_BUF;
  }

  /* Put in a queue which is processed in lwip thread loop */
  xQueueGenericSend(_pbufqueue, &p, 100, queueSEND_TO_BACK);
  tcpip_trycallback(_callbackmsg);

  return ERR_OK;
}
