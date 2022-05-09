#ifndef W5500LWIP_H
#define W5500LWIP_H

#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <freertos/queue.h>
#include "w5500.h"

#ifdef ESP32
#include <SPI.h>
#define SPIparam(x...) x
#else
#define SPIparam(x...)
#endif

    


class Wiznet5500lwIP: public Wiznet5500 {

public:

    Wiznet5500lwIP (int8_t cs=SS): Wiznet5500(cs)
    {
    }

    // start with dhcp client
    // default mac-address is inferred(+modified) from esp8266's STA one
    //boolean begin (SPIparam(SPIClass& spi,) const uint8_t *macAddress = NULL, uint16_t mtu = 1500); MANU
    //boolean begin (const uint8_t *macAddress = NULL, uint16_t mtu = 1500);
    boolean begin (uint8_t *mac_address = NULL);
    boolean begin (uint8_t *mac_address, IPAddress local_ip , IPAddress subnet , IPAddress gateway );
    boolean begin (uint8_t *mac_address , IPAddress local_ip , IPAddress subnet , IPAddress gateway , IPAddress dns_server);

    const netif* getNetIf   () const { return &_netif; }
    IPAddress    localIP    () const { return IPAddress(_netif.ip_addr.u_addr.ip4.addr); }
    IPAddress    subnetMask () const { return IPAddress(_netif.netmask.u_addr.ip4.addr); }
    IPAddress    gatewayIP  () const { return IPAddress(_netif.gw.u_addr.ip4.addr); }

    void lwiptcpip_callback();
    
    err_t loop();

protected:
    netif _netif;
    uint16_t _mtu;
    
    err_t start_with_dhclient ();
    //err_t start_with_static_address (IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);
    //err_t start_with_static_address (IPAddress local_ip, IPAddress gateway, IPAddress subnet);
    err_t start_with_static_address ();
    //bool config(IPAddress local_ip, IPAddress gateway, IPAddress subnet, IPAddress dns1 = (uint32_t)0x00000000, IPAddress dns2 = (uint32_t)0x00000000);
    
    err_t netif_init ();

    static err_t netif_init_s (netif* netif);
    static err_t linkoutput_s (netif *netif, struct pbuf *p);
    tcpip_callback_msg *_callbackmsg;
    QueueHandle_t _pbufqueue;
};

#endif // W5500LWIP_H
