#include <string.h>
#include <ctype.h>

#include "engine.h"

bool validate_sequence(const char *seq) {
    if (!seq) return false;
    if (strlen(seq)!=5) return false;

    for (int i=0; i<5; i++) {
        if (!isdigit(seq[i])) return false;
    }
    return true;
}

void get_feedback(const char *master, const char *attempt, char *feedback) {
    if (!validate_sequence(master) || !validate_sequence(attempt)) {
        strcpy(feedback, "error");
        return;
    }

    int master_counts[10]={0};
    int attempt_counts[10]={0};
    int x_count=0;
    int o_count=0;
    int dash_count=0;

    for (int i=0; i<5; i++) {
        if (master[i]==attempt[i]) x_count++;
        else {
            master_counts[master[i]-'0']++;
            attempt_counts[attempt[i]-'0']++;
        }
    }

    for (int i=0; i<10; i++) {
        if (master_counts[i]<attempt_counts[i]) o_count+=master_counts[i];
        else o_count+=attempt_counts[i];
    }

    dash_count=5-x_count-o_count;

    int idx=0;
    for (int i=0; i<x_count; i++) feedback[idx++]='x';
    for (int i=0; i<o_count; i++) feedback[idx++]='o';
    for (int i=0; i<dash_count; i++) feedback[idx++]='-';
    
    feedback[idx]='\0';
}