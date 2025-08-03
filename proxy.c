
/**
 * proxy.c - Simple HTTP proxy server
 *
 * This program implements a basic HTTP proxy server that forwards client
 * requests to servers and returns the responses. It handles multiple clients
 * using select()/poll() or multithreading.
 *
 * Features:
 *  - Parses HTTP GET requests
 *  - Forwards requests to target web servers
 *  - Returns the server's response to the client
 *
 * Technologies:
 *  - POSIX sockets (AF_INET, SOCK_STREAM)
 *  - Select-based I/O multiplexing (or pthreads, if used)
 *
 * Author: Ayla Tabi
 * Date: August 2025
 */

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
#include <netdb.h>  // Add this header
#include <string.h>
#include <time.h>
#include <openssl/evp.h>
#define BUFSIZE 1024
extern int errno;
char blocklist[2][50];
int capacity = 10;
int array_size = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct {
    char md5hash[33];   // MD5 hashes are 32 characters long + 1 for null terminator
    char filename[100];
    char timestamp[100];
} cache;
cache *cache_array;


/**
 * Parses an HTTP request message to extract method, URL, protocol, and hostname.
 *
 * This function uses regex to parse the request line and headers:
 *  - Extracts the HTTP method, URL, and protocol version from the request line.
 *  - Extracts the "Host" header value for the hostname.
 *
 * @param client_message The full HTTP request message as a string.
 * @param method Buffer to store the HTTP method (e.g., "GET").
 * @param url Buffer to store the URL path (e.g., "/index.html").
 * @param protocol Buffer to store the protocol (e.g., "HTTP/1.1").
 * @param hostname Buffer to store the hostname extracted from "Host" header.
 */

void get_request_parameters(char *client_message, char *method, char *url, char *protocol, char *hostname){

    sscanf(client_message, "%s %s %s %s", method, url, protocol, hostname);
    char pattern[] = "^[A-Z]+ /([^ ]*) HTTP/1";
    regex_t regex;
    int reti;

 
    reti = regcomp(&regex, pattern, REG_EXTENDED);
            
    regmatch_t matches[2]; 
    reti = regexec(&regex, client_message, 1, matches, 0);   
    int start = matches[0].rm_so;
    int end = matches[0].rm_eo;
    char str[end - start + 1];
    memcpy(str, &client_message[start], end - start);
    str[end - start] = '\0';
    method = strtok(str, " ");  // First part (GET)
    url = strtok(NULL, " ");    // Second part (/example.format)
    protocol = strtok(NULL, " "); // Third part (HTTP/1)
    regfree(&regex);
    memset(matches, 0, sizeof(matches));
   
    char pattern1[] = "Host:\\s*([a-zA-Z0-9.-]+)";
    reti = regcomp(&regex, pattern1, REG_EXTENDED);
    if (reti) {
        printf("Could not compile regex\n");
        return;
    }
    memset(matches, 0, sizeof(matches));
    reti = regexec(&regex, client_message, 2, matches, 0);
    if (reti == 0)
    {
        int start1 = matches[1].rm_so;
        int end1 = matches[1].rm_eo;
        char str1[end1 - start1 + 1]; 
        strncpy(str1, &client_message[start1], end1 - start1);
        str1[end1 - start1] = '\0';
        strcpy(hostname, str1);

    }
    regfree(&regex);
 
}

/**
 * Sends the appropriate HTTP Content-Type header to the client based on the file extension.
 *
 *
 * @param client_sock The client socket file descriptor.
 * @param method The HTTP method (e.g., "GET").
 * @param full_url_path The full URL path requested by the client.
 * @param protocol The HTTP protocol version (e.g., "HTTP/1.1").
 */
void content_type_processing(int client_sock, char *method, char *full_url_path, char *protocol){
    char *extension;
    char header[256];
    extension = strrchr(full_url_path, '.');  
    printf("extension is %s\n", extension); 
    if ((strcmp(extension, ".gif") == 0)){
        const char *type = "image/gif";
        snprintf(header, sizeof(header), "%s 200 OK\nContent-Type: %s\n\n", protocol, type);
        write(client_sock, header, strlen(header));
    }
}

/**
 * Searches the cache array for an entry matching the given MD5 hash.
 *
 * @param hash_output The MD5 hash string to search for.
 * @return The index of the cache entry if found; otherwise, -1.
 */
int find_url(char *hash_output){
    for (int i = 0; i < array_size; i++) {
        if (strcmp(cache_array[i].md5hash, hash_output) == 0) {
            return i; //return index
        }
    }
    return -1;
}

