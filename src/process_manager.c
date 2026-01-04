#include <stdlib.h>
#include "../../include/process.h"
#include "../../include/process_manager.h"
#include "../../include/list_utils.h"
#include "../../include/process_reader.h"
#include "../../include/cpu_history.h"
//inutile avec nouvelle version réseau
process_node* get_local_process_list(int topN)
{
    process_t *all = NULL;
    int n = collect_all_processes(&all);

    if (n <= 0){
        free(all);
        return NULL;
    }

    if (n > topN) n = topN;

    process_node *head = NULL;

    for (int i=0;i<n;i++)
        add_process(&head, &all[i]);

    free(all);
    return head;
}
