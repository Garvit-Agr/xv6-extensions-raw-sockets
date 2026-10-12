#include "cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void die(const char *msg) {
  fprintf(stderr, "%s\n", msg);
  exit(EXIT_FAILURE);
}

TempestConfig parse_arguments(int argc, char *argv[]) {
  TempestConfig config={NULL, false};

  if (argc<2) die("tempest: invalid location");
  if (argc>3) die("tempest: too many arguments");

  config.city_name=argv[1];
  if (strlen(config.city_name)==0 || strcmp(config.city_name, "--raw")==0) die("tempest: invalid location");

  if (argc==3) {
    if (strcmp(argv[2], "--raw")==0) config.raw_output=true;
    else die("tempest: too many arguments");
  }

  return config;
}