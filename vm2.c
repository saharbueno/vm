#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Structs
//so each page table entry has fields valid, frame, R and M 
typedef struct {
    int valid;
    int frame;
    int R; // Read from or written to (accessed)
    int M; // Written to (modified)
} PageTableEntry;

typedef struct {
    int vpn;
    int occupied;
    int R;
    int M;
} PhysicalFrame;

// Global variables
PageTableEntry* page_table;
PhysicalFrame* physical_memory;
int page_size, offset_bits;
int num_virtual_pages, num_frames;
int page_faults = 0;
int total_reads = 0, total_writes = 0;

// Function declarations
int get_offset_bits(int page_size);
int extract_vpn(unsigned int virtual_address);
void handle_memory_access(unsigned int virtual_address, int vpn, int op);
void reset_r_bits();
int find_victim(); //NRU

// main function
int main(int argc, char* argv[]) {
    // Check for correct number of command line arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <input_file> <page_size> <R_reset_interval>\n", argv[0]);
        return 1;
    }

    // Parse command line arguments
    char* input_file_name = argv[1];
    page_size = atoi(argv[2]);         // e.g. 32, 64, or 128
    int reset_interval = atoi(argv[3]); // After how many accesses to clear R bits

    // Determine offset size and number of pages/frames
    offset_bits = get_offset_bits(page_size);
    num_virtual_pages = 1 << (16 - offset_bits); // bc we have 16-bit virtual address space -> this will get u 2^x
    num_frames = 1 << (10 - offset_bits);        // bc we have a 10-bit physical address space

    // Allocate memory for page table and physical memory
    page_table = calloc(num_virtual_pages, sizeof(PageTableEntry));
    physical_memory = calloc(num_frames, sizeof(PhysicalFrame));

    // Open the input file
    FILE* file = fopen(input_file_name, "r");
    /*if (!file) {
        perror("Error opening file");
        return 1;
    }*/

    char input_line[100];
    int access_count = 0;

    // Process each memory access line in the file
    while (fgets(input_line, sizeof(input_line), file)) {
        char address_string[10];  // holds the hex string like "abcd"
        int operation;           // 0 for read, 1 for write

        //read the inputs 
        sscanf(input_line, "%s %d", address_string, &operation);

        unsigned int virtual_address;
        //convert the hex string to int 
        sscanf(address_string, "%x", &virtual_address); // convert hex string to int

        //get virtual page number
        int virtual_page_number = extract_vpn(virtual_address);

        // Update read/write counters
        if (operation == 0) total_reads++;
        else total_writes++;

        // Handle the memory access
        //Checks if the page is already in memory (hit)
        handle_memory_access(virtual_address, virtual_page_number, operation);

        access_count++;

        // Reset R Bits Every N Accesses
        if (access_count % reset_interval == 0) {
            reset_r_bits();
        }
    }

    fclose(file);

    //Output required by the assignment
    printf("num reads = %d\n", total_reads);
    printf("num writes = %d\n", total_writes);
    printf("percentage of page faults = %.2f\n", (float)page_faults / (total_reads + total_writes));

    // Print physical memory (final output)
    for (int i = 0; i < num_frames; i++) {
        if (physical_memory[i].occupied)
            printf("mem[%d]: %x\n", i, physical_memory[i].vpn);
        else
            printf("mem[%d]: ffffffff\n", i);
    }

    // Free dynamically allocated memory
    free(page_table);
    free(physical_memory);
    return 0;
}

int get_offset_bits(int page_size){
    int offset_bits;

    if (page_size == 32){
        //32 = 2^5
        //so for 32 bytes, will need 5 bits
        offset_bits = 5;
    } else if (page_size == 64){
        offset_bits = 6;
    } else if (page_size == 128){
        offset_bits = 7;
    } else {
        fprintf(stderr, "Invalid page size. Must be 32, 64, or 128.\n");
        exit(1);
    }
    return offset_bits;
}

int extract_vpn(unsigned int virtual_address){
    //just remove offset_bits from virtual address, you get the page number
    int vpn = virtual_address >> offset_bits;
    return vpn;
}


void handle_memory_access(unsigned int virtual_address, int vpn, int operation){
    //check if it's a page hit or a page fault for given vpn
    if (page_table[vpn].valid){
        int frame = page_table[vpn].frame;

        page_table[vpn].R = 1;
        //operation is write
        if (operation == 1){
            page_table[vpn].M = 1;
        }

        //you updated the R+M in the 
        physical_memory[frame].R = 1;
        if (operation == 1){
            physical_memory[frame].M = 1;
        }
    }
    //if u don't find the page 
    else {
        //this is a page fault so
        page_faults++;
        //now need to look for a frame to load page into - chekc if frame is empty
        /*physical_memory = calloc(num_frames, sizeof(PhysicalFrame));*/
        int frame = -1;
        for (int i = 0; i < num_frames; i++){
            //look at struct above 
            if (!physical_memory[i].occupied){
                frame = i;
                break;
            }
        }

        //let's say you didn't find a free physical frame for your page
        //now you're going to have to use NRU algo to evict a frame 
        if (frame == -1){
            //method to call NRU algo 
            frame = find_victim();
            //this is the virtual page number of the page you want to evict
            int old_vpn = physical_memory[frame].vpn;
            page_table[old_vpn].valid = 0;

        }

        //so you just had a page fault 
        //so now you have to add this page into physical memory
        //You need to update both:The page table (to map VPN → frame)
        //The physical memory (to say this frame now holds VPN)

        //load into page table
        page_table[vpn].valid = 1;
        page_table[vpn].frame = frame;
        page_table[vpn].R = 1;
        page_table[vpn].M = (operation == 1);

        //load into physical frame
        physical_memory[frame].vpn = vpn;
        physical_memory[frame].occupied = 1;
        physical_memory[frame].R = 1;
        physical_memory[frame].M = (operation == 1);

    }
}

//NRU algorithm - we're looking through occupied physical memory frames
int find_victim(){
    //the worst possible frame
    int best_class = 10;
    //no frame selected
    int best_frame = -1;

    for (int i = 0; i < num_frames; i++){
        if (physical_memory[i].occupied == 1){
        //don't go down unless occupied

            int R = physical_memory[i].R;
            int M = physical_memory[i].M;

            int current_class = 3;

            // classify the page (0 to 3) based on R and M
            if (R == 0 && M == 0) {
                current_class = 0;
            } else if (R == 0 && M == 1) {
                current_class = 1;
            } else if (R == 1 && M == 0) {
                current_class = 2;
            } else {
                current_class = 3;
            }

            // is this better than what we’ve seen so far? if it is then
            if (current_class < best_class) {
                best_class = current_class;
                best_frame = i;

                if (current_class == 0){
                    break;
                }
            }
        }
    }

    return best_frame;

}

void reset_r_bits(){
    for (int i = 0; i < num_virtual_pages; i++){
        if (page_table[i].valid == 1){
            //set the access bit to 0 
            page_table[i].R = 0;
            physical_memory[page_table[i].frame].R = 0;
        }
    }
}
