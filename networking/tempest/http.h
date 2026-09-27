#ifndef HTTP_H
#define HTTP_H

char *url_encode(const char *str);
char *build_http_request(const char *encoded_city);
void print_raw_request(const char *request);

#endif