#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H

// Max blocks in RAM - CHANGE LATER!!!
#define MAX_BLOCKS 20
#define HASH_SIZE 65

#define BLOCKCHAIN_FILE "/lfs/chain.log"

typedef struct {
    char timestamp[32];
    char event[32];
    char user[32];
    char MAC[18];
    char prev_hash[HASH_SIZE];
    char curr_hash[HASH_SIZE];
} Block;

/* Add block to blockchain */
void add_block(const char *timestamp, const char *event, const char *user, const char *mac);
/* Validate blockchain from file system */
bool validate_chain_from_file(void);
/* Validate blockchain held in RAM */
bool validate_chain_in_RAM(void);
/* Print contents of blockchain */
void print_chain(void);


#endif /* BLOCKCHAIN_H */