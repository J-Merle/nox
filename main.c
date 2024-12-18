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
#include <sys/types.h>

#define STAT_ID_INTEL 0x7e
#define STAT_ID_CRIT_CHANCE 0x73
#define STAT_ID_STRENGTH 0x76
#define STAT_ID_AGILITY 0x77
#define STAT_ID_LUCK 0x7b
#define STAT_ID_WISDOM 0x7c
#define STAT_ID_VITA 0x7d
#define STAT_ID_INTEL_DEBUF 0x9b
#define STAT_ID_AGI_DEBUF 0x9a
#define STAT_ID_HEAL 0xb2
#define STAT_ID_ESCAPE 0xf0
#define STAT_ID_PA 0x6f
#define STAT_ID_RES_EARTH 0xd2
#define STAT_ID_INVOCATION 0xb6
#define STAT_ID_HUNTING 0x1b
#define STAT_ID_DAMAGE_REFLECT 0xdc
#define STAT_ID_DAMAGE_FIRE 0xa8
#define STAT_ID_INIT 0xae
#define STAT_ID_ESCAPE_DEBUF 0xf2

#define DOFUS_SERVER_PORT 5555

#define MILESTONE_STAT_HEADER_START 0x22
#define MILESTONE_STAT_HEADER_END 0x08
#define MILESTONE_STAT_FOOTER 0x2a

#define PAYLOAD_SIZE 64000

// Uncomment to get some raw data info as hexa
//#define RAW_PACKET
char* stat_name(uint8_t stat_id) {
    switch (stat_id) {
        case STAT_ID_INTEL: return "INTEL";
        case STAT_ID_CRIT_CHANCE: return "CRIT CHANCE";
        case STAT_ID_STRENGTH: return "STRENGTH";
        case STAT_ID_AGILITY: return "AGILITY";
        case STAT_ID_VITA: return "VITA";
        case STAT_ID_LUCK: return "LUCK";
        case STAT_ID_WISDOM: return "WISDOM";
        case STAT_ID_INTEL_DEBUF: return "DEBUF INTEL";
        case STAT_ID_AGI_DEBUF: return "DEBUF AGI";
        case STAT_ID_HEAL: return "HEAL";
        case STAT_ID_ESCAPE: return "ESCAPE";
        case STAT_ID_PA: return "PA";
        case STAT_ID_RES_EARTH: return "RES EARTH %";
        case STAT_ID_INVOCATION: return "INVOCATION";
        case STAT_ID_HUNTING: return "HUNTING";
        case STAT_ID_DAMAGE_REFLECT: return "DAMAGE REFLECT";
        case STAT_ID_DAMAGE_FIRE: return "DAMAGE FIRE";
        case STAT_ID_INIT: return "INIT";
        case STAT_ID_ESCAPE_DEBUF: return "DEBUF ESCAPE";
        default: 
            printf("%x was not found\n", stat_id);
            return "UNKNOWN";
    }
}

struct stat {
    uint16_t code;
    uint16_t value;
};

struct item {
    struct stat stats[16];
    uint8_t stats_len;
    uint32_t price;
    float efficiency;
    uint16_t power;
};

void compute_item_infos(struct item* item) {
    for (int i = 0; i < item->stats_len; i++) {

        if(item->stats[i].code == STAT_ID_PA) item->power += 100*item->stats[i].value;
        if(item->stats[i].code == STAT_ID_VITA) item->power += 1 * item->stats[i].value;
        if(item->stats[i].code == STAT_ID_INTEL) item->power += 1 * item->stats[i].value;
        if(item->stats[i].code == STAT_ID_STRENGTH) item->power += 1 * item->stats[i].value;
        if(item->stats[i].code == STAT_ID_LUCK) item->power += 1 * item->stats[i].value;
        if(item->stats[i].code == STAT_ID_AGILITY) item->power += 1 * item->stats[i].value;
        if(item->stats[i].code == STAT_ID_DAMAGE_FIRE) item->power += 5 * item->stats[i].value;

    }

    if(item->price > 0) {
        item->efficiency = (float) item->power / item->price;
    }

}
void display_item_infos(struct item* item) {
    printf("===================\n");
    for(int i = 0; i < item->stats_len; i++) {
        printf("%-20s\t%u\n", stat_name(item->stats[i].code), item->stats[i].value);
    }
    printf("Price %u K\n", item->price);
    printf("Power: %d Efficiency %f\n", item->power, item->efficiency);
    printf("===================\n");

}


uint8_t read_byte(unsigned char** data) {
    uint8_t value = *(*data);
    *data += 1;
    return value;
}

unsigned short read_short(unsigned char** data) {
    uint16_t value = *(*data);
    *data += 1;
    return value;
}

