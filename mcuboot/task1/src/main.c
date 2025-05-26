#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/sys/printk.h>
#include <zephyr/random/random.h>

#define STACKSIZE               1024
#define RNG_PRIORITY            2         // Priority of the RNG thread (lower priority than display)
#define DISPLAY_PRIORITY        1         // Higher priority for the display thread
#define MSGQ_SIZE               10        // Size of the message queue
#define ALIGNMENT_SIZE_IN_BYTES 4         // Alignment size for message queue
#define MAX_8_DIGIT             100000000 // Used for modulo operation to ensure 8-digit output

// Binary semaphore to synchronize the RNG and display threads
K_SEM_DEFINE(signal_sem, 1, 1); 

// Message queue for storing random numbers, 10 slots, 4 bytes alignment (8 digits)
K_MSGQ_DEFINE(rng_msgq, sizeof(uint32_t), MSGQ_SIZE, ALIGNMENT_SIZE_IN_BYTES);


/*
 * RNG thread: Generates random numbers and puts them into the message queue.
 */
void rgn_thread(void *, void *, void *) {

	uint32_t random_num;
	
    while (1) {

        // Wait until semaphore is available
        k_sem_take(&signal_sem, K_FOREVER);

        // // Generate a random 32-bit number
        random_num = sys_rand32_get();

        // Attempt to put the random number into the message queue when it's ready
        if (k_msgq_put(&rng_msgq, &random_num, K_FOREVER) != 0) {
            // Handle message queue put failure
        }
    }
}

/*
 * Display thread: Retrieves and prints random numbers from the message queue.
 */
void display_thread(void *, void *, void *) {

	uint32_t random_num;

    while (1) {

        // Wait for a number from the queue
        k_msgq_get(&rng_msgq, &random_num, K_FOREVER);

        // Print the random number as a zero-padded 8-digit output
        printk("%08u\n", random_num % MAX_8_DIGIT); 

        // Print every 2 seconds
        k_sleep(K_SECONDS(2)); 

        // Signal RNG thread to generate a new number after the 2s
        k_sem_give(&signal_sem);
    }
}

int main(void) {

    return 0;  // Main does nothing since threads are defined outside
}

// Define and start threads
K_THREAD_DEFINE(rng_tid, STACKSIZE, rgn_thread, NULL, NULL, NULL, RNG_PRIORITY, 0, 0);
K_THREAD_DEFINE(display_tid, STACKSIZE, display_thread, NULL, NULL, NULL, DISPLAY_PRIORITY, 0, 0);