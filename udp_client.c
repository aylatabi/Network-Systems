/* 
 * udpclient.c - A simple UDP client
 * usage: udpclient <host> <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <sys/time.h>


#define BUFSIZE 1024
#define TIMEOUT 5


/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(0);
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

void send_file_info_for_put(char* filename, char* filetype,  int sockfd, struct sockaddr_in addr){

    char size_buffer[BUFSIZE]; //string buffer for calculating the size of the whole file
    
    socklen_t addr_size;
    addr_size = sizeof(addr);

    struct Packet_Struct packet_info;
    struct Stop_and_Wait_Struct stop_and_wait_packet_info;

    //stop and wait setup, average round trip time is 1000 ms
   
    char check_ack_sum[256]; //the ack id that the server sends back is stored here
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    //Open the file and calculate the total byte size
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error opening %s\n", filename);
        
    }
    fseek(file, 0, SEEK_END);  
    long size = ftell(file);  
    fclose(file);
    snprintf(size_buffer, sizeof(size_buffer), "%ld", size);

    printf("Long size as string: %s\n", size_buffer);
    
    //send the server the filename, state, filetype, and the total size of the file
    
    strcpy(packet_info.data, filename); 
    packet_info.operation_type = PUT;
    strcpy(packet_info.filetype, filetype);   
    strcpy(packet_info.total_bytes, size_buffer);
    sendto(sockfd, &packet_info, sizeof(struct Packet_Struct), 0, (struct sockaddr*)&addr, sizeof(addr));
      
    
    FILE *fptr = fopen(filename, "rb");
    size_t num_bytes_read;
    int stop_and_wait_flag = 0;
    int id = 0; //the ack id for each packet
    // struct timeval start, end; //struct for calculating round trip time

    //while loop for reading 1024 bytes from the file
    while (num_bytes_read = fread(stop_and_wait_packet_info.data, 1, BUFSIZE, fptr)> 0){

      snprintf(stop_and_wait_packet_info.ack_id, sizeof(stop_and_wait_packet_info.ack_id), "%d", id);
      
     
      while (stop_and_wait_flag == 0){
        // gettimeofday(&start, NULL); 
        // send over the ack id and 1-24 bytes of data 
        sendto(sockfd, &stop_and_wait_packet_info, 1280, 0, (struct sockaddr*)&addr, sizeof(addr));
        
        int n = recvfrom(sockfd, check_ack_sum, 256, 0, (struct sockaddr*)&addr, &addr_size);
       
        // gettimeofday(&end, NULL);
      
        memset(&stop_and_wait_packet_info, 0, sizeof(struct Stop_and_Wait_Struct));

        //check if the ack id is the same. If n returns -1 then timeout
        if (strcmp(check_ack_sum, stop_and_wait_packet_info.ack_id) && n >= 0) {
            printf("Received correct ACK\n");
            //exit while loop and send next flag
            stop_and_wait_flag = 1;
        }
        else{
          printf("Resending packet\n");
        }       
      }
      //reset flag and increment ack id
      stop_and_wait_flag = 0;
      id++;
    }
    fclose(fptr);
}

void send_filename(char* filename, int sockfd, struct sockaddr_in addr){
  int n;
  struct Packet_Struct packet_info;
  char buffer[BUFSIZE];
  strcpy(packet_info.data, filename);  
  printf("Sending Filename%s\n", packet_info.data);
  packet_info.operation_type = GET;
  sendto(sockfd, &packet_info, sizeof(struct Packet_Struct), 0, (struct sockaddr*)&addr, sizeof(addr));
}

int get_filename_total_byte_count(int sockfd, struct sockaddr_in addr){
    int n;
    socklen_t addr_size;
    addr_size = sizeof(addr);
    printf("Waiting for byte count\n");
    struct Packet_Struct packet_info;
    while(1){
      n = recvfrom(sockfd, &packet_info, sizeof(struct Packet_Struct), 0, (struct sockaddr*)&addr, &addr_size);
      printf("Total bytes...%s\n", packet_info.data);
      break;
    }
    int byte_count;
    byte_count = atoi(packet_info.data);
    return byte_count;
}

void delete_file(char* filename, int sockfd, struct sockaddr_in addr){
  int n;
  struct Packet_Struct packet_info;
  strcpy(packet_info.data, filename);  
  printf("Sending %s\n", packet_info.data);
  packet_info.operation_type = DELETE_PACKET;
  sendto(sockfd, &packet_info, sizeof(struct Packet_Struct), 0, (struct sockaddr*)&addr, sizeof(addr));


}

void exit_func(int sockfd, struct sockaddr_in addr){
  int n;
  struct Packet_Struct packet_info;
  strcpy(packet_info.data, "exit");
  
  packet_info.operation_type = EXIT;
  sendto(sockfd, &packet_info, sizeof(struct Packet_Struct), 0, (struct sockaddr*)&addr, sizeof(addr));

}
void print_file_names(int sockfd, struct sockaddr_in addr){
  int n;
  char buffer[BUFSIZE];
  struct Packet_Struct packet_info;
  socklen_t addr_size;
  addr_size = sizeof(addr);
  strcpy(packet_info.data, "List current directory");
  packet_info.operation_type = SEARCH;
  sendto(sockfd, &packet_info, sizeof(struct Packet_Struct), 0, (struct sockaddr*)&addr, sizeof(addr));

  while(1){
    int n = recvfrom(sockfd, buffer, BUFSIZE, 0, (struct sockaddr*)&addr, &addr_size);
    if (strcmp(buffer, "END") == 0){
      printf("Done receiving filenames\n");
      return;
    }
    buffer[n] = '\0'; 
    printf("Received filename: %s\n",buffer);
  }
}
void receive_file(int byte_count, char* file_type, int sockfd, struct sockaddr_in addr){
    int n;
    printf("byte count is %d\n", byte_count);
    socklen_t addr_size;
    addr_size = sizeof(addr);
    printf("FILE TYPE %s\n", file_type);
    // char* buffer[BUFSIZE];
    printf("Waiting for file contents\n");
    struct Stop_and_Wait_Struct stop_and_wait_packet_info; 
    int curr_byte_count = byte_count;
     while(1){

    //       //retrieve the bytes from the file in 1024 packets and the ack num 
      int recv_bytes = recvfrom(sockfd, &stop_and_wait_packet_info, 1280, 0, (struct sockaddr*)&addr, &addr_size);   
      
      char file_name[BUFSIZE] = "Received_File_From_Server"; //store the file with this name

      FILE *output_file = fopen(strcat(file_name, file_type), "ab"); //add the correct filetype to the end
      
      if (output_file) {
        if (curr_byte_count < BUFSIZE){
          //send the ack to the client
          size_t curr_byte_num = fwrite(stop_and_wait_packet_info.data, 1, curr_byte_count, output_file);
          // sendto(sockfd, stop_and_wait_packet_info.ack_id, 256, 0, (struct sockaddr*)&addr, sizeof(addr));
          fclose(output_file);
          break;
        }
        else{
          size_t curr_byte_num = fwrite(stop_and_wait_packet_info.data, 1, 1024, output_file);
          // sendto(sockfd, stop_and_wait_packet_info.ack_id, 256, 0, (struct sockaddr*)&addr, sizeof(addr));
          fclose(output_file);
          curr_byte_count -= 1024; //this is used to know when to exit this while loop
        }        
      } 
      memset(&stop_and_wait_packet_info, 0, 1280);
    }

    printf("Client done\n");
    return;
}


int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    char buf[BUFSIZE];

    /* check command line arguments */
    if (argc != 3) {
       fprintf(stderr,"usage: %s <hostname> <port>\n", argv[0]);
       exit(0);
    }

    hostname = argv[1];
    portno = atoi(argv[2]); //port number
  
    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

     /* get a message from the user */
    int flag = 0;
    while (flag == 0){
    bzero(buf, BUFSIZE);
    printf("Type any of the following commands:\n Type in filetype as .txt, .jpg, etc...seperate comma and no space\nget[file_name,file_type]\nput[file_name,file_type]\ndelete[file_name,file_type]\nls\nexit\n");
    fgets(buf, BUFSIZE, stdin);
    printf("message is :  %s\n", buf);
    
   
      if (strstr(buf, "get") != NULL){
            
            char output[BUFSIZE];

            const char *start = strstr(buf, "get[");
            start += 4;
            const char *end = strchr(start, ']');
            size_t length = end - start;
            strncpy(output, start, length);
            output[length] = '\0';
            char *file_name = strtok(output, ",");
            char *file_type = strtok(NULL, ",");

            printf("Requesting %s from the server...\n", output);
           
            send_filename(file_name, sockfd, serveraddr);
            int byte_count;
            byte_count = get_filename_total_byte_count(sockfd, serveraddr);
            receive_file(byte_count, file_type, sockfd, serveraddr);
            
          }
          else if (strstr(buf, "put") != NULL){
            char output[BUFSIZE];

            const char *start = strstr(buf, "put[");
            start += 4;
            const char *end = strchr(start, ']');
            size_t length = end - start;
            strncpy(output, start, length);
            output[length] = '\0';
            char *file_name = strtok(output, ",");
            char *file_type = strtok(NULL, ",");  
            send_file_info_for_put(file_name, file_type, sockfd, serveraddr);

          }
          else if (strstr(buf, "delete") != NULL){
            char output[BUFSIZE];
            const char *start = strstr(buf, "delete[");
            start += 7;
            const char *end = strchr(start, ']');
            size_t length = end - start;
            strncpy(output, start, length);
            output[length] = '\0';
           

            printf("Requesting %s from the server...\n", output);
            char *type = "delete";

            delete_file(output, sockfd, serveraddr);
          }
          else if (strstr(buf, "ls") != NULL){
            print_file_names(sockfd, serveraddr);
          }
          else if (strstr(buf, "exit") != NULL){
            exit_func(sockfd, serveraddr);
            flag = 1;
          }
          else{
            printf("Command not Recognized\n");
          }
    }
   
    return 0;
}


