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

#define DOFUS_SERVER_PORT 5555

unsigned short read_short(unsigned char** data) {
    uint8_t value = *(*data);
    *data += 1;
    return value;
}

unsigned short read_var_short(unsigned char** data) {

    int offset = 0;
    unsigned short value = 0;

    while(1) {
        uint8_t v = read_short(data);
        if(offset > 0) {
            value = value + ((v &127) << (offset*7));
        } else {
            value = value + (v &127);
        }
        offset++;
        if((v & 128) == 0) break;
    }

    return value;
}

int main(int argc, char* argv[]) {
    uint16_t dest_port = 0;

    if (argc !=3) {

        printf("No client port set with '--port' argument, will be using the dofus server port (%d)\n", DOFUS_SERVER_PORT);
        printf("This can lead to unverified behavior\n");
        printf("Try running 'ss -tlpa | grep -i dofus' or similar to find the client port\n");

    } else {

        if(strcmp(argv[1], "--port") == 0) {
            sscanf(argv[2], "%hu", &dest_port);
            printf("Using client port %hu\n", dest_port);
        } else {
            printf("Unrecognized argument\n");
        }

    }



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

      // Client port is set : ignore packets not sent to it
      if ( dest_port != 0 && htons(tcp->dest) != dest_port) {
        continue;
      } else if (dest_port == 0 && htons(tcp->source) != DOFUS_SERVER_PORT) {
          // Client port not set, ignore packets not from DOFUS_SERVER_PORT (unsafe)
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
          // Skip for hdv dev
          //continue;
          uint8_t value = *(data);

          value = read_short(&data);
          assert(value == 18);

          read_var_short(&data); 

          value = read_short(&data);
          assert(value == 10);

          uint8_t chat_size = read_short(&data);
          char chat_message[512] = {0};
          memcpy(chat_message, data, chat_size);
          printf("%s\n", chat_message);
      }


      if(memcmp(data_type, "iqs", 3) ==0) {
          printf("Paquet HDV\n");
          uint8_t value = read_short(&data);
          assert(value == 18);
          uint8_t data_size = read_var_short(&data); // Data size : let us now if all the data has been split between several packets
      }


    }

    return 0;
}