uint32_t read_var_int(unsigned char** data) {

    int offset = 0;
    unsigned int value = 0;

    while(1) {
        uint16_t v = read_byte(data);
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

unsigned short read_var_short(unsigned char** data) {

    int offset = 0;
    unsigned short value = 0;

    while(1) {
        uint8_t v = read_byte(data);
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

    unsigned char* buffer = (unsigned char *)malloc(65536);

    unsigned char payload[PAYLOAD_SIZE]; // Will contain aggregated data of "large" packets
    int payload_len = 0;
    
    // Open the raw socket
    int sock = socket (AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if(sock == -1)
    {
        perror("Failed to create socket");
        exit(1);
    }
    while(1) {
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

      offset += sizeof(struct tcphdr) + 12; // 12 is the number of bytes in options of tcp header 

      unsigned char* data = (buffer + offset);

      // Prepare the payload
      if(payload_len == 0) {
          memset(payload, 0, PAYLOAD_SIZE);
      } 

      unsigned int data_size = (int)ntohs(ip->tot_len) - (int)(tcp->doff * 4) - (int)(ip->ihl * 4); // There is no easier way to determine data size ???
      if(data_size > 0) {

          // TODO Maybe we want to perform this operation only on the pakcets we are interrested in 
          memcpy(payload + payload_len, data, data_size);
          payload_len += data_size;
          if(data_size == 1448) {
              continue; // Big packet ? read the next
          } else {
              data = payload;
          }
      }


      // Payload should be usable at this point
      // Any data split into several packets should have been agregated

      char* type_string="type.ankama.com/";
      char data_type[4] = {0};
      data_type[3] = '\0'; // Ensure we end the string (only usefull for printing the value)

      // We don't understand for now how the first bytes are used so we just search for the type string
      for( int i = 0; i < 30; i++) {

          if (memcmp(data, type_string, 16) == 0) {
              memcpy(data_type, data + 16, 3);
              data += 16 + 3;
              break;
          }
          data++;
      }

      if(memcmp(data_type, "iyc", 3) == 0) {
          // Skip for hdv dev
          payload_len = 0;
          continue;
          uint8_t value = *(data);

          value = read_byte(&data);
          assert(value == 18);

          uint16_t data_size = read_var_short(&data); 

          value = read_byte(&data);
          assert(value == 10);

          uint8_t chat_size = read_byte(&data);
          char chat_message[512] = {0};
          memcpy(chat_message, data, chat_size);
          printf("%s\n", chat_message);
      } 

      if(memcmp(data_type, "iqs", 3) ==0) {
          uint8_t value = read_byte(&data);
          assert(value == 0x12);
          // Data size : let us now if all the data has been split between several packets
          uint16_t data_size = read_var_short(&data); 
          if(data_size < 10) {
              // Probably  closing an item detail
              payload_len = 0;
              continue;
          }

          value = read_byte(&data);
          assert(value == 0x08);
          value = read_byte(&data);
          value = read_var_short(&data);
          data += 2; // (Original item ID ?)
          float best_efficiency = 0;
          while(data < payload + payload_len) { 
              struct item* current_item = malloc(sizeof (struct item)); // TODO remember to free this memory
              value = read_byte(&data);
              assert(value == 0x1a);
              // Lot of unknown data here
              data += 2; 
              value = read_var_short(&data); 
              value = read_byte(&data);
              assert(value == 0x10);
              read_var_int(&data);
              read_byte(&data);
              read_byte(&data);

              
              value = read_byte(&data);
              assert(value == 0x22);
              int power = 0;
              int stat_index = 0;


              while(value == 0x22) {

                  uint8_t stat_size = 0;
                  uint16_t stat_type = 0;
                  uint16_t stat_value = 0;

                  stat_size = read_byte(&data);


                  
                  // TODO
                  if(stat_size > 5) {
                      printf("A stat with size %hu is ignored (probably weapon stat)\n", stat_size);
                      data += stat_size;
                      value = read_byte(&data);
                      continue;
                  }

#ifdef RAW_PACKET
                  // Debug stats
                  printf("%02x ", stat_size);
                  for (int v = 0; v < stat_size; v++) {
                    value = read_byte(&data);
                    printf("%02x ", value);
                  }
                  printf("\n");
                  value = read_byte(&data);
#else

                  data++; // Skip next byte (0x08)
                  stat_type = read_var_short(&data);
                  data++; // Skip next byte (0x18)
                  stat_value = read_var_short(&data);

                  struct stat current_stat = { .code = stat_type, .value = stat_value };
                  current_item->stats[stat_index] = current_stat;
                  current_item->stats_len++;


                  value = read_byte(&data);
                  stat_index++;
#endif
              }
              
              if(value != MILESTONE_STAT_FOOTER) {
                  printf("Some data might be missing\n");
                  break;
              }
              value = read_byte(&data); // End block size
              current_item->price = read_var_int(&data);
              compute_item_infos(current_item);
              if(current_item->efficiency > best_efficiency) {

                  best_efficiency = current_item->efficiency;
                  printf("Better item found !\n"); // TODO in the future maybe store items in an array and sort it ?
                  display_item_infos(current_item);
              }
              data += 2; // Skip next two empty bytes
          }

      } 

      payload_len = 0;


    }

    return 0;
}
