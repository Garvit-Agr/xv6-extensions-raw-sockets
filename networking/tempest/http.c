#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cli.h"
#include "http.h"

char *url_encode(const char *str) {
    if (!str) return NULL;

    size_t len=strlen(str);
    char *encoded=malloc((len*3)+1);
    if (!encoded) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    char *p=encoded;
    for (size_t i=0; i<len; i++) {
        unsigned char c=(unsigned char)str[i];
        if (isalnum(c) || c=='-' || c=='_' || c=='.' || c=='~') *p++=c;
        else {
            sprintf(p, "%%%02X", c);
            p+=3;
        }
    }
    *p='\0';
    return encoded;
}

char *build_http_request(const char *encoded_city) {
    const char *template =
        "GET /%s?0T HTTP/1.1\r\n"
        "Host: wttr.is\r\n"
        "User-Agent: curl/8.0\r\n"
        "Accept: */*\r\n"
        "Connection: close\r\n\r\n";

    size_t req_len=strlen(template)+strlen(encoded_city)+1;
    char *request=malloc(req_len);
    if (!request) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    snprintf(request, req_len, template, encoded_city);
    return request;
}

void print_raw_request(const char *request) {
    const char *curr=request;
    const char *next;
    while ((next=strstr(curr, "\r\n"))!=NULL) {
        if (next==curr) break;

        printf("> %.*s\n", (int)(next-curr), curr);
        curr=next+2;
    }
    printf(">\n");
}

void parse_http_response(const char *response, bool raw_output) {
    const char *body=strstr(response, "\r\n\r\n");
    if (!body) die("tempest: invalid http response format");

    bool is_error=false;
    if (strncmp(response, "HTTP/1.1 200", 12)!=0 && strncmp(response, "HTTP/1.0 200", 12)!=0) is_error=true;
    if (strstr(response, "location not found")!=NULL) is_error=true;

    if (raw_output) {
        const char *curr=response;
        const char *next;

        while (curr<body) {
            next=strstr(curr, "\r\n");
            if (next && next<=body) {
                if (next==curr) break;
                printf("< %.*s\n", (int)(next - curr), curr);
                curr=next+2;
            }
        }
        printf("<\n");
        printf("%s", body+4);
        if (is_error) exit(EXIT_FAILURE);
    }
    else {
        if (is_error) die("tempest: invalid location");
        printf("%s", body+4);
    }
}