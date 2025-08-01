/* 
 * udpserver.c - A simple UDP echo server 
 * usage: udpserver <port>
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <dirent.h>


#define BUFSIZE 1024


void error(char *msg) {
  perror(msg);
  exit(1);
}

typedef enum{
    GET,
    PUT,
    DELETE_PACKET,
    SEARCH,
    EXIT
}Type;

struct Packet_Struct {
    char data[BUFSIZE];
    Type operation_type;
    char filetype[BUFSIZE];
    char total_bytes[BUFSIZE];
    char ack_id[256];
};

struct Stop_and_Wait_Struct{
  char data[BUFSIZE];
  char ack_id[256];
};


void server_operations(int sockfd, struct sockaddr_in addr){
  int n;
  socklen_t addr_size;
  addr_size = sizeof(addr);
  char filename[BUFSIZE];
  printf("Waiting for filename\n");
  char size_buffer[BUFSIZE];
  char buffer[BUFSIZE];
  struct Packet_Struct packet_info;
  struct Stop_and_Wait_Struct stop_and_wait_packet_info;
 
  while (1){
    
    n = recvfrom(sockfd, &packet_info, sizeof(struct Packet_Struct), 0, (struct sockaddr*)&addr, &addr_size);
    printf("Recieved: %s\n", packet_info.data);
  
    strcpy(filename, packet_info.data); 
    switch (packet_info.operation_type){
      case 0: //erver Operation for GET
        //calculate the total byte count
        FILE *file = fopen(packet_info.data, "rb");
        if (file == NULL) {
            printf("Error opening %s\n", filename);  
            break;
        }
        fseek(file, 0, SEEK_END);  
        long size = ftell(file);  
        fclose(file);
        snprintf(size_buffer, sizeof(size_buffer), "%ld", size);

        // Print the string
        printf("Long size as string: %s\n", size_buffer);

         
        memset(&packet_info, 0, sizeof(packet_info));

        packet_info.operation_type = GET;
        strcpy(packet_info.data, size_buffer);  

        //send the total byte count
        sendto(sockfd, &packet_info, sizeof(struct Packet_Struct), 0, (struct sockaddr*)&addr, sizeof(addr));
      

        //stop and wait setup, average round trip time is 1000 ms
      
        // char check_ack_sum[256]; //the ack id that the server sends back is stored here
        // struct timeval timeout;
        // timeout.tv_sec = 1;
        // timeout.tv_usec = 0;
        // setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));    

        int stop_and_wait_flag = 0;
        int id = 0; //the ack id for each packet


        FILE *fptr = fopen(filename, "rb");
        size_t num_bytes_read;
        // printf("sending bytes\n");
        while (num_bytes_read = fread(stop_and_wait_packet_info.data, 1, BUFSIZE, fptr)> 0){
          snprintf(stop_and_wait_packet_info.ack_id, sizeof(stop_and_wait_packet_info.ack_id), "%d", id);
          // printf("ID IS %s\n", stop_and_wait_packet_info.ack_id);
          // while (stop_and_wait_flag == 0){
            // gettimeofday(&start, NULL); 
            // send over the ack id and 1-24 bytes of data 
            sendto(sockfd, &stop_and_wait_packet_info, 1280, 0, (struct sockaddr*)&addr, sizeof(addr));
            
            // int n = recvfrom(sockfd, check_ack_sum, 256, 0, (struct sockaddr*)&addr, &addr_size);
          
            // gettimeofday(&end, NULL);
          
            // memset(&stop_and_wait_packet_info, 0, sizeof(struct Stop_and_Wait_Struct));

            //check if the ack id is the same. If n returns -1 then timeout
            // if (strcmp(check_ack_sum, stop_and_wait_packet_info.ack_id) && n >= 0) {
            //     printf("Received correct ACK\n");
            //     //exit while loop and send next flag
            //     stop_and_wait_flag = 1;
            // }
            // else{
            //   printf("Resending packet\n");
            // }       
          // }
          //reset flag and increment ack id
          // stop_and_wait_flag = 0;
          // id++;
        }
        fclose(fptr);
        printf("EXITING\n");
        // fclose(fptr);
        break;
      case 1: //server operation for PUT
        //convert the total byte received from the client to an int
        int byte_count;
        byte_count = atoi(packet_info.total_bytes); 
        int curr_byte_count = byte_count;  

        printf("Waiting for file contents\n");
         
      

        while(1){

          //retrieve the bytes from the file in 1024 packets and the ack num 
          int recv_bytes = recvfrom(sockfd, &stop_and_wait_packet_info, 1280, 0, (struct sockaddr*)&addr, &addr_size);   
          
          char file_name[BUFSIZE] = "Received_File_From_Client"; //store the file with this name

          FILE *output_file = fopen(strcat(file_name, packet_info.filetype), "ab"); //add the correct filetype to the end
         
          if (output_file) {
            if (curr_byte_count < BUFSIZE){
              //send the ack to the client
              size_t curr_byte_num = fwrite(stop_and_wait_packet_info.data, 1, curr_byte_count, output_file);
              sendto(sockfd, stop_and_wait_packet_info.ack_id, 256, 0, (struct sockaddr*)&addr, sizeof(addr));
              fclose(output_file);
              break;
            }
            else{
              size_t curr_byte_num = fwrite(stop_and_wait_packet_info.data, 1, 1024, output_file);
              sendto(sockfd, stop_and_wait_packet_info.ack_id, 256, 0, (struct sockaddr*)&addr, sizeof(addr));
              fclose(output_file);
              curr_byte_count -= 1024; //this is used to know when to exit this while loop
            }        
          } 
          memset(&stop_and_wait_packet_info, 0, sizeof(struct Stop_and_Wait_Struct));
        }
        printf("Done adding file\n");
        break;
      case 2: //server operation for DELETE
        printf("Deleting %s\n", filename);
        if (remove(filename) == 0) {
          printf("%s deleted successfully.\n", filename);
        } else {
            printf("File not found\n");
        }
     
        break;
      case 3:
        printf("printing contents of current directory\n");
        struct dirent *dirent_ptr;
        DIR *ptr = opendir(".");
        if (ptr) {
        while ((dirent_ptr = readdir(ptr)) != NULL) {
          
            size_t filename_size = strlen(dirent_ptr->d_name);
            sendto(sockfd, dirent_ptr->d_name, filename_size, 0, (struct sockaddr*)&addr, sizeof(addr));

          }
          closedir(ptr);
          const char *end_message = "END";
          sendto(sockfd, end_message, BUFSIZE, 0, (struct sockaddr*)&addr, sizeof(addr));
        }
        break;
      case 4:
        printf("exiting...\n");
        return;
      default:
        printf("Invalid option. Please try again.\n");
        break;
    }
    
  }
 

  printf("Done with operation\n");
}





int main(int argc, char **argv) {
  
  int sockfd; /* socket */
  int portno; /* port to listen on */
  int clientlen; /* byte size of client's address */
  struct sockaddr_in serveraddr; /* server's addr */
  struct sockaddr_in clientaddr; /* client addr */
  struct hostent *hostp; /* client host info */
  char buf[BUFSIZE]; /* message buf */
  char *hostaddrp; /* dotted decimal host addr string */
  int optval; /* flag value for setsockopt */
  int n; /* message byte size */

  /* 
   * check command line arguments 
   */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);
  // printf("portno =: %d\n", portno);
  /* 
   * socket: create the parent socket 
   */
  sockfd = socket(AF_INET, SOCK_DGRAM, 0); //add timeout here
  if (sockfd < 0) 
    error("ERROR opening socket");

  /* setsockopt: Handy debugging trick that lets 
   * us rerun the server immediately after we kill it; 
   * otherwise we have to wait about 20 secs. 
   * Eliminates "ERROR on binding: Address already in use" error. 
   *
  optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
	     (const void *)&optval , sizeof(int));

  /*
   * build the server's Internet address
   */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  // /* 
  //  * bind: associate the parent socket with a port 
  //  */
  if (bind(sockfd, (struct sockaddr *) &serveraddr, 
	   sizeof(serveraddr)) < 0) 
    error("ERROR on binding");

  printf("[STARTING] UDP File Server\n");

  
  server_operations(sockfd, clientaddr);
  

 
}


