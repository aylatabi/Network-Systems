
#include <regex.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>	//strlen
#include<sys/socket.h>
#include<arpa/inet.h>	//inet_addr
#include<unistd.h>	//write
#include<errno.h>
#include<pthread.h>
#include<stdbool.h>
#define BUFSIZE 1024
extern int errno;
void get_request_parameters(char *client_message, char *method, char *url, char *protocol){

    sscanf(client_message, "%s %s %s", method, url, protocol);
    char pattern[] = "^[A-Z]+ /([^ ]*) HTTP/1";
    regex_t regex;
    int reti;

 
    reti = regcomp(&regex, pattern, REG_EXTENDED);
            
    regmatch_t matches[1]; 
    reti = regexec(&regex, client_message, 1, matches, 0);   
    int start = matches[0].rm_so;
    int end = matches[0].rm_eo;
    char str[end - start + 1];
    memcpy(str, &client_message[start], end - start);
    // Null-terminate the string
    str[end - start] = '\0';
    method = strtok(str, " ");  // First part (GET)
    url = strtok(NULL, " ");    // Second part (/example.format)
    protocol = strtok(NULL, " "); // Third part (HTTP/1)

}

void get_status_code(char *error_handler, char *method, char *full_url_path, char *protocol){
    FILE *fh;
    
    fh = fopen(full_url_path, "r");
    if (strstr(full_url_path, ".") == NULL){
        strcpy(error_handler, "400 Bad Request");
        return;
    }
    else if (fh == NULL){
        
        if (errno == ENOENT){
           
            strcpy(error_handler, "404 Not Found"); //example: some_file.txt
            return;
        }
        else if (errno == EACCES) {
           
            strcpy(error_handler, "403 Forbidden"); //permissions.txt
            return;
        } 
        else {
            printf("null"); //should never get to this else statement
        }
    }
    else if ((strcmp(protocol, "HTTP/1.1") != 0) && (strcmp(protocol, "HTTP/1.0") != 0)){
        strcpy(error_handler, "505 HTTP Version Not Supported");
        return;
    }
    else if (strcmp(method, "GET") != 0){
        strcpy(error_handler, "405 Method Not Allowed");
        return;
    }
    else{
        strcpy(error_handler, "200 OK");
        return;
    }
   

}

