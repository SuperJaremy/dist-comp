#include "ipc_banking.h"
#include "banking.h"
#include "ipc.h"
#include "ipc_parent.h"
#include "ipc_proc.h"

#include <stdlib.h>
#include <string.h>

BalanceState balance_state_init(balance_t balance, timestamp_t timestamp) {
	BalanceState state = {0};
	state.s_balance = balance;
	state.s_time = timestamp;
	return state;
}

BalanceHistory *balance_history_init(local_id id, balance_t init_balance) {
	BalanceHistory *history = malloc(sizeof(BalanceHistory));
	history->s_id = id;
	history->s_history_len = 1;
	history->s_history[0] = balance_state_init(init_balance, 0);
	return history;
}

void balance_history_add_state(BalanceHistory *history, timestamp_t timestamp, balance_t balance) {
	uint8_t history_len = history->s_history_len;
	balance_t last_balance = history->s_history[history_len - 1].s_balance;
	for(; history_len < timestamp; history_len++) {
		history->s_history[history_len] = balance_state_init(last_balance, history_len);
	}
	history->s_history[timestamp] = balance_state_init(balance, timestamp);
	history->s_history_len = timestamp + 1;
}

void transfer(void * parent_data, local_id src, local_id dst, balance_t amount) {
	struct ipc_parent *parent = parent_data;
	timestamp_t time = get_physical_time();
	TransferOrder order = {
		.s_dst = dst,
		.s_src = src,
		.s_amount = amount
	};
	Message *m = malloc(sizeof(Message));
	m->s_header.s_magic = MESSAGE_MAGIC;
	m->s_header.s_type = TRANSFER;
	m->s_header.s_local_time = time;
	m->s_header.s_payload_len = sizeof(order);
	memcpy(m->s_payload, &order, sizeof(order));
	send(parent->ipc_proc, src, m);
	while(receive(parent->ipc_proc, dst, m) == NO_READ);
}
