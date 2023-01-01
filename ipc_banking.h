#ifndef __IPC_BANKING_H__
#define __IPC_BANKING_H__

#include "banking.h"

#include "ipc.h"

BalanceState balance_state_init(balance_t balance, timestamp_t timestamp);

BalanceHistory *balance_history_init(local_id id, balance_t init_balance);

void balance_history_add_state(BalanceHistory *history, timestamp_t timestamp, balance_t balance);

#endif
