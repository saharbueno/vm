#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


#define MAX_PAGES 16   // 2^10 possible physical pages (10-bit physical address)
#define MAX_VPAGES 65536 // 2^16 virtual addresses

// Structure representing a page table entry
typedef struct {
    int valid;   // 1 if present in physical memory
    int vpn;     // virtual page number
    int ppn;     // physical page number (index in memory)
    int R;       // referenced bit
    int M;       // modified bit (write)
} PageTableEntry;

// Global arrays to simulate memory and the page table
PageTableEntry page_table[MAX_VPAGES];
int memory[MAX_PAGES];    // physical memory holding VPNs
int memory_used = 0;      // current number of frames used

// Statistics
int num_reads = 0;
int num_writes = 0;
int num_faults = 0;
int access_count = 0;

// Configurable variables
int page_size;
int clear_r_every;        // how often to clear R bits
int offset_bits;

// Helper to extract VPN from virtual address
int get_vpn(unsigned int address) {
    return address >> offset_bits; // remove offset bits
}

// Clear R bits every N accesses
void maybe_clear_r_bits() {
    if (access_count > 0 && access_count % clear_r_every == 0) {
        for (int i = 0; i < MAX_VPAGES; ++i) {
            if (page_table[i].valid) {
                page_table[i].R = 0;
            }
        }
    }
}

// NRU Replacement Policy (to be implemented fully later)
int select_victim_page() {
    int best_class = 4;
    int victim_ppn = -1;

    for (int ppn = 0; ppn < memory_used; ++ppn) {
        int vpn = memory[ppn];
        if (vpn == -1) continue;

        int R = page_table[vpn].R;
        int M = page_table[vpn].M;
        int class = 2 * R + M;

        if (class < best_class) {
            best_class = class;
            victim_ppn = ppn;

            // Only break if this is the absolute best case
            if (best_class == 0)
                break;
        }
    }

    return victim_ppn;
}









// Load a page into physical memory
void handle_page_fault(int vpn, int is_write) {
    num_faults++;

    // If space is available in physical memory
    if (memory_used < MAX_PAGES) {
        page_table[vpn].valid = 1;
        page_table[vpn].ppn = memory_used;
        page_table[vpn].R = 1;
        page_table[vpn].M = is_write;
        page_table[vpn].vpn = vpn;

        memory[memory_used] = vpn;
        memory_used++;
    } else {
        // Select a victim using NRU
        int victim_ppn = select_victim_page();
        int victim_vpn = memory[victim_ppn];

        // Invalidate victim's page table entry
        // Fully invalidate the victim's page table entry
        page_table[victim_vpn].valid = 0;
        page_table[victim_vpn].R = 0;
        page_table[victim_vpn].M = 0;
        page_table[victim_vpn].ppn = -1;


        // Replace with new page
        page_table[vpn].valid = 1;
        page_table[vpn].ppn = victim_ppn;
        page_table[vpn].R = 1;
        page_table[vpn].M = is_write;
        page_table[vpn].vpn = vpn;

        memory[victim_ppn] = vpn;
    }
}


// Main function
int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s inputfile pagesize clear_r_every_n\n", argv[0]);
        return 1;
    }

    // Parse command line arguments
    char *input_file = argv[1];
    page_size = atoi(argv[2]);
    clear_r_every = atoi(argv[3]);

    if (page_size != 32 && page_size != 64 && page_size != 128) {
        printf("Error: Page size must be 32, 64, or 128.\n");
        return 1;
    }

    if (clear_r_every <= 0) {
        printf("Error: clear_r_every must be > 0.\n");
        return 1;
    }

    // Compute number of bits to shift for VPN
    offset_bits = (int)(log2(page_size));

    // Initialize memory
    for (int i = 0; i < MAX_PAGES; i++) {
        memory[i] = -1; // -1 indicates unused frame
    }

    // Open input file
    FILE *fp = fopen(input_file, "r");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    // Read address and operation from file
    char address_str[10];
    int op; // 0 = read, 1 = write
    while (fscanf(fp, "%s %d", address_str, &op) == 2) {
        unsigned int address = (unsigned int)strtol(address_str, NULL, 16);
        int vpn = get_vpn(address);

        access_count++;
        if (op == 0) num_reads++;
        else num_writes++;

        if (!page_table[vpn].valid) {
            handle_page_fault(vpn, op);
        } else {
            page_table[vpn].R = 1;
            if (op == 1) page_table[vpn].M = 1;
        }

        maybe_clear_r_bits();  // 👈 now runs *after* updating R

    }

    fclose(fp);

    // Print statistics
    printf("num reads = %d\n", num_reads);
    printf("num writes = %d\n", num_writes);
    printf("percentage of page faults %.2f\n",
           (num_reads + num_writes) == 0 ? 0.0 : ((float)num_faults / (num_reads + num_writes)));

    // Print memory contents
    for (int i = 0; i < 16; i++) {
        if (i < memory_used && memory[i] != -1)
            printf("mem[%d]: %x\n", i, memory[i]);
        else
            printf("mem[%d]: ffffffff\n", i);
    }

    return 0;
}