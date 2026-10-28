#pragma once
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace lcars {
// Log network metadata only: never request headers, tokens, SSIDs or weather URLs.
inline void network_snapshot() {
  wifi_ap_record_t ap{};
  const auto wifi_error=esp_wifi_sta_get_ap_info(&ap);
  ESP_LOGI("network_diag","Association=%s RSSI=%d dBm channel=%u",
           esp_err_to_name(wifi_error),int(ap.rssi),unsigned(ap.primary));
  auto *netif=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if(!netif)return;
  esp_netif_ip_info_t info{};
  if(esp_netif_get_ip_info(netif,&info)==ESP_OK)
    ESP_LOGI("network_diag","IP=" IPSTR " gateway=" IPSTR " netmask=" IPSTR,
             IP2STR(&info.ip),IP2STR(&info.gw),IP2STR(&info.netmask));
  for(int i=0;i<2;++i) {
    esp_netif_dns_info_t dns{};
    if(esp_netif_get_dns_info(netif,static_cast<esp_netif_dns_type_t>(i),&dns)==ESP_OK &&
       dns.ip.type==ESP_IPADDR_TYPE_V4)
      ESP_LOGI("network_diag","DNS[%d]=" IPSTR,i,IP2STR(&dns.ip.u_addr.ip4));
  }
}

// Called only in the network worker after a failed real request. This preserves
// the original failure before DNS lookups or extra sockets can affect caches.
inline void network_probe(const char *host,const char *port) {
  const auto started=esp_timer_get_time();
  addrinfo hints{};hints.ai_family=AF_INET;hints.ai_socktype=SOCK_STREAM;
  addrinfo *addresses=nullptr;
  const int lookup=getaddrinfo(host,port,&hints,&addresses);
  ESP_LOGI("network_diag","DNS %s result=%d elapsed=%lld ms",host,lookup,
           static_cast<long long>((esp_timer_get_time()-started)/1000));
  if(lookup!=0 || !addresses)return;
  unsigned count=0;
  for(auto *address=addresses;address && count<3;address=address->ai_next,++count) {
    char ip[INET_ADDRSTRLEN]{};
    inet_ntop(AF_INET,&reinterpret_cast<sockaddr_in*>(address->ai_addr)->sin_addr,ip,sizeof(ip));
    const int fd=::socket(address->ai_family,address->ai_socktype,address->ai_protocol);
    int error=fd<0?errno:0;
    const auto connect_started=esp_timer_get_time();
    if(fd>=0) {
      const int flags=fcntl(fd,F_GETFL,0);
      if(flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)error=errno;
      else if(connect(fd,address->ai_addr,address->ai_addrlen)<0) {
        error=errno;
        if(error==EINPROGRESS) {
          fd_set writable;FD_ZERO(&writable);FD_SET(fd,&writable);
          timeval timeout{3,0};
          const int ready=select(fd+1,nullptr,&writable,nullptr,&timeout);
          if(ready==0)error=ETIMEDOUT;
          else if(ready<0)error=errno;
          else {
            socklen_t length=sizeof(error);
            if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)<0)error=errno;
          }
        }
      }
      close(fd);
    }
    ESP_LOGI("network_diag","TCP %s (%s):%s errno=%d (%s) elapsed=%lld ms",host,ip,port,
             error,error?std::strerror(error):"connected",
             static_cast<long long>((esp_timer_get_time()-connect_started)/1000));
  }
  freeaddrinfo(addresses);
}

inline void network_http_error(const char *tag,esp_http_client_handle_t client,esp_err_t error) {
  const int task_errno=errno;
  const int socket_errno=esp_http_client_get_errno(client);
  int tls_error=0,flags=0;
  const auto transport_error=esp_http_client_get_and_clear_last_tls_error(client,&tls_error,&flags);
  ESP_LOGW(tag,"HTTP error=%s transport=%s (0x%x) mbedTLS=%d verify=0x%x socket_errno=%d (%s) task_errno=%d",
           esp_err_to_name(error),esp_err_to_name(transport_error),unsigned(transport_error),
           tls_error,flags,socket_errno,socket_errno>0?std::strerror(socket_errno):"none",task_errno);
}
}
