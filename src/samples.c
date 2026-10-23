#include "samples.h"

#include <stdlib.h>

/**
 * @brief Free every data node and reset the caller-owned sentinel.
 * @param[in,out] head Sentinel whose nodes were dynamically allocated, or NULL.
 * @note The sentinel itself is never freed; NULL is a no-op.
 */
void samples_free(Sample *head) {
    if (!head) return;
    Sample *node = head->next;
    while (node) {
        Sample *next = node->next;
        free(node);
        node = next;
    }
    *head = (Sample){0};
}
