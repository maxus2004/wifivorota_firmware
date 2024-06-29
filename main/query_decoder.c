#include "query_decoder.h"
#include <string.h>

void urlDecode(char* data) {
    // Create two pointers that point to the start of the data
    char* leader = data;
    char* follower = leader;

    // While we're not at the end of the string (current character not NULL)
    while (*leader) {
        // Check to see if the current character is a %
        if (*leader == '%') {

            // Grab the next two characters and move leader forwards
            leader++;
            char high = *leader;
            leader++;
            char low = *leader;

            // Convert ASCII 0-9A-F to a value 0-15
            if (high > 0x39) high -= 7;
            high &= 0x0f;

            // Same again for the low byte:
            if (low > 0x39) low -= 7;
            low &= 0x0f;

            // Combine the two into a single byte and store in follower:
            *follower = (high << 4) | low;
        } else {
            // All other characters copy verbatim
            *follower = *leader;
        }

        // Move both pointers to the next character:
        leader++;
        follower++;
    }
    // Terminate the new string with a NULL character to trim it off
    *follower = 0;
}

char* queryStr(query_t query, const char* key) {
    for (int i = 0;i < query.queryCount;i++) {
        char* eq = strchr(query.query[i], '=');
        if (eq == NULL)continue;
        int len = eq - query.query[i];
        if (strncmp(query.query[i], key, len) == 0) {
            return eq + 1;
        }
    }
    return 0;
}

int queryInt(query_t query, const char* key) {
    return atoi(queryStr(query, key));
}

bool queryContains(query_t query, const char* key) {
    return queryStr(query, key) != 0;
}

query_t queryDecode(char* i) {
    query_t query = { 0 };
    while (*i != '\0') {
        if (*i == '?' || *i == '&') {
            *i = '\0';
            query.query[query.queryCount] = i + 1;
            urlDecode(query.query[query.queryCount]);
            query.queryCount++;
            if (query.queryCount >= 16) break;
        }
        i++;
    }
    return query;
}