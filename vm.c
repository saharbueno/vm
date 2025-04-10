#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// constants
#define VIRT_PAGE_COUNT 65536
#define PHYSICAL_MEM_SIZE 1024

// stats
int read_count = 0;
int write_count = 0;
int fault_count = 0;
int total_accesses = 0;

// config
int frame_size;
int clear_r_interval;
int addr_offset;
int frame_limit;

// simulated physical memory
int *frame_table;
int frames_filled = 0;

// page table entry
typedef struct {
    int ref_bit;
    int mod_bit;
    int virt_page;
    int phys_frame;
    int active;
} PageRecord;

PageRecord page_map[VIRT_PAGE_COUNT];

// select frame to evict using NRU policy
int choose_replacement() {
    int best_class = 4;
    int selected_frame = -1;

    for (int i = 0; i < frames_filled; ++i) {
        int current_vpn = frame_table[i];
        if (current_vpn == -1) continue;

        int R = page_map[current_vpn].ref_bit;
        int M = page_map[current_vpn].mod_bit;
        int nru_class = 2 * R + M;

        if (nru_class < best_class) {
            best_class = nru_class;
            selected_frame = i;
            if (best_class == 0) break;
        }
    }

    return selected_frame;
}

// extract virtual page number
int extract_vpn(unsigned int addr) {
    return addr >> addr_offset;
}

// clear referenced bits at regular intervals
void reset_ref_bits() {
    if (total_accesses > 0 && total_accesses % clear_r_interval == 0) {
        for (int i = 0; i < VIRT_PAGE_COUNT; ++i) {
            if (page_map[i].active) {
                page_map[i].ref_bit = 0;
            }
        }
    }
}

// handle page fault (load or replace)
void resolve_page_fault(int vpn, int write_flag) {
    fault_count++;

    if (frames_filled < frame_limit) {
        page_map[vpn].active = 1;
        page_map[vpn].phys_frame = frames_filled;
        page_map[vpn].ref_bit = 1;
        page_map[vpn].mod_bit = write_flag;
        page_map[vpn].virt_page = vpn;

        frame_table[frames_filled] = vpn;
        frames_filled++;
    } else {
        int repl_frame = choose_replacement();
        int old_vpn = frame_table[repl_frame];

        page_map[old_vpn].active = 0;
        page_map[old_vpn].ref_bit = 0;
        page_map[old_vpn].mod_bit = 0;
        page_map[old_vpn].phys_frame = -1;

        page_map[vpn].active = 1;
        page_map[vpn].phys_frame = repl_frame;
        page_map[vpn].ref_bit = 1;
        page_map[vpn].mod_bit = write_flag;
        page_map[vpn].virt_page = vpn;

        frame_table[repl_frame] = vpn;
    }
}

// print memory contents
void dump_memory() {
    for (int i = 0; i < frame_limit; i++) {
        if (i < frames_filled && frame_table[i] != -1)
            printf("frame[%d]: %x\n", i, frame_table[i]);
        else
            printf("frame[%d]: ffffffff\n", i);
    }
}

// read and process memory access events
void simulate(FILE *input) {
    if (frame_size == 32) addr_offset = 5;
    else if (frame_size == 64) addr_offset = 6;
    else if (frame_size == 128) addr_offset = 7;

    frame_limit = PHYSICAL_MEM_SIZE / frame_size;
    frame_table = malloc(frame_limit * sizeof(int));
    if (!frame_table) {
        printf("Memory allocation failed.\n");
        exit(1);
    }

    for (int i = 0; i < frame_limit; i++) {
        frame_table[i] = -1;
    }

    char addr_str[10];
    int rw_flag;
    while (fscanf(input, "%s %d", addr_str, &rw_flag) == 2) {
        unsigned int virt_addr = (unsigned int)strtol(addr_str, NULL, 16);
        int vpn = extract_vpn(virt_addr);

        total_accesses++;
        if (rw_flag == 0) read_count++;
        else write_count++;

        if (!page_map[vpn].active) {
            resolve_page_fault(vpn, rw_flag);
        } else {
            page_map[vpn].ref_bit = 1;
            if (rw_flag == 1) page_map[vpn].mod_bit = 1;
        }

        reset_ref_bits();
    }
}

// entry point
int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: ./vmem inputfile pagesize clear_r_every\n");
        return 1;
    }

    char *filename = argv[1];
    frame_size = atoi(argv[2]);
    clear_r_interval = atoi(argv[3]);

    if (frame_size != 32 && frame_size != 64 && frame_size != 128) {
        printf("Error: Page size must be 32, 64, or 128\n");
        return 1;
    }

    if (clear_r_interval <= 0) {
        printf("Error: clear_r_every must be > 0\n");
        return 1;
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Error opening file");
        return 1;
    }

    simulate(f);
    fclose(f);

    printf("num reads = %d\n", read_count);
    printf("num writes = %d\n", write_count);
    printf("percentage of page faults %.2f\n",
           (read_count + write_count) == 0 ? 0.0 : ((float)fault_count / (read_count + write_count)));

    dump_memory();
    free(frame_table);
    return 0;
}
