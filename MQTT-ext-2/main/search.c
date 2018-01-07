#include "esp_system.h"
#include <stdlib.h>
#include <string.h>

/*
 * Search a string within a message. If found return true.
 */
static bool search_string(const char zoek[],  char input[]) {
 char *s ;

 s = strstr(input, zoek);      // search for string zoek in input
 if (s != NULL)                     // if successful then s now points at zoek
 {
    // printf("Found string at index = %d\n", s - input);
     return true;
 }                                  // index of "zoek" in input can be found by pointer subtraction
 else
 {
    // printf("String not found\n");  // `strstr` returns NULL if search string not found
     return false;
 }
}
