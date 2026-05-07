// metrics.h
#ifndef METRICS_H
#define METRICS_H

#include "../include/transaction.h"

void print_accounts_to_file(const char *filename);
void print_metrics(Transaction *transactions, int num_transactions, Account *initial_accounts, int num_accounts);

#endif