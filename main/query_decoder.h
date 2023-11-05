#pragma once
#include <stdbool.h>

typedef struct {
    char* query[16];
    int queryCount;
}query_t;

char* queryStr(query_t query, const char* key);
int queryInt(query_t query, const char* key);
bool queryContains(query_t query, const char* key);

query_t queryDecode(char* str);