void content_type_processing(int client_sock, char *error_handler, char *method, char *full_url_path, char *protocol){
    char *extension;
    extension = strrchr(full_url_path, '.');  
    printf("extension is %s\n", extension); 
    char header[256];
    char *buffer;
    if (extension == NULL){
        const char *type = "null";
        snprintf(header, sizeof(header), "%s %s\nContent-Type: %s\n\n", protocol, error_handler, type);
        write(client_sock, header, strlen(header));
    }
    else if ((strcmp(extension, ".html") == 0)){
        const char *type = "text/html";
        if (strcmp(error_handler, "200 OK") == 0){
            
            snprintf(header, sizeof(header), "%s 200 OK\nContent-Type: %s\n\n", protocol, type);
            long file_size;
            FILE *file = fopen(full_url_path, "r");
            if (file != NULL){
                fseek(file, 0, SEEK_END);
                file_size = ftell(file);
                printf("Content-Length: %ld\n", file_size);
                rewind(file);
                buffer = (char *)malloc(strlen(header) + file_size + 1);
                strcpy(buffer, header);
                fread(buffer + strlen(header), 1, file_size, file);
                buffer[strlen(header) + file_size] = '\0';
                printf("buffer size is %ld\n", strlen(buffer));
                write(client_sock, buffer, strlen(buffer));
            }
            fclose(file);
            free(buffer);
        }
        else{
            snprintf(header, sizeof(header), "%s %s\nContent-Type: %s\n\n", protocol, error_handler, type);
            write(client_sock, header, strlen(header));
        }
    }
    else if ((strcmp(extension, ".jpg") == 0)){
        const char *type = "image/jpg";
        if (strcmp(error_handler, "200 OK") == 0){
            char packet[BUFSIZE];
            size_t num_bytes_read;
            FILE *fptr = fopen(full_url_path, "rb");
            fseek(fptr, 0, SEEK_END);         
            long file_size = ftell(fptr);      
            fseek(fptr, 0, SEEK_SET);
            printf("Content-Length: %ld\n", file_size);
            snprintf(header, sizeof(header), "%s 200 OK\nContent-Type: %s\n\n", protocol, type);
            write(client_sock, header, strlen(header));
            while (num_bytes_read = fread(packet, 1, BUFSIZE, fptr)> 0){
                write(client_sock , packet , BUFSIZE); 
            } 
            fclose(fptr);
        }
        else{
            snprintf(header, sizeof(header), "%s %s\nContent-Type: %s\n\n", protocol, error_handler, type);
            write(client_sock, header, strlen(header));
        }
    }
    else if ((strcmp(extension, ".gif") == 0)){
        const char *type = "image/gif";
        if (strcmp(error_handler, "200 OK") == 0){
            char packet[BUFSIZE];
            size_t num_bytes_read;
            FILE *fptr = fopen(full_url_path, "rb");
            fseek(fptr, 0, SEEK_END);         
            long file_size = ftell(fptr);      
            fseek(fptr, 0, SEEK_SET);
            printf("Content-Length: %ld\n", file_size);
            snprintf(header, sizeof(header), "%s 200 OK\nContent-Type: %s\n\n", protocol, type);
            write(client_sock, header, strlen(header));
            while (num_bytes_read = fread(packet, 1, BUFSIZE, fptr)> 0){
                write(client_sock , packet , BUFSIZE); 
            } 
            fclose(fptr);
        }
        else{
            snprintf(header, sizeof(header), "%s %s\nContent-Type: %s\n\n", protocol, error_handler, type);
            write(client_sock, header, strlen(header));
        }
    }
    else if ((strcmp(extension, ".txt") == 0)){
        const char *type = "text/plain";
        if (strcmp(error_handler, "200 OK") == 0){
            snprintf(header, sizeof(header), "%s 200 OK\nContent-Type: %s\n\n", protocol, type);
            long file_size;
            FILE *file = fopen(full_url_path, "r");
            fseek(file, 0, SEEK_END);
            file_size = ftell(file);
            printf("Content-Length: %ld\n", file_size);
            rewind(file);
            buffer = (char *)malloc(strlen(header) + file_size + 1);
            strcpy(buffer, header);
            fread(buffer + strlen(header), 1, file_size, file);
            buffer[strlen(header) + file_size] = '\0';
            printf("buffer size is %ld\n", strlen(buffer));
            write(client_sock, buffer, strlen(buffer));
            fclose(file);
            free(buffer);
        }
        else{
            snprintf(header, sizeof(header), "%s %s\nContent-Type: %s\n\n", protocol, error_handler, type);
            write(client_sock, header, strlen(header));
        }
    }
    else if ((strcmp(extension, ".png") == 0)){
        const char *type = "image/png";
        if (strcmp(error_handler, "200 OK") == 0){
            char packet[BUFSIZE];
            size_t num_bytes_read;
            FILE *fptr = fopen(full_url_path, "rb");
            fseek(fptr, 0, SEEK_END);         
            long file_size = ftell(fptr);      
            fseek(fptr, 0, SEEK_SET);
            printf("Content-Length: %ld\n", file_size);
            snprintf(header, sizeof(header), "%s 200 OK\nContent-Type: %s\n\n", protocol, type);
            write(client_sock, header, strlen(header));
            while (num_bytes_read = fread(packet, 1, BUFSIZE, fptr)> 0){
                write(client_sock , packet , BUFSIZE); 
            } 
            fclose(fptr);
        }
        else{
            snprintf(header, sizeof(header), "%s %s\nContent-Type: %s\n\n", protocol, error_handler, type);
            write(client_sock, header, strlen(header));
        }
    }
    else if ((strcmp(extension, ".ico") == 0)){
        const char *type = "image/x-icon";
        if (strcmp(error_handler, "200 OK") == 0){
            char packet[BUFSIZE];
            size_t num_bytes_read;
            FILE *fptr = fopen(full_url_path, "rb");
            fseek(fptr, 0, SEEK_END);         
            long file_size = ftell(fptr);      
            fseek(fptr, 0, SEEK_SET);
            printf("Content-Length: %ld\n", file_size);
            snprintf(header, sizeof(header), "%s 200 OK\nContent-Type: %s\n\n", protocol, type);
            write(client_sock, header, strlen(header));
            while (num_bytes_read = fread(packet, 1, BUFSIZE, fptr)> 0){
                write(client_sock , packet , BUFSIZE); 
            } 
            fclose(fptr);
        }
        else{
            snprintf(header, sizeof(header), "%s %s\nContent-Type: %s\n\n", protocol, error_handler, type);
            write(client_sock, header, strlen(header));
        }
    }
    else if ((strcmp(extension, ".css") == 0)){
        const char *type = "text/css";
        if (strcmp(error_handler, "200 OK") == 0){
            snprintf(header, sizeof(header), "%s 200 OK\nContent-Type: %s\n\n", protocol, type);
            long file_size;
            FILE *file = fopen(full_url_path, "r");
            fseek(file, 0, SEEK_END);
            file_size = ftell(file);
            printf("Content-Length: %ld\n", file_size);
            rewind(file);
            buffer = (char *)malloc(strlen(header) + file_size + 1);
            strcpy(buffer, header);
            fread(buffer + strlen(header), 1, file_size, file);
            buffer[strlen(header) + file_size] = '\0';
            printf("buffer size is %ld\n", strlen(buffer));
            write(client_sock, buffer, strlen(buffer));
            fclose(file);
            free(buffer);
        }
        else{
            snprintf(header, sizeof(header), "%s %s\nContent-Type: %s\n\n", protocol, error_handler, type);
            write(client_sock, header, strlen(header));
        }
    }
    else if ((strcmp(extension, ".js") == 0)){
        const char *type = "application/javascript";
        if (strcmp(error_handler, "200 OK") == 0){
            snprintf(header, sizeof(header), "%s 200 OK\nContent-Type: %s\n\n", protocol, type);
            long file_size;
            FILE *file = fopen(full_url_path, "r");
            fseek(file, 0, SEEK_END);
            file_size = ftell(file);
            printf("Content-Length: %ld\n", file_size);
            rewind(file);
            buffer = (char *)malloc(strlen(header) + file_size + 1);
            strcpy(buffer, header);
            fread(buffer + strlen(header), 1, file_size, file);
            buffer[strlen(header) + file_size] = '\0';
            printf("buffer size is %ld\n", strlen(buffer));
            write(client_sock, buffer, strlen(buffer));
            fclose(file);
            free(buffer);
        }
        else{
            snprintf(header, sizeof(header), "%s %s\nContent-Type: %s\n\n", protocol, error_handler, type);
            write(client_sock, header, strlen(header));
        }
    }

}

