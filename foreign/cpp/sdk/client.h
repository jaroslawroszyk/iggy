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
#pragma once

#include <sodium.h>
#include <stdexcept>
#include <string>
#include <optional>
#include <vector>
#include "model.h"
#include "net/iggy.h"
#include "net/transport.h"
#include "ffi.h"

namespace icp {
namespace client {

/**
 * @class Credentials
 * @brief Iggy server login credentials: username and password for now.
 *
 * This class provides a secure holder for the password; when destroyed, it zeroes of the memory.
 */
class Credentials {
private:
    std::string username;
    std::string password;

public:
    Credentials(const std::string& username, const std::string& password)
        : username(username)
        , password(password) {}

    ~Credentials() { sodium_memzero(&password[0], password.size()); }
};

/**
 * @struct Options
 * @brief A struct to hold various options.
 *
 * This struct is used to hold various options and settings
 * that are used throughout the application. Each field in the
 * struct represents a different option.
 */
struct Options {
    /**
     * @brief The hostname of the Iggy server.
     */
    std::string hostname = "localhost";

    /**
     * @brief The port the Iggy server is listening on; default depends on transport. Defaults to the DEFAULT_TCP_PORT.
     */
    unsigned short port = icp::net::DEFAULT_TCP_PORT;

    /**
     * @brief The network transport to use when connecting to the server. Defaults to TCP.
     */
    icp::net::transport::Transport transport = icp::net::transport::Transport::TCP;

    /**
     * @brief The user credentials to use when connecting to the server.
     *
     * Note that the default credentials are built-in and are not secure. We recommend that you create a new
     * admin user with a strong password and then deactivate this user.
     */
    client::Credentials credentials = client::Credentials("iggy", "iggy");

    void validate() const {
        if (hostname.empty()) {
            throw std::invalid_argument("Hostname cannot be empty");
        }
    }
};

/**
 * @class Client
 * @brief Flexible client for connecting to the Iggy server.
 *
 * Main Iggy C++ client. You can use Options to configure the endpoint, credentials and transport.
 */
class Client {
public:
    explicit Client(const Options& options);
    ~Client();

    /**
     * @brief Send a synchronous ping to the server to check if it is alive.
     */
    void ping();

    /**
     * @brief Get the Iggy server's performance statistics.
     */
    icp::model::sys::Stats getStats();

    /**
     * @brief Send messages to a stream/topic and partition.
     */
    void send(const std::string& stream_id,
              const std::string& topic_id,
              uint32_t partition_id,
              const std::vector<icp::model::message::Message>& messages);

    /**
     * @brief Poll messages using a simple strategy.
     * @param partition_id Optional partition id; pass std::nullopt for server side routing
     * @return PolledMessages batch
     */
    icp::model::message::PolledMessages poll(const std::string& stream_id,
                                             const std::string& topic_id,
                                             const std::optional<uint32_t>& partition_id,
                                             icp::model::shared::Consumer consumer,
                                             uint32_t strategy_kind,
                                             uint64_t strategy_value,
                                             uint32_t count,
                                             bool auto_commit);

private:
    IggyFfiClient* handle = nullptr;
};
};  // namespace client
};  // namespace icp
