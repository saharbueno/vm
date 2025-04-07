// imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// max number of physical pages
#define MAXPAGES 16
// total number of virtual pages
#define MAXVPAGES 65536

// stats
int reads = 0;
int writes = 0;
int faults = 0;
int total_accesses = 0;

// page size and tracking for R-bit clears
int pg_size;
int r_clear_interval;
int offset_bits;

// memory and usage tracking
int physical_mem[MAXPAGES];
int used_pages = 0;

// page table entry
typedef struct {
    int valid;
    int vpn;
    int ppn;
    int ref;
    int mod;
} PTE;
PTE table[MAXVPAGES];

// extract vpn from address
int extract_vpn(unsigned int addr) {
    return addr >> offset_bits;
}

// clear referenced bits if needed
void clear_referenced_bits() {
    if (total_accesses > 0 && total_accesses % r_clear_interval == 0) {
        for (int i = 0; i < MAXVPAGES; i++) {
            if (table[i].valid) {
                table[i].ref = 0;
            }
        }
    }
}

// select victim using NRU
int nru_select() {
    int min_class = 4;
    int victim = -1;

    for (int i = 0; i < used_pages; i++) {
        int vpn = physical_mem[i];
        if (vpn == -1) continue;

        int r = table[vpn].ref;
        int m = table[vpn].mod;
        int class = 2 * r + m;

        if (class < min_class) {
            min_class = class;
            victim = i;
            if (min_class == 0) break;
        }
    }
    return victim;
}

// handle page fault
void page_fault(int vpn, int write) {
    faults++;

    if (used_pages < MAXPAGES) {
        table[vpn].valid = 1;
        table[vpn].ppn = used_pages;
        table[vpn].ref = 1;
        table[vpn].mod = write;
        table[vpn].vpn = vpn;

        physical_mem[used_pages] = vpn;
        used_pages++;
    } else {
        int replace_index = nru_select();
        int evict_vpn = physical_mem[replace_index];

        table[evict_vpn].valid = 0;
        table[evict_vpn].ref = 0;
        table[evict_vpn].mod = 0;
        table[evict_vpn].ppn = -1;

        table[vpn].valid = 1;
        table[vpn].ppn = replace_index;
        table[vpn].ref = 1;
        table[vpn].mod = write;
        table[vpn].vpn = vpn;

        physical_mem[replace_index] = vpn;
    }
}

// initialize memory
void initialize_memory() {
    for (int i = 0; i < MAXPAGES; i++) {
        physical_mem[i] = -1;
    }
}

// print memory contents
void display_memory() {
    for (int i = 0; i < 16; i++) {
        if (i < used_pages && physical_mem[i] != -1)
            printf("mem[%d]: %x\n", i, physical_mem[i]);
        else
            printf("mem[%d]: ffffffff\n", i);
    }
}

// simulate memory accesses
void simulate_accesses(FILE *file) {
    offset_bits = (int)(log2(pg_size));
    initialize_memory();

    char hex_addr[10];
    int action;

    while (fscanf(file, "%s %d", hex_addr, &action) == 2) {
        unsigned int addr = (unsigned int)strtol(hex_addr, NULL, 16);
        int vpn = extract_vpn(addr);

        total_accesses++;
        if (action == 0) reads++;
        else writes++;

        if (!table[vpn].valid) {
            page_fault(vpn, action);
        } else {
            table[vpn].ref = 1;
            if (action == 1) table[vpn].mod = 1;
        }

        clear_referenced_bits();
    }
}

// main
int main(int argc, char *argv[]) {
    char *filename = argv[1];
    pg_size = atoi(argv[2]);
    r_clear_interval = atoi(argv[3]);

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("file open error");
        return 1;
    }

    simulate_accesses(file);
    fclose(file);

    printf("num reads = %d\n", reads);
    printf("num writes = %d\n", writes);
    printf("percentage of page faults %.2f\n",
        (reads + writes) == 0 ? 0.0 : ((float)faults / (reads + writes)));

    display_memory();
    return 0;
}