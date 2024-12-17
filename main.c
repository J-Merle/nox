// raw_sock.c
#include <assert.h>
#include <linux/if_ether.h>
#include <stdint.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<netinet/ip.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include <inttypes.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>

#define DOFUS_PORT 53902

int main() {
    struct sockaddr_in source_socket_address, dest_socket_address;

    int packet_size;

    unsigned char *buffer = (unsigned char *)malloc(65536);
    
    // Open the raw socket
    int sock = socket (AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(sock == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }
    while(1) {
      // recvfrom is used to read data from a socket
      packet_size = recvfrom(sock , buffer , 65536 , 0 , NULL, NULL);
      if (packet_size == -1) {
        printf("Failed to get packets\n");
        return 1;
      }


      struct ethhdr *eth = (struct ethhdr *)(buffer); 
      int offset = sizeof(struct ethhdr);
      struct iphdr *ip = (struct iphdr *) (buffer + offset);
      if (ip->protocol != 6 ) continue;
      offset += sizeof(struct iphdr);
      struct tcphdr *tcp = (struct tcphdr *) (buffer + offset);

      uint16_t dest_port = htons(tcp->dest);
      if ( dest_port != DOFUS_PORT ) {
        continue;
      }

      offset += sizeof(struct tcphdr);

      unsigned char* data = (buffer + offset);
      char* type_string="type.ankama.com/";
      char data_type[4] = {0};

      // We don't understand for now how the first bytes are used so we just search for the type string
      for( int i = 0; i < 30; i++) {
          if (memcmp(data, type_string, 16) == 0) {
              memcpy(data_type, data + 16, 3);
            data_type[3] = '\0'; // Ensure we end the string (only usefull for printing the value)
              offset += 16 + 3;
              data = (buffer + offset);
              //printf("Found data type·%s\n", data_type);
              break;
          }
          offset++;
          data = (buffer + offset);
      }

      if(memcmp(data_type, "iyc", 3) == 0) {
          uint8_t value = *(data);
          assert(value == 18);
          // Looks like 0a always appear before massage size
          // For now we will use this because it looks like data before could have different size
          for(uint8_t l_offset = 0; l_offset < 5; l_offset++) {
            offset++;
            data = (buffer+offset);
            uint8_t l_value = *(data);
            if(l_value == 0x0a) {
                offset++;
                break;
            }
          }
          uint8_t chat_size = *(buffer + offset);
          offset++;
          data = (buffer + offset);
          char chat_message[512] = {0};
          memcpy(chat_message, data, chat_size);
          printf("%s\n", chat_message);
      }


    }

    return 0;
}