void* handle_connection(void* p_client_sock){
    int client_sock = *((int*)p_client_sock);
    free(p_client_sock);
    /*
    Get Message from Client
    */
    int read_size;
    char client_message[2000];
    char header[256];
    read_size = recv(client_sock , client_message , 1024 , 0);
    printf("Message is %s", client_message);

    /*
    Parse Message to get method, url and protocol-
    */
    char *method = malloc(256 * sizeof(char));
    char *url = malloc(256 * sizeof(char));
    char *protocol = malloc(256 * sizeof(char));

    /*
    for i in {1..1}; do echo -e "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n" | nc localhost 5001 & done
    wait

    echo -e "GET /sorta_large_file HTTP/1.9\r\nHost: localhost\r\n\r\n" | nc localhost 5001

    */
    get_request_parameters(client_message, method, url, protocol);

    printf("method is %s\n", method);
    printf("url is %s\n", url);
    printf("protocol is %s\n", protocol);
    
    char *prefix;
    char *extension;
    extension = strrchr(url, '.'); 
    FILE *fh;
    FILE *fh1;

    if ((extension != NULL) && ((strcmp(extension, ".png") == 0) || (strcmp(extension, ".gif") == 0) || (strcmp(extension, ".jpg") == 0))){
        if (strstr(url, "/images") != NULL){
            prefix = malloc(strlen("www") + 1);
            strcpy(prefix, "www");
        }
        else{

          
            prefix = malloc(strlen("www/images")+1);
            strcpy(prefix, "www/images");
            size_t size2 = strlen(url);         
            size_t size3 = strlen(prefix);     
            char test_url_path[size2 + size3 + 1]; 
            strcpy(test_url_path, prefix);
            strcat(test_url_path, url);
            fh = fopen(test_url_path, "r");
            if (fh == NULL){
                free(prefix);
                prefix = malloc(strlen("www/graphics")+1);
                strcpy(prefix, "www/graphics");
            
                size_t size4 = strlen(url);         
                size_t size5 = strlen(prefix);     
                char test_url_path2[size4 + size5 + 1]; 
                strcpy(test_url_path2, prefix);
                strcat(test_url_path2, url);
                fh1 = fopen(test_url_path2, "r");
                if (fh1 == NULL){
                    free(prefix);
                    prefix = malloc(strlen("www/fancybox")+1);
                    strcpy(prefix, "www/fancybox");
                    // fclose(fh1);
                }
                
                // fclose(fh);
            }

        
        
        }
    }
    else if ((extension != NULL) && (strcmp(extension, ".css") == 0)){
        if ((strstr(url, "/css") != NULL) || (strstr(url, "/fancybox") != NULL)){
            prefix = malloc(strlen("www") + 1);
            strcpy(prefix, "www");
        }
       
    }
    else{
        prefix = malloc(strlen("www")+1);
        strcpy(prefix, "www");
    }


    // char prefix[] = "www";
    size_t size = strlen(url);         
    size_t size1 = strlen(prefix);     
    char full_url_path[size + size1 + 1]; 
    strcpy(full_url_path, prefix);
    strcat(full_url_path, url);

    printf("full url path is %s\n", full_url_path);

    /*
    Error handler
    */
    char error_handler[40];
    get_status_code(error_handler, method, full_url_path, protocol);

    printf("error handler is %s\n", error_handler);

    pthread_t thread_id = pthread_self();
    printf("Handling connection in thread %lu\n", (unsigned long)thread_id);

    content_type_processing(client_sock, error_handler, method, full_url_path, protocol);

   
    close(client_sock);
    free(prefix);
  
    printf("closing connection with %lu\n", (unsigned long)thread_id);
}
int main(int argc , char *argv[])
{  
	int socket_desc , client_sock , c;
	struct sockaddr_in server , client;
	
	
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");
	int opt = 1;
    if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        return 1;
    }
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 5001 );
	
	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("bind done");
	
	//Listen
	listen(socket_desc , 100);
	
    while(true){
        printf("Waiting for Connections...\n");
        c = sizeof(struct sockaddr_in);
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0)
        {
            perror("accept failed");
            return 1;
        }
        printf("Connected\n");
        // handle_connection(client_sock);
        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;
        pthread_t t;
        pthread_create(&t, NULL, handle_connection, pclient);

    }


	

	
	return 0;
}