/**
 * Computes the MD5 hash of the given input string.
 *
 * Uses OpenSSL's EVP (Envelope) library functions to compute the MD5 digest.
 * The resulting hash is returned as a null-terminated hexadecimal string.
 *
 * @param input The input string to hash.
 * @param hash_output Buffer to store the resulting MD5 hash as a hex string.
 *                    Must be at least 33 bytes long to hold 32 hex chars + null terminator.
 *
 */
void compute_md5(const char *input, char *hash_output) {
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_lengh = 0;

    EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
    EVP_DigestUpdate(ctx, input, strlen(input));
    EVP_DigestFinal_ex(ctx, hash, &hash_lengh);
    EVP_MD_CTX_free(ctx);
    for (unsigned int i = 0; i < hash_lengh; i++) {
        sprintf(&hash_output[i * 2], "%02x", hash[i]);
    }
    hash_output[hash_lengh * 2] = '\0'; 
}

/**
 * Sends an HTTP request to the destination server, utilizing a timed cache.
 *
 * This function implements a caching proxy behavior:
 *  - Computes an MD5 hash of the requested URL to uniquely identify cache entries.
 *  - Checks if the requested resource’s file extension is cacheable.
 *  - If the resource is cacheable, looks up the cache metadata array for a cached
 *    copy and checks if it has expired (based on a 15-second freshness threshold).
 *  - If a fresh cached copy exists, serves it directly to the client.
 *  - Otherwise, connects to the destination server, forwards the request,
 *    receives the response, saves it in the cache, and then serves it to the client.
 *
 * The function also handles DNS resolution, socket creation, and file I/O for caching.
 *
 * @param client_sock The client socket file descriptor to send the response to.
 * @param bytesRead Number of bytes read from the client request buffer.
 * @param buffer The HTTP request data received from the client.
 * @param hostname The hostname of the destination server.
 * @param portnum The port number of the destination server (default 80).
 * @param method HTTP method string (e.g., "GET").
 * @param url The requested URL.
 * @param protocol The HTTP protocol version (e.g., "HTTP/1.1").
 */
void send_message_to_server(int client_sock, ssize_t bytesRead, char *buffer, char *hostname, int portnum, char *method, char *url, char *protocol){
    char header[256];
   
    int in_cache_flag = 0;
    char hash_output[33]; 
   
    compute_md5(url, hash_output);

    
    printf("MD5 Hash: %s\n", hash_output);

    time_t time_now;
    struct tm *time_info;
    char curr_timestamp[100];
 
    time(&time_now);
    time_info = localtime(&time_now); 
    strftime(curr_timestamp, sizeof(curr_timestamp), "%H-%M-%S", time_info);
 
    char *extension;
   
    extension = strrchr(url, '.');  
    const char *acceptable_extensions[] = {".html", ".css", ".js", ".png", ".jpg", ".jpeg", ".gif"};
    size_t num_ext = sizeof(acceptable_extensions) / sizeof(acceptable_extensions[0]);

    // Check if extension matches one of the valid ones
    int is_valid = 0;
    for (size_t i = 0; i < num_ext; ++i) {
        if (strcmp(extension, acceptable_extensions[i]) == 0) {
            is_valid = 1;
            break;
        }
    }

    if (!is_valid) {
        printf("Invalid extension: %s\n", extension);
        strcpy(extension, ".html");
    } else {
        printf("Valid extension: %s\n", extension);
    }

    char filename[100];
    
    int index = find_url(hash_output);     
    if (index == -1){
        pthread_mutex_lock(&mutex);
        cache newFile1;
        printf("INDEX NOT FOUND\n");
        char *part_of_url = strrchr(url, '/'); // Find the last '/' character
        
        if (part_of_url) {
            strcpy(filename, part_of_url + 1);
        }
        strncpy(newFile1.md5hash, hash_output, sizeof(newFile1.md5hash) - 1);
        newFile1.md5hash[sizeof(newFile1.md5hash) - 1] = '\0'; // Ensure null termination

        strncpy(newFile1.filename, filename, sizeof(newFile1.filename) - 1);
        newFile1.filename[sizeof(newFile1.filename) - 1] = '\0'; // Ensure null termination

        strncpy(newFile1.timestamp, curr_timestamp, sizeof(newFile1.timestamp) - 1);
        newFile1.timestamp[sizeof(newFile1.timestamp) - 1] = '\0';
        cache_array[array_size] = newFile1;
        (array_size)++;
        pthread_mutex_unlock(&mutex);

       

    }
    else{
        printf("IN CACHE\n");
      
        char *cached_timestamp = cache_array[index].timestamp;
       
        int curr_hours, curr_minutes, curr_seconds;
        sscanf(curr_timestamp, "%d-%d-%d", &curr_hours, &curr_minutes, &curr_seconds);

        int cache_hours, cache_minutes, cache_seconds;
        sscanf(cached_timestamp, "%d-%d-%d", &cache_hours, &cache_minutes, &cache_seconds);

        int total_curr_seconds = curr_hours * 3600 + curr_minutes * 60 + curr_seconds;
        int total_cached_seconds = cache_hours * 3600 + cache_minutes * 60 + cache_seconds;

      
        int diff_seconds = total_curr_seconds - total_cached_seconds;

       
        printf("Difference: %d seconds\n", diff_seconds);

        if (diff_seconds > 15){
            pthread_mutex_lock(&mutex);
            cache_array[array_size] = (cache){0};
            cache newFile1;
            printf("replaceing filename\n");
            char *part_of_url = strrchr(url, '/'); 
        
            if (part_of_url) {
                strcpy(filename, part_of_url + 1);
            }
            strncpy(newFile1.md5hash, hash_output, sizeof(newFile1.md5hash) - 1);
            newFile1.md5hash[sizeof(newFile1.md5hash) - 1] = '\0'; 

            strncpy(newFile1.filename, filename, sizeof(newFile1.filename) - 1);
            newFile1.filename[sizeof(newFile1.filename) - 1] = '\0'; 

            strncpy(newFile1.timestamp, curr_timestamp, sizeof(newFile1.timestamp) - 1);
            newFile1.timestamp[sizeof(newFile1.timestamp) - 1] = '\0';
            cache_array[array_size] = newFile1;
            (array_size)++;
            
            pthread_mutex_unlock(&mutex);
        }
        else{
            in_cache_flag = 1;
            printf("in cache\n");
            strcpy(filename, cache_array[index].filename);
        }

    }

    if (in_cache_flag == 0){
        printf("NOT IN CACHE REQUEST FROM SERVER\n");
        struct sockaddr_in target_addr;
    
        memset(&target_addr, 0, sizeof(target_addr));

       
        target_addr.sin_family = AF_INET;
        target_addr.sin_port = htons(portnum);

       
        struct hostent *host = gethostbyname(hostname);
        if (host == NULL) {
            fprintf(stderr, "Error: Unable to resolve hostname %s\n", hostname);
            exit(1);
        }

        
        bcopy((char *)host->h_addr, (char *)&target_addr.sin_addr.s_addr, host->h_length);
    
        int targetsock = socket(AF_INET, SOCK_STREAM, 0);
        if (targetsock == -1)
        {
            printf("Target socket error");
        }
        if (connect(targetsock, (struct sockaddr *)&target_addr, sizeof(target_addr)) < 0) {
            perror("Connect error\n");
            return;
        }
        write(targetsock, buffer, strlen(buffer)); 
        memset(buffer, 0, sizeof(buffer));
        FILE *file = fopen(filename, "wb");
        if (!file) {
            perror("Failed to open file for writing");
            return;
        }
    
        
        while((bytesRead = recv(targetsock, buffer, sizeof(buffer), 0)) > 0){
            
            ssize_t bytesWrittenFile = fwrite(buffer, 1, bytesRead, file);
            if (bytesWrittenFile != bytesRead) {
                if (ferror(file)) {
                    perror("write failed");
                    fclose(file);
                } 
            }
            if (memset(buffer, 0, sizeof(buffer)) == NULL) {
                perror("memset failed");
                exit(EXIT_FAILURE);
            }
        }
        printf("read all bytes\n");
        fclose(file);
        close(targetsock);
    }
    
    // printf("Generated filename: %s\n", filename);
   
    

    FILE *fptr = fopen(filename, "rb");
    if (!fptr) {
        perror("can't open file");
        return;
    }

    size_t num_bytes_read;
   
    while ((num_bytes_read = fread(buffer, 1, sizeof(buffer), fptr)) > 0){
                
        ssize_t bytesWritten = write(client_sock, buffer, num_bytes_read);
        if (bytesWritten == -1) {
            perror("write failed");
            exit(EXIT_FAILURE);
        }
       
    } 
    fclose(fptr);
    
   
}


/**
 * Checks whether the given URL is blocklisted.
 *
 * If the URL matches an entry in the "blocklist" file, a 403 Forbidden response
 * is sent to the client and the function returns 1.
 *
 * @param client_sock The socket file descriptor for the client.
 * @param url The URL being requested.
 * @param protocol The HTTP protocol version (e.g., "HTTP/1.1").
 * @return 1 if the URL is blocklisted, 0 if not, -1 on error.
 */
int handle_blocklist(int client_sock, char *url, char *protocol){
    char header[256];
    char body[512];
    FILE *fptr = fopen("blocklist", "rb");
    if (!fptr) {
        perror("Failed to open file for writing");
        return -1;
    }
    char line[100];
    while (fgets(line, sizeof(line), fptr) != NULL) {  // Read each line in blocklist
     
        line[strcspn(line, "\n")] = '\0';

     
        if (strcmp(url, line) == 0) { //if url is in blocklist then send over this instead
            printf("!!This website is blocked: %s\n", line);
            snprintf(body, sizeof(body),
                    "<html>"
                    "<head><title>403 Forbidden</title></head>"
                    "</html>");

            
            snprintf(header, sizeof(header),
                    "%s 403 Forbidden\r\n"
                    "Content-Type: text/html\r\n"
                    "Content-Length: %zu\r\n"
                    "Connection: close\r\n"
                    "\r\n",
                    protocol, strlen(body));

           
            write(client_sock, header, strlen(header));
            write(client_sock, body, strlen(body));
            return 1;
        } 
    }

    fclose(fptr);
    
    return 0;
}

/**
 * Handles a single client connection.
 *
 * Reads the HTTP request from the client, parses the method, URL, and protocol.
 * If the request is a GET, it checks whether the URL is blocklisted.
 * If allowed, it forwards the request to the destination server and sends back
 * the server's response to the client.
 *
 * @param pclient Pointer to a dynamically allocated client socket file descriptor.
 * @return NULL
 */

void* handle_connection(void* p_client_sock){
    
    int client_sock = *((int*)p_client_sock);
    free(p_client_sock);

    char buffer[10000]; 
    memset(buffer, 0, sizeof(buffer));

    // Read data from client socket
    ssize_t bytesRead = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead < 0) { 
        return NULL;
    }

    //If there is data to be read
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        
        char *method = malloc(256 * sizeof(char));
        char *url = malloc(256 * sizeof(char));
        char *protocol = malloc(256 * sizeof(char));
        char *hostname = malloc(256 * sizeof(char));
        get_request_parameters(buffer, method, url, protocol, hostname);
        url[strcspn(url, "\r\n")] = '\0';
        if (strcmp(method, "GET") == 0){
           
            printf("Request:\n%s\n", buffer);
            printf("method is %s\n", method);
            printf("url is %s\n", url);
            printf("protocol is %s\n", protocol);
            printf("Hostname is %s\n", hostname);
            int blocked = handle_blocklist(client_sock, url, protocol);
            if (blocked == 1){
                return NULL; //Exit this function
            }
            char* port = strrchr(url,':'); //set a default port
            int portnum = 0;
            if (port){
                portnum = strtol(port+1, NULL,10);
            }
            if (portnum == 0){
                portnum = 80;
            }
            printf("portnum is %d\n", portnum);
            send_message_to_server(client_sock, bytesRead, buffer, hostname, portnum, method, url, protocol);
            printf("exited send message to server\n");
            free(method);
            free(url);
            free(protocol);
            free(hostname);
        }
       
       
    } else if (bytesRead == 0) {
        printf("Client disconnected.\n");
    }
  
  
    close(client_sock);
    return NULL;
   
}
//  http://localhost:8888/index.html
/**
 * Main server loop: accepts incoming client connections and handles each
 * connection in a new thread. Uses pthreads for concurrency.
 *
 * For each accepted connection:
 *  - Allocates memory to store the client socket descriptor
 *  - Creates a new thread that runs handle_connection()
 *  - Detaches the thread so system resources are automatically cleaned up
 */
int main(int argc , char *argv[]){

    
    //Create cache array
    cache_array = malloc(capacity * sizeof(cache));
    if (!cache_array) {
        perror("Failed to allocate memory");
        return 1;
    }

    //Create socket
    int proxy_sock , client_sock , c;
	struct sockaddr_in sockaddr_proxy , sockaddr_client;	
	
	proxy_sock = socket(AF_INET , SOCK_STREAM , 0);
	if (proxy_sock == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");
	int opt = 1;
    if (setsockopt(proxy_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        return 1;
    }
	
    memset(&sockaddr_proxy, 0, sizeof(sockaddr_proxy));
	sockaddr_proxy.sin_family = AF_INET;
	sockaddr_proxy.sin_addr.s_addr = INADDR_ANY; 
	sockaddr_proxy.sin_port = htons( 8888 );
	
	
	if(bind(proxy_sock,(struct sockaddr *)&sockaddr_proxy , sizeof(sockaddr_proxy)) < 0)
	{
		
		perror("Bind failed");
		return 1;
	}
	
	listen(proxy_sock , 100);

    while(true){
        
        c = sizeof(struct sockaddr_in);
        client_sock = accept(proxy_sock, (struct sockaddr *)&sockaddr_client, (socklen_t*)&c);
        if (client_sock < 0)
        {
            perror("Accept failed");
            return 1;
        }
        //multithreaded
        int *pclient = malloc(sizeof(int));
        *pclient = client_sock;
        pthread_t t;
        pthread_create(&t, NULL, handle_connection, pclient);
        
    }
    // close(proxy_sock);
    return 0;
}