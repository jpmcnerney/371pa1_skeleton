/*
# Copyright 2025 University of Kentucky
#
# Licensed under the Apache License, Version 2.0 (the "License");
# You may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
*/

/*
Please specify the group members here:
# Student #1: JP McNerney
# Student #2: Emma Lohrer
# Student #3: Clayton Curry
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>

#define MAX_EVENTS 64
#define MESSAGE_SIZE 16
#define DEFAULT_CLIENT_THREADS 4

char *server_ip = "127.0.0.1";
int server_port = 12345;
int num_client_threads = DEFAULT_CLIENT_THREADS;
int num_requests = 1000000;

/*
 * This structure is used to store per-thread data in the client.
 */
typedef struct {
    int epoll_fd;        /* File descriptor for the epoll instance, used for monitoring events on the socket. */
    int socket_fd;       /* File descriptor for the client socket connected to the server. */
    long long total_rtt; /* Accumulated Round-Trip Time (RTT) for all messages sent and received (in microseconds). */
    long total_messages; /* Total number of messages sent and received. */
} client_thread_data_t;

/*
 * This function runs in a separate client thread to handle communication with the server.
 */
void *client_thread_func(void *arg) {
    client_thread_data_t *data = (client_thread_data_t *)arg;
    struct epoll_event event, events[MAX_EVENTS];
    char send_buf[MESSAGE_SIZE] = "ABCDEFGHIJKMLNOP"; /* Send 16-Bytes message every time */
    char recv_buf[MESSAGE_SIZE];
    struct timeval start, end;

    // Hint 1: Register the "connected" client_thread's socket in its epoll instance.
    // Hint 2: Use gettimeofday() and "struct timeval start, end" to record timestamps, which can be used to calculate RTT.

    // Register socket with epoll so we can wait for events on it
    event.events = EPOLLIN;
    event.data.fd = data->socket_fd;
    if (epoll_ctl(data->epoll_fd, EPOLL_CTL_ADD, data->socket_fd, &event) < 0) {
        perror("epoll_ctl failed"); // Error handling
        pthread_exit(NULL);
    }

    /* TODO:
     * It sends messages to the server, waits for a response using epoll,
     * and measures the round-trip time (RTT) of this request-response.
     */
    for (int i = 0; i < num_requests; i++) {
        gettimeofday(&start, NULL);

        // Send message to server
        if (write(data->socket_fd, send_buf, MESSAGE_SIZE) != MESSAGE_SIZE) {
            perror("client write error"); // Error handling
            break;
        }

        // Wait for the server to respond
        int nfds = epoll_wait(data->epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            perror("epoll_wait error"); // Error handling
            break;
        }

        for (int e = 0; e < nfds; e++) {
            if (events[e].data.fd == data->socket_fd) {
                // Read response from server
                if (read(data->socket_fd, recv_buf, MESSAGE_SIZE) != MESSAGE_SIZE) {
                    perror("client read error"); // Error handling
                    break;
                }
            }
        }

        // Measure RTT
        gettimeofday(&end, NULL);
        long long start_us = (long long)start.tv_sec * 1000000 + start.tv_usec;
        long long end_us   = (long long)end.tv_sec   * 1000000 + end.tv_usec;
        long long rtt_us   = end_us - start_us;

        // Update stats
        data->total_rtt += rtt_us;
        data->total_messages++;
    }

    /* TODO:
     * The function exits after sending and receiving a predefined number of messages (num_requests).
     * It calculates the request rate based on total messages and RTT.
     */
    pthread_exit(NULL);
}

/*
 * This function orchestrates multiple client threads to send requests to a server,
 * collect performance data from each thread, and compute aggregated metrics of all threads.
 */
void run_client() {
    pthread_t threads[num_client_threads];
    client_thread_data_t thread_data[num_client_threads];
    struct sockaddr_in server_addr;

    /* TODO:
     * Create sockets and epoll instances for client threads
     * and connect these sockets of client threads to the server.
     */
    
    // Hint: Use thread_data to save the created socket and epoll instance for each thread.
    // You will pass the thread_data to pthread_create() as below.
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    for (int i = 0; i < num_client_threads; i++) {
        thread_data[i].total_rtt = 0;
        thread_data[i].total_messages = 0;

        thread_data[i].socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(thread_data[i].socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("connect failed"); // Error handling
            close(thread_data[i].socket_fd);
            continue;
        }

        thread_data[i].epoll_fd = epoll_create1(0);
        if (thread_data[i].epoll_fd < 0) {
            perror("epoll_create1 failed"); // Error handling
            close(thread_data[i].socket_fd);
            continue;
        }

        pthread_create(&threads[i], NULL, client_thread_func, &thread_data[i]);
    }

    /* TODO:
     * Wait for client threads to complete and aggregate metrics of all client threads.
     */
    long long total_rtt = 0;
    long total_messages = 0;

    for (int i = 0; i < num_client_threads; i++) {
        pthread_join(threads[i], NULL);

        total_rtt += thread_data[i].total_rtt;
        total_messages += thread_data[i].total_messages;

        close(thread_data[i].socket_fd);
        close(thread_data[i].epoll_fd);
    }

    if (total_messages > 0) {
        float total_time_sec = (float)total_rtt / 1000000.0;
        float total_request_rate = total_messages / total_time_sec;

        printf("Average RTT: %lld us\n", total_rtt / total_messages);
        printf("Total Request Rate: %f requests/s\n", total_request_rate);
    } else {
        printf("No successful messages.\n");
    }
}

/*
 * Basic server that uses epoll to handle multiple clients.
 */
void run_server() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv_addr;
    struct epoll_event event, events[MAX_EVENTS];

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &srv_addr.sin_addr);

    bind(server_fd, (struct sockaddr *)&srv_addr, sizeof(srv_addr));
    listen(server_fd, 128);

    int epoll_fd = epoll_create1(0);
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            if (fd == server_fd) {
                int client_fd = accept(server_fd, NULL, NULL);
                event.events = EPOLLIN;
                event.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
            } else {
                char buffer[MESSAGE_SIZE];
                if (read(fd, buffer, MESSAGE_SIZE) > 0) {
                    write(fd, buffer, MESSAGE_SIZE);
                } else {
                    close(fd);
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (strcmp(argv[1], "server") == 0) run_server();
    else if (strcmp(argv[1], "client") == 0) run_client();
    else printf("Invalid usage.\n");
}
