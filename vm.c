#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define MAXVPAGES 65536
#define PHYS_MEM_SIZE 1024  // in bytes

// stats
int num_reads = 0;
int num_writes = 0;
int num_faults = 0;
int access_count = 0;

// size + offset variables
int page_size;
int clear_r_every;
int offset_bits;
int max_pages;

// physical memory + memory used
int *mem;
int mem_used = 0;

// page table entry object
typedef struct {
    int R;
    int M;
    int vpn; 
    int ppn;
    int valid;
} PTEntry;

PTEntry pt[MAXVPAGES];

// nru replacement policy helper function
int get_victim() {
    int best = 4;
    int victim_ppn = -1;

    for (int ppn = 0; ppn < mem_used; ++ppn) {
        int vpn = mem[ppn];
        if (vpn == -1) continue;

        int R = pt[vpn].R;
        int M = pt[vpn].M;
        int class = 2 * R + M;

        if (class < best) {
            best = class;
            victim_ppn = ppn;
            if (best == 0) break;
        }
    }

    return victim_ppn;
}

// get vpn from virtual address helper function
int get_vpn(unsigned int address) {
    return address >> offset_bits;
}

// clear r bits every n accesses helper function
void maybe_clear_r_bits() {
    if (access_count > 0 && access_count % clear_r_every == 0) {
        for (int i = 0; i < MAXVPAGES; ++i) {
            if (pt[i].valid) {
                pt[i].R = 0;
            }
        }
    }
}

// load a page into physical mem
void handle_page_fault(int vpn, int is_write) {
    num_faults++;

    if (mem_used < max_pages) {
        pt[vpn].valid = 1;
        pt[vpn].ppn = mem_used;
        pt[vpn].R = 1;
        pt[vpn].M = is_write;
        pt[vpn].vpn = vpn;

        mem[mem_used] = vpn;
        mem_used++;
    } else {
        int victim_ppn = get_victim();
        int victim_vpn = mem[victim_ppn];

        pt[victim_vpn].valid = 0;
        pt[victim_vpn].R = 0;
        pt[victim_vpn].M = 0;
        pt[victim_vpn].ppn = -1;

        pt[vpn].valid = 1;
        pt[vpn].ppn = victim_ppn;
        pt[vpn].R = 1;
        pt[vpn].M = is_write;
        pt[vpn].vpn = vpn;

        mem[victim_ppn] = vpn;
    }
}

// print memory contents
void print_memory() {
    for (int i = 0; i < max_pages; i++) {
        if (i < mem_used && mem[i] != -1)
            printf("mem[%d]: %x\n", i, mem[i]);
        else
            printf("mem[%d]: ffffffff\n", i);
    }
}

// process the memory accesses from file
void process_memory_accesses(FILE *fp) {
    if (page_size == 32) offset_bits = 5;
    else if (page_size == 64) offset_bits = 6;
    else if (page_size == 128) offset_bits = 7;

    max_pages = PHYS_MEM_SIZE / page_size;
    mem = malloc(max_pages * sizeof(int));
    if (!mem) {
        printf("Memory allocation failed.\n");
        exit(1);
    }

    for (int i = 0; i < max_pages; i++) {
        mem[i] = -1;
    }

    char address_str[10];
    int op;
    while (fscanf(fp, "%s %d", address_str, &op) == 2) {
        unsigned int address = (unsigned int)strtol(address_str, NULL, 16);
        int vpn = get_vpn(address);

        access_count++;
        if (op == 0) num_reads++;
        else num_writes++;

        if (!pt[vpn].valid) {
            handle_page_fault(vpn, op);
        } else {
            pt[vpn].R = 1;
            if (op == 1) pt[vpn].M = 1;
        }

        maybe_clear_r_bits();
    }
}

// main function
int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: ./vmem inputfile pagesize clear_r_every\n");
        return 1;
    }

    char *input_file = argv[1];
    page_size = atoi(argv[2]);
    clear_r_every = atoi(argv[3]);

    if (page_size != 32 && page_size != 64 && page_size != 128) {
        printf("Error: Page size must be 32, 64, or 128\n");
        return 1;
    }

    if (clear_r_every <= 0) {
        printf("Error: clear_r_every must be > 0\n");
        return 1;
    }

    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    process_memory_accesses(fp);
    fclose(fp);

    printf("num reads = %d\n", num_reads);
    printf("num writes = %d\n", num_writes);
    printf("percentage of page faults %.2f\n",
           (num_reads + num_writes) == 0 ? 0.0 : ((float)num_faults / (num_reads + num_writes)));

    print_memory();

    free(mem);
    return 0;
}
