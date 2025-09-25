#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IggyFfiClient IggyFfiClient;

typedef struct {
  const uint8_t* ptr;
  size_t len;
} IggyFfiMessage;

typedef struct {
  const uint8_t* payload_ptr;
  size_t payload_len;
  uint64_t offset;
} IggyFfiPolledMessage;

typedef struct {
  uint32_t partition_id;
  uint64_t current_offset;
  const IggyFfiPolledMessage* messages_ptr;
  size_t messages_len;
} IggyFfiPolledBatch;

IggyFfiClient* iggy_client_new_tcp(const char* addr);
void iggy_client_free(IggyFfiClient* client);

int iggy_send_messages(
  IggyFfiClient* client,
  const char* stream,
  const char* topic,
  uint32_t partition_id,
  const IggyFfiMessage* messages_ptr,
  size_t messages_len
);

int iggy_poll_messages(
  IggyFfiClient* client,
  const char* stream,
  const char* topic,
  int32_t partition_id_opt,
  uint32_t consumer_kind,
  uint32_t consumer_id,
  uint32_t strategy_kind,
  uint64_t strategy_value,
  uint32_t count,
  bool auto_commit,
  IggyFfiPolledBatch* out_batch
);

void iggy_poll_free(IggyFfiPolledBatch* batch);

#ifdef __cplusplus
}
#endif 