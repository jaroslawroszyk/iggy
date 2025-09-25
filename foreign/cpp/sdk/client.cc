// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.
#include "client.h"
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using icp::model::message::PolledMessages;

icp::client::Client::Client(const Options& options) {
	// to make more natural interface for setting options we use a struct, so need to validate it.
	options.validate();
	std::ostringstream addr;
	addr << options.hostname << ":" << options.port;
	this->handle = iggy_client_new_tcp(addr.str().c_str());
	if (this->handle == nullptr) {
		throw std::runtime_error("Failed to initialize Iggy client (TCP)");
	}
}

icp::client::Client::~Client() {
	if (this->handle != nullptr) {
		iggy_client_free(this->handle);
		this->handle = nullptr;
	}
}

void icp::client::Client::ping() {
	// TODO: expose ping via FFI if needed; for now, a no-op that validates connectivity by constructing the client.
	if (this->handle == nullptr) {
		throw std::runtime_error("Client not initialized");
	}
}

icp::model::sys::Stats icp::client::Client::getStats() {
	// minimal placeholder
	static const std::string empty;
	return icp::model::sys::Stats(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, empty, empty, empty, empty);
}

void icp::client::Client::send(const std::string& stream_id,
                               const std::string& topic_id,
                               uint32_t partition_id,
                               const std::vector<std::vector<unsigned char>>& payloads) {
	if (this->handle == nullptr) {
		throw std::runtime_error("Client not initialized");
	}
	std::vector<IggyFfiMessage> msgs;
	msgs.reserve(payloads.size());
	for (const auto& p : payloads) {
		IggyFfiMessage m{reinterpret_cast<const uint8_t*>(p.data()), p.size()};
		msgs.push_back(m);
	}
	int rc = iggy_send_messages(this->handle,
	                            stream_id.c_str(),
	                            topic_id.c_str(),
	                            partition_id,
	                            msgs.data(),
	                            msgs.size());
	if (rc != 0) {
		throw std::runtime_error("iggy_send_messages failed");
	}
}

std::vector<std::vector<unsigned char>> icp::client::Client::poll(const std::string& stream_id,
                                                                  const std::string& topic_id,
                                                                  uint32_t count,
                                                                  const std::optional<uint32_t>& partition_id,
                                                                  bool auto_commit) {
	if (this->handle == nullptr) {
		throw std::runtime_error("Client not initialized");
	}
	IggyFfiPolledBatch batch{};
	uint32_t strategy_kind_next = 3; // NEXT
	uint64_t strategy_value_unused = 0;
	uint32_t consumer_kind = 1; // CONSUMER
	uint32_t consumer_id = 0;   // default consumer id
	int32_t part_opt = partition_id.has_value() ? static_cast<int32_t>(partition_id.value()) : -1;
	int rc = iggy_poll_messages(this->handle,
	                           stream_id.c_str(),
	                           topic_id.c_str(),
	                           part_opt,
	                           consumer_kind,
	                           consumer_id,
	                           strategy_kind_next,
	                           strategy_value_unused,
	                           count,
	                           auto_commit,
	                           &batch);
	if (rc != 0) {
		throw std::runtime_error("iggy_poll_messages failed");
	}

	std::vector<std::vector<unsigned char>> out;
	out.reserve(batch.messages_len);
	for (size_t i = 0; i < batch.messages_len; ++i) {
		const IggyFfiPolledMessage& m = batch.messages_ptr[i];
		std::vector<unsigned char> payload;
		payload.resize(m.payload_len);
		std::memcpy(payload.data(), m.payload_ptr, m.payload_len);
		out.push_back(std::move(payload));
	}
	iggy_poll_free(&batch);
	return out;
}
