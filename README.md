# esp32-lan-w5500
Small snipped to enable lan via spi on esp32 using wiznet w5500

example code :)
> #include "w5500/w5500-lwIP.h"
> Wiznet5500lwIP ethernet(CS_WIZNET);
> WiFi.begin();
> WiFi.mode(WIFI_OFF);
> uint8_t mac[6];
> WiFi.macAddress(mac);
> IPAddress _IP(192,168,188,100);
> IPAddress _GW(192,168,188,1);
> IPAddress _NM(255,255,255,0);
> IPAddress _DNS(8,8,8,8);
> //static
> ethernet.begin(mac, _IP, _NM,_GW, _DNS)
> //dhcp
> ethernet.begin(mac)
