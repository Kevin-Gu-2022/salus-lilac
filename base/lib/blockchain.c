#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cJSON.h>
#include <zephyr/logging/log.h>
#include <mbedtls/sha256.h>
#include "blockchain.h"

LOG_MODULE_REGISTER(blockchain, LOG_LEVEL_DBG);

#define STACK_SIZE 4096
#define THREAD_PRIORITY 2

static Block chain[MAX_BLOCKS];
static int block_count = 0;

static void to_sha256_hex(const char *input, char *output) {
    unsigned char hash[32];
    mbedtls_sha256_context ctx;

    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const unsigned char *)input, strlen(input));
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);

    for (int i = 0; i < 32; i++) {
        sprintf(output + i * 2, "%02x", hash[i]);
    }
    output[64] = '\0';
}

void add_block(const char *timestamp, const char *event, const char *user, const char *mac) {
    if (block_count >= MAX_BLOCKS) return;

    Block *new_block = &chain[block_count];
    strncpy(new_block->timestamp, timestamp, sizeof(new_block->timestamp) - 1);
    strncpy(new_block->event, event, sizeof(new_block->event) - 1);
    strncpy(new_block->user, user, sizeof(new_block->user) - 1);
    strncpy(new_block->MAC, mac, sizeof(new_block->MAC) - 1);

    const char *prev_hash = (block_count > 0) ? chain[block_count - 1].curr_hash : "GENESIS";
    strncpy(new_block->prev_hash, prev_hash, HASH_SIZE - 1);

    // Build JSON without curr_hash
    cJSON *block_json = cJSON_CreateObject();
    cJSON_AddStringToObject(block_json, "timestamp", new_block->timestamp);
    cJSON_AddStringToObject(block_json, "event", new_block->event);
    cJSON_AddStringToObject(block_json, "user", new_block->user);
    cJSON_AddStringToObject(block_json, "MAC", new_block->MAC);
    cJSON_AddStringToObject(block_json, "prev_hash", new_block->prev_hash);

    char *serialized = cJSON_PrintUnformatted(block_json);
    to_sha256_hex(serialized, new_block->curr_hash);

    cJSON_Delete(block_json);
    free(serialized);

    block_count++;

    /* Uncomment if file system exists

    // Append to file
    FILE *fp = fopen(BLOCKCHAIN_FILE, "a");
    if (fp) {
        cJSON *full_json = cJSON_CreateObject();
        cJSON_AddStringToObject(full_json, "timestamp", new_block->timestamp);
        cJSON_AddStringToObject(full_json, "event", new_block->event);
        cJSON_AddStringToObject(full_json, "user", new_block->user);
        cJSON_AddStringToObject(full_json, "MAC", new_block->MAC);
        cJSON_AddStringToObject(full_json, "prev_hash", new_block->prev_hash);
        cJSON_AddStringToObject(full_json, "curr_hash", new_block->curr_hash);

        char *line = cJSON_PrintUnformatted(full_json);
        fprintf(fp, "%s\n", line);

        fclose(fp);
        cJSON_Delete(full_json);
        free(line);
    } else {
        printk("Failed to open blockchain file\n");
    }
    
    */
}

/**
 * Validate all blocks in blockchain file
 */
bool validate_chain_from_file(void) {
    FILE *fp = fopen(BLOCKCHAIN_FILE, "r");
    if (!fp) {
        printk("Failed to open blockchain file\n");
        return false;
    }

    char line[600];
    Block prev = {0}, curr;
    bool first = true;

    while (fgets(line, sizeof(line), fp)) {
        cJSON *json = cJSON_Parse(line);
        if (!json) {
            fclose(fp);
            printk("Invalid JSON in file\n");
            return false;
        }

        strncpy(curr.timestamp, cJSON_GetObjectItem(json, "timestamp")->valuestring, sizeof(curr.timestamp));
        strncpy(curr.event,     cJSON_GetObjectItem(json, "event")->valuestring,     sizeof(curr.event));
        strncpy(curr.user,      cJSON_GetObjectItem(json, "user")->valuestring,      sizeof(curr.user));
        strncpy(curr.MAC,       cJSON_GetObjectItem(json, "MAC")->valuestring,       sizeof(curr.MAC));
        strncpy(curr.prev_hash, cJSON_GetObjectItem(json, "prev_hash")->valuestring, HASH_SIZE);
        strncpy(curr.curr_hash, cJSON_GetObjectItem(json, "curr_hash")->valuestring, HASH_SIZE);

        if (!first) {
            // Recompute hash from previous block
            cJSON *rebuild = cJSON_CreateObject();
            cJSON_AddStringToObject(rebuild, "timestamp", prev.timestamp);
            cJSON_AddStringToObject(rebuild, "event",     prev.event);
            cJSON_AddStringToObject(rebuild, "user",      prev.user);
            cJSON_AddStringToObject(rebuild, "MAC",       prev.MAC);
            cJSON_AddStringToObject(rebuild, "prev_hash", prev.prev_hash);

            char *serialized = cJSON_PrintUnformatted(rebuild);
            char recomputed[HASH_SIZE];
            to_sha256_hex(serialized, recomputed);

            if (strcmp(curr.prev_hash, recomputed) != 0) {
                printk("Validation failed at timestamp: %s\n", curr.timestamp);
                cJSON_Delete(json);
                cJSON_Delete(rebuild);
                free(serialized);
                fclose(fp);
                return false;
            }

            cJSON_Delete(rebuild);
            free(serialized);
        }

        prev = curr;
        cJSON_Delete(json);
        first = false;
    }

    fclose(fp);
    return true;
}

/**
 * Temporary function just to test the validation of blockchain
 */
bool validate_chain_in_RAM(void) {

    char computed_hash[HASH_SIZE];
    for (int i = 1; i < block_count; i++) {
        cJSON *block_json = cJSON_CreateObject();
        cJSON_AddStringToObject(block_json, "timestamp", chain[i - 1].timestamp);
        cJSON_AddStringToObject(block_json, "event", chain[i - 1].event);
        cJSON_AddStringToObject(block_json, "user", chain[i - 1].user);
        cJSON_AddStringToObject(block_json, "MAC", chain[i - 1].MAC);
        cJSON_AddStringToObject(block_json, "prev_hash", chain[i - 1].prev_hash);

        char *serialized = cJSON_PrintUnformatted(block_json);
        to_sha256_hex(serialized, computed_hash);

        if (strcmp(computed_hash, chain[i].prev_hash) != 0) {
            printf("Invalid chain at block %d\n", i);
            cJSON_Delete(block_json);
            free(serialized);
            return false;
        }

        cJSON_Delete(block_json);
        free(serialized);
    }
    return true;
}


/**
 * Print the contents of blockchain
 */
void print_chain(void) {
    for (int i = 0; i < block_count; i++) {
        printf("Block %d:\n", i);
        printf("  timestamp:   %s\n", chain[i].timestamp);
        printf("  event:       %s\n", chain[i].event);
        printf("  user:        %s\n", chain[i].user);
        printf("  MAC:         %s\n", chain[i].MAC);
        printf("  prev_hash:   %s\n", chain[i].prev_hash);
        printf("  curr_hash:   %s\n\n", chain[i].curr_hash);
    }
}

/**
 * Thread that periodically checks blockchain
 */
void blockchain_validation_thread() {

    for (;;) {
        K_SLEEP(K_MSEC(15000));
        if (!validate_chain_from_file()) {
            LOG_ERR("Blockchain tampered!");
        } else {
            LOG_INF("Blockchain valid.");
        }
    }

}

// Definte and start validation thread
K_THREAD_DEFINE(blockchain_thread_id, STACK_SIZE, blockchain_validation_thread, NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);