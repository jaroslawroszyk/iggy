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

package tcp_test

import (
	iggcon "github.com/apache/iggy/foreign/go/contracts"
	. "github.com/onsi/ginkgo/v2"
)

var _ = Describe("CREATE STREAM:", func() {
	When("User is logged in", func() {
		Context("and tries to create stream with unique name and id", func() {
			client := createAuthorizedConnection()
			streamId := int(createRandomUInt32())
			name := createRandomString(32)

			err := client.CreateStream(iggcon.CreateStreamRequest{
				StreamId: streamId,
				Name:     name,
			})
			defer deleteStreamAfterTests(streamId, client)

			itShouldNotReturnError(err)
			itShouldSuccessfullyCreateStream(streamId, name, client)
		})

		Context("and tries to create stream with duplicate stream name", func() {
			client := createAuthorizedConnection()
			streamId := int(createRandomUInt32())
			name := createRandomString(32)

			err := client.CreateStream(iggcon.CreateStreamRequest{
				StreamId: streamId,
				Name:     name,
			})
			defer deleteStreamAfterTests(streamId, client)

			itShouldNotReturnError(err)
			itShouldSuccessfullyCreateStream(streamId, name, client)

			err = client.CreateStream(iggcon.CreateStreamRequest{
				StreamId: int(createRandomUInt32()),
				Name:     name,
			})

			itShouldReturnSpecificError(err, "stream_name_already_exists")
		})

		Context("and tries to create stream with duplicate stream id", func() {
			client := createAuthorizedConnection()
			streamId := int(createRandomUInt32())
			name := createRandomString(32)

			err := client.CreateStream(iggcon.CreateStreamRequest{
				StreamId: streamId,
				Name:     name,
			})
			defer deleteStreamAfterTests(streamId, client)

			itShouldNotReturnError(err)
			itShouldSuccessfullyCreateStream(streamId, name, client)

			err = client.CreateStream(iggcon.CreateStreamRequest{
				StreamId: streamId,
				Name:     createRandomString(32),
			})

			itShouldReturnSpecificError(err, "stream_id_already_exists")
		})

		Context("and tries to create stream name that's over 255 characters", func() {
			client := createAuthorizedConnection()
			streamId := int(createRandomUInt32())
			name := createRandomString(256)

			err := client.CreateStream(iggcon.CreateStreamRequest{
				StreamId: streamId,
				Name:     name,
			})

			itShouldReturnSpecificError(err, "stream_name_too_long")
		})
	})

	When("User is not logged in", func() {
		Context("and tries to create stream", func() {
			client := createConnection()
			err := client.CreateStream(iggcon.CreateStreamRequest{
				StreamId: int(createRandomUInt32()),
				Name:     createRandomString(32),
			})

			itShouldReturnUnauthenticatedError(err)
		})
	})
})
