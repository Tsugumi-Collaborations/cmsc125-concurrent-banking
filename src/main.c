/*
 * file: main.c
 */
#include "../include/bank.h"
#include "../include/transaction.h"
#include "../include/timer.h"
#include "../include/metrics.h"
#include "../include/lock_manager.h"
#include "../include/buffer_pool.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

// Global Variables
bool verbose = false;
int deadlock_strategy = 0; // 0 = prevention, 1 = detection

int main(int argc, char *argv[])
{
    // Command-line Options:
    // --accounts=FILE: Initial account balances
    // --trace=FILE: Transaction workload
    // --deadlock=prevention|detection: Deadlock strategy
    // --tick-ms=N: Milliseconds per tick (default: 100)
    // --verbose: Print detailed operation logs
    char accounts_file[256] = "";
    char trace_file[256] = "";
    char deadlock[16] = "prevention";
    int tick = 100;

    // Command-line Argument Parsing:
    for (int i = 1; i < argc; i++)
    {
        if (strncmp(argv[i], "--accounts=", 11) == 0)
        {
            strncpy(accounts_file, argv[i] + 11, sizeof(accounts_file) - 1);
            printf("account file in command line detected.\n");
        }
        else if (strncmp(argv[i], "--trace=", 8) == 0)
        {
            strncpy(trace_file, argv[i] + 8, sizeof(trace_file) - 1);
            printf("trace file in command line detected.\n");
        }
        else if (strncmp(argv[i], "--deadlock=", 11) == 0)
        {
            strncpy(deadlock, argv[i] + 11, sizeof(deadlock) - 1);
            printf("deadlock in command line detected.\n");
        }
        else if (strncmp(argv[i], "--tick-ms=", 10) == 0)
        {
            tick = atoi(argv[i] + 10);
            if (tick <= 0)
            {
                fprintf(stderr, "Error: Tick speed must be a positive integer.\n");
                return 0;
            }
            printf("tick input in command line detected.\n");
        }
        else if (strncmp(argv[i], "--verbose", 9) == 0)
        {
            verbose = true;
            printf("verbose input in command line detected.\n");
        }
        else
        {
            fprintf(stderr, "Unknown/Missing argument(s): %s\n", argv[i]);
            return 0;
        }
    }

    // Initilizations:

    // Deadlock Strategy
    if (strncmp(deadlock, "prevention", 16) == 0)
    {
        deadlock_strategy = 0;
        printf("deadlock prevention strategy selected.\n");
    }
    else if (strncmp(deadlock, "detection", 16) == 0)
    {
        deadlock_strategy = 1;
        printf("deadlock detection strategy selected.\n");
    }
    else
    {
        fprintf(stderr, "Error: Invalid deadlock strategy '%s'. Use 'prevention' or 'detection'.\n", deadlock);
        return 0;
    }

    // Accounts
    int num_accounts = 0;
    Account *accounts = load_accounts(accounts_file, &num_accounts);
    if (accounts == NULL)
    {
        printf("account loading did not occur.\n");
        return 0;
    }
    else
    {
        printf("account loading successful.\n");
    }

    // Bank
    bank.num_accounts = num_accounts;
    for (int i = 0; i < num_accounts; i++)
    {
        bank.accounts[i].account_id       = accounts[i].account_id;
        bank.accounts[i].balance_centavos = accounts[i].balance_centavos;
        pthread_rwlock_init(&bank.accounts[i].lock, NULL); 
    }
    pthread_mutex_init(&bank.bank_lock, NULL);

    // Buffer Pool
    init_buffer_pool(&buffer_pool);

    // Transactions
    int num_transactions = 0;
    Transaction *transactions = load_transactions(trace_file, &num_transactions);
    if (transactions == NULL)
    {
        printf("transaction loading did not occur.\n");
        return 0;
    }
    else
    {
        printf("transaction loading successful.\n");
    }

    for (int i = 0; i < num_transactions; i++)
        print_transaction(&transactions[i]);

    // Tick Interval
    tick_interval_ms = tick;

    // Set active transaction count for deadlock detection
    // extern int num_active_transactions;
    num_active_transactions = num_transactions;

    simulation_running = 1;

    // Start the timer thread
    pthread_t timer_tid;
    pthread_create(&timer_tid, NULL, timer_thread, NULL);

    // Launch one thread per transaction
    for (int i = 0; i < num_transactions; i++)
    {
        pthread_create(&transactions[i].thread, NULL,
                       execute_transaction, &transactions[i]);
    }

    // Wait for all transaction threads to finish
    for (int i = 0; i < num_transactions; i++)
    {
        pthread_join(transactions[i].thread, NULL);
    }

    // Stop the timer thread
    simulation_running = 0;
    pthread_join(timer_tid, NULL);
    print_metrics(transactions, num_transactions, accounts, num_accounts);
    print_accounts_to_file("tests/post_accounts.txt");

    // Cleanup
    free(accounts);
    free(transactions);
    return 0;
}