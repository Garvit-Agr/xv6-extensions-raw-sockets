#include "cli.h"
#include "net_socket.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
  
    TempestConfig config=parse_arguments(argc, argv);
    (void)config;

    int sockfd=connect_to_host("wttr.is", "80");
    
    close_socket(sockfd);



  return 0;
}