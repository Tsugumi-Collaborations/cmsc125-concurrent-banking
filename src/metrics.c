#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../include/bank.h"
#include "../include/metrics.h"

void print_accounts_to_file(const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (!file)
    {
        fprintf(stderr, "Error: could not open output file '%s'\n", filename);
        return;
    }

    fprintf(file, "# AccountID  FinalBalanceCentavos\n\n");

    for (int i = 0; i < bank.num_accounts; i++)
    {
        fprintf(file, "%-4d %d\n",
                bank.accounts[i].account_id,
                bank.accounts[i].balance_centavos);
    }

    fclose(file);
    printf("Final account balances written to '%s'\n", filename);
}

void print_metrics(Transaction *transactions, int num_transactions, Account *initial_accounts, int num_accounts)
{
    // Load initial balances from the passed accounts array
    int expected[MAX_ACCOUNTS];
    int account_ids[MAX_ACCOUNTS];

    for (int i = 0; i < num_accounts; i++)
    {
        account_ids[i] = initial_accounts[i].account_id;
        expected[i]    = initial_accounts[i].balance_centavos;
    }

    // Replay only committed transactions to compute expected final balances
    for (int t = 0; t < num_transactions; t++)
    {
        Transaction *tx = &transactions[t];
        if (tx->status != TX_COMMITTED)
            continue;

        for (int o = 0; o < tx->num_ops; o++)
        {
            Operation *op = &tx->ops[o];

            int idx = -1;
            for (int i = 0; i < num_accounts; i++)
            {
                if (account_ids[i] == op->account_id)
                {
                    idx = i;
                    break;
                }
            }

            switch (op->type)
            {
                case OP_DEPOSIT:
                    if (idx >= 0)
                        expected[idx] += op->amount_centavos;
                    break;

                case OP_WITHDRAW:
                    if (idx >= 0)
                        expected[idx] -= op->amount_centavos;
                    break;

                case OP_TRANSFER:
                {
                    int target_idx = -1;
                    for (int i = 0; i < num_accounts; i++)
                    {
                        if (account_ids[i] == op->target_account)
                        {
                            target_idx = i;
                            break;
                        }
                    }
                    if (idx >= 0)
                        expected[idx] -= op->amount_centavos;
                    if (target_idx >= 0)
                        expected[target_idx] += op->amount_centavos;
                    break;
                }

                case OP_BALANCE:
                    // Read-only, no effect on expected balance
                    break;
            }
        }
    }

    // Compare expected vs actual and report
    printf("\n=== Transaction Integrity Check ===\n");
    printf("%-12s %-16s %-16s %-8s\n",
           "AccountID", "Expected", "Actual", "Status");
    printf("%-12s %-16s %-16s %-8s\n",
           "---------", "--------", "------", "------");

    bool all_passed = true;
    for (int i = 0; i < num_accounts; i++)
    {
        int actual = bank.accounts[i].balance_centavos;
        bool passed = (actual == expected[i]);
        if (!passed)
            all_passed = false;

        printf("%-12d %-16d %-16d %s\n",
               account_ids[i],
               expected[i],
               actual,
               passed ? "OK" : "ANOMALY");
    }

    printf("\nIntegrity check: %s\n",
           all_passed ? "PASSED — all balances match expected values"
                      : "FAILED — one or more balances deviate from expected");

    // Transaction summary
    int committed = 0, aborted = 0;
    for (int t = 0; t < num_transactions; t++)
    {
        if (transactions[t].status == TX_COMMITTED) committed++;
        else if (transactions[t].status == TX_ABORTED)  aborted++;
    }

    printf("\n=== Transaction Summary ===\n");
    printf("Total transactions : %d\n", num_transactions);
    printf("Committed          : %d\n", committed);
    printf("Aborted            : %d\n", aborted);
}