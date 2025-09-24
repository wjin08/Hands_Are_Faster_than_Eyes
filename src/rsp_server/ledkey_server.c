#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 5000
#define DEVICE_FILENAME "/dev/ledkey"
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

int dev_fd;
int client_fds[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// 클라이언트 추가
void add_client(int fd)
{
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(client_fds[i] == 0)
        {
            client_fds[i] = fd;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// 클라이언트 제거
void remove_client(int fd)
{
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(client_fds[i] == fd)
        {
            client_fds[i] = 0;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// 모든 클라이언트에게 브로드캐스트
void broadcast_to_all(char *message, int sender_fd)
{
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(client_fds[i] != 0 && client_fds[i] != sender_fd)
        {
            write(client_fds[i], message, strlen(message));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// 모든 클라이언트에게 전송 (발신자 포함)
void send_to_all(char *message)
{
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(client_fds[i] != 0)
        {
            write(client_fds[i], message, strlen(message));
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

unsigned char value_to_led_pattern(unsigned char value)
{
    int led_count;
    
    if (value == 0) {
        led_count = 0;
    } else if (value <= 31) {
        led_count = 1;
    } else if (value <= 63) {
        led_count = 2;
    } else if (value <= 95) {
        led_count = 3;
    } else if (value <= 127) {
        led_count = 4;
    } else if (value <= 159) {
        led_count = 5;
    } else if (value <= 191) {
        led_count = 6;
    } else if (value <= 223) {
        led_count = 7;
    } else {
        led_count = 8;
    }
    
    unsigned char pattern = 0;
    for (int i = 0; i < led_count; i++)
    {
        pattern |= (1 << (7 - i));
    }
    return pattern;
}

void print_led_status(unsigned char value, unsigned char pattern)
{
    int i;
    int led_count = 0;
    
    for (i = 0; i < 8; i++) {
        if (pattern & (1 << i)) led_count++;
    }
    
    printf("Value: %3d (0x%02X) -> %d LEDs: [", value, value, led_count);
    
    for (i = 7; i >= 0; i--)
    {
        if (pattern & (0x01 << i))
            printf("O");
        else
            printf("X");
        if (i > 0)
            printf(" ");
    }
    printf("]\n");
}

void *handle_client(void *arg)
{
    int client_fd = *(int*)arg;
    char buffer[BUFFER_SIZE] = {0};
    char client_id[50] = "Unknown";
    int n;
    
    printf("Client connected (FD: %d)\n", client_fd);
    add_client(client_fd);
    
    // 로그인 정보 수신
    n = read(client_fd, buffer, BUFFER_SIZE);
    if (n > 0)
    {
        buffer[n] = '\0';
        printf("Login info from FD %d: %s\n", client_fd, buffer);
        
        // 클라이언트 ID 추출
        if(buffer[0] == '[')
        {
            char *end = strchr(buffer, ':');
            if(!end) end = strchr(buffer, ']');
            if(end)
            {
                int len = end - buffer - 1;
                if(len > 0 && len < 49)
                {
                    strncpy(client_id, buffer + 1, len);
                    client_id[len] = '\0';
                }
            }
        }
        
        write(client_fd, "[SERVER]Connected\n", 18);
    }
    
    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);
        n = read(client_fd, buffer, BUFFER_SIZE - 1);
        
        if (n <= 0)
        {
            printf("Client disconnected (FD: %d, ID: %s)\n", client_fd, client_id);
            break;
        }
        
        buffer[n] = '\0';
        printf("\n[FROM %s(FD:%d)]: %s", client_id, client_fd, buffer);
        
        // 모든 메시지를 다른 클라이언트에게 브로드캐스트
        broadcast_to_all(buffer, client_fd);
        
        // LED 데이터 처리
        if (strstr(buffer, "LED@"))
        {
            char *led_str = strstr(buffer, "LED@");
            led_str += 4;
            
            unsigned char dial_value = 0;
            if (strncmp(led_str, "0x", 2) == 0)
            {
                dial_value = (unsigned char)strtoul(led_str, NULL, 16);
            }
            else
            {
                dial_value = (unsigned char)atoi(led_str);
            }
            
            unsigned char led_pattern = value_to_led_pattern(dial_value);
            print_led_status(dial_value, led_pattern);
            
            // LED 디바이스에 쓰기
            if (dev_fd >= 0)
            {
                write(dev_fd, &led_pattern, sizeof(led_pattern));
                
                // LED 변경 알림을 모든 클라이언트에게 전송
                char notify[100];
                snprintf(notify, sizeof(notify), "[SERVER]LED_UPDATE@0x%02x\n", dial_value);
                send_to_all(notify);
            }
        }
        // 일반 메시지 처리
        else if (strstr(buffer, "[ALLMSG]") || strstr(buffer, "["))
        {
            printf("Broadcasting message to all clients\n");
            // 이미 broadcast_to_all로 전송됨
        }
    }
    
    remove_client(client_fd);
    close(client_fd);
    free(arg);
    return NULL;
}

int main()
{
    int server_fd, *client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread_id;
    
    // 클라이언트 배열 초기화
    memset(client_fds, 0, sizeof(client_fds));
    
    // LED 디바이스 열기
    if (access(DEVICE_FILENAME, F_OK) != 0)
    {
        int ret = mknod(DEVICE_FILENAME, S_IRWXU | S_IRWXG | S_IFCHR, (230 << 8) | 0);
        if (ret < 0)
        {
            perror("mknod()");
        }
    }
    
    dev_fd = open(DEVICE_FILENAME, O_RDWR | O_NDELAY);
    if (dev_fd < 0)
    {
        perror("Device open failed");
        printf("Warning: Running without hardware device - Simulation mode\n");
    }
    else
    {
        printf("Device opened successfully\n");
        unsigned char led_off = 0;
        write(dev_fd, &led_off, sizeof(led_off));
    }
    
    // 소켓 생성
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Socket creation failed");
        return -1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 5) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        return -1;
    }
    
    printf("\n===== LED Control Server (Broadcast Mode) =====\n");
    printf("Port: %d\n", PORT);
    printf("All messages will be broadcast to other clients\n");
    printf("===============================================\n");
    printf("Waiting for connections...\n\n");
    
    while (1)
    {
        client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (*client_fd < 0)
        {
            perror("Accept failed");
            free(client_fd);
            continue;
        }
        
        printf("New connection from %s:%d\n", 
               inet_ntoa(client_addr.sin_addr), 
               ntohs(client_addr.sin_port));
        
        if (pthread_create(&thread_id, NULL, handle_client, client_fd) != 0)
        {
            perror("Thread creation failed");
            close(*client_fd);
            free(client_fd);
        }
        
        pthread_detach(thread_id);
    }
    
    close(server_fd);
    if (dev_fd >= 0)
        close(dev_fd);
    
    return 0;
}