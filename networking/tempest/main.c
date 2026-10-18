#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include "cli.h"
#include "net_socket.h"
#include "http.h"

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);

    TempestConfig config=parse_arguments(argc, argv);

    char *encoded_city=url_encode(config.city_name);
    char *request=build_http_request(encoded_city);

    int sockfd=connect_to_host("wttr.is", "80", start);
    send_data(sockfd, request, strlen(request), start);

    if (config.raw_output) print_raw_request(request);

    char *response=receive_data(sockfd, start);
    parse_http_response(response, config.raw_output);

    free(response);
    free(encoded_city);
    free(request);
    close_socket(sockfd);

    return 0;
}