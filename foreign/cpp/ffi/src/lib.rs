// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements. See the NOTICE file
// distributed with this work for additional information.
// The ASF licenses this file to you under the Apache License, Version 2.0.

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint, c_ulonglong};
use std::ptr;
use std::sync::Arc;

use bytes::Bytes;
use iggy::prelude::*;

#[repr(C)]
pub struct IggyFfiClient {
    inner: Arc<IggyClient>,
}

#[repr(C)]
pub struct IggyFfiMessage {
    ptr: *const u8,
    len: usize,
}

#[repr(C)]
pub struct IggyFfiPolledMessage {
    // minimal: return a pointer/len to payload; no headers for MVP
    payload_ptr: *const u8,
    payload_len: usize,
    // server-provided metadata
    offset: c_ulonglong,
}

#[repr(C)]
pub struct IggyFfiPolledBatch {
    partition_id: c_uint,
    current_offset: c_ulonglong,
    messages_ptr: *const IggyFfiPolledMessage,
    messages_len: usize,
}

#[no_mangle]
pub extern "C" fn iggy_client_new_tcp(addr: *const c_char) -> *mut IggyFfiClient {
    if addr.is_null() {
        return ptr::null_mut();
    }
    let c_str = unsafe { CStr::from_ptr(addr) };
    let addr = match c_str.to_str() {
        Ok(s) => s.to_string(),
        Err(_) => return ptr::null_mut(),
    };
    let client = IggyClientBuilder::new()
        .with_tcp()
        .with_server_address(addr)
        .build();
    match client {
        Ok(c) => Box::into_raw(Box::new(IggyFfiClient { inner: Arc::new(c) })),
        Err(_) => ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn iggy_client_free(client: *mut IggyFfiClient) {
    if client.is_null() {
        return;
    }
    unsafe { drop(Box::from_raw(client)) };
}

#[no_mangle]
pub extern "C" fn iggy_send_messages(
    client: *mut IggyFfiClient,
    stream_id_str: *const c_char,
    topic_id_str: *const c_char,
    partition_id: c_uint,
    messages_ptr: *const IggyFfiMessage,
    messages_len: usize,
) -> c_int {
    if client.is_null() || stream_id_str.is_null() || topic_id_str.is_null() {
        return -1;
    }
    let client = unsafe { &*client };
    let stream = unsafe { CStr::from_ptr(stream_id_str) };
    let topic = unsafe { CStr::from_ptr(topic_id_str) };

    let stream = match stream.to_str() { Ok(s) => Identifier::from(s), Err(_) => return -2 };
    let topic = match topic.to_str() { Ok(s) => Identifier::from(s), Err(_) => return -3 };
    let partitioning = Partitioning::partition_id(partition_id);

    let mut msgs: Vec<IggyMessage> = Vec::with_capacity(messages_len);
    if messages_len > 0 && !messages_ptr.is_null() {
        let slice = unsafe { std::slice::from_raw_parts(messages_ptr, messages_len) };
        for m in slice {
            if m.ptr.is_null() { return -4; }
            let data = unsafe { std::slice::from_raw_parts(m.ptr, m.len) };
            let msg = IggyMessage::from(Bytes::copy_from_slice(data));
            msgs.push(msg);
        }
    }

    let inner = client.inner.clone();
    // run on a temporary Tokio runtime for simplicity (blocking)
    let rt = tokio::runtime::Runtime::new().unwrap();
    let result = rt.block_on(async move { inner.send_messages(&stream, &topic, &partitioning, msgs.as_mut_slice()).await });
    match result {
        Ok(_) => 0,
        Err(_) => -10,
    }
}

#[no_mangle]
pub extern "C" fn iggy_poll_messages(
    client: *mut IggyFfiClient,
    stream_id_str: *const c_char,
    topic_id_str: *const c_char,
    partition_id_opt: c_int, // -1 = None, otherwise Some(partition_id)
    consumer_kind: c_uint,   // 1 = CONSUMER, 2 = CONSUMER_GROUP
    consumer_id: c_uint,
    strategy_kind: c_uint,   // 1 = offset, 2 = timestamp, 3 = next, 4 = earliest, 5 = last
    strategy_value: c_ulonglong, // only used for offset/timestamp
    count: c_uint,
    auto_commit: bool,
    out_batch: *mut IggyFfiPolledBatch,
) -> c_int {
    if client.is_null() || stream_id_str.is_null() || topic_id_str.is_null() || out_batch.is_null() {
        return -1;
    }
    let client = unsafe { &*client };
    let stream_c = unsafe { CStr::from_ptr(stream_id_str) };
    let topic_c = unsafe { CStr::from_ptr(topic_id_str) };
    let stream = match stream_c.to_str() { Ok(s) => Identifier::from(s), Err(_) => return -2 };
    let topic = match topic_c.to_str() { Ok(s) => Identifier::from(s), Err(_) => return -3 };

    let partition_id = if partition_id_opt < 0 { None } else { Some(partition_id_opt as u32) };

    let consumer = match consumer_kind {
        1 => Consumer::consumer(consumer_id),
        2 => Consumer::consumer_group(consumer_id),
        _ => return -4,
    };

    let strategy = match strategy_kind {
        1 => PollingStrategy::Offset { value: strategy_value },
        2 => PollingStrategy::Timestamp { value: strategy_value },
        3 => PollingStrategy::Next,
        4 => PollingStrategy::Earliest,
        5 => PollingStrategy::Last,
        _ => return -5,
    };

    let inner = client.inner.clone();
    let rt = tokio::runtime::Runtime::new().unwrap();
    let result = rt.block_on(async move {
        inner
            .poll_messages(&stream, &topic, partition_id, &consumer, &strategy, count, auto_commit)
            .await
    });

    let polled = match result {
        Ok(p) => p,
        Err(_) => return -10,
    };

    // allocate C-friendly array for messages; caller owns and must free via iggy_poll_free
    let mut ffi_messages: Vec<IggyFfiPolledMessage> = Vec::with_capacity(polled.messages.len());
    for m in &polled.messages {
        let payload = m.payload.clone();
        let ptr = payload.as_ptr();
        let len = payload.len();
        // leak the Bytes to keep memory alive; freed in iggy_poll_free
        std::mem::forget(payload);
        ffi_messages.push(IggyFfiPolledMessage { payload_ptr: ptr, payload_len: len, offset: m.offset.unwrap_or(0) });
    }
    let msgs_ptr = ffi_messages.as_ptr();
    let msgs_len = ffi_messages.len();
    // leak vector; freed in iggy_poll_free
    std::mem::forget(ffi_messages);

    unsafe {
        (*out_batch).partition_id = polled.partition_id.unwrap_or(0);
        (*out_batch).current_offset = polled.current_offset.unwrap_or(0);
        (*out_batch).messages_ptr = msgs_ptr;
        (*out_batch).messages_len = msgs_len;
    }

    0
}

#[no_mangle]
pub extern "C" fn iggy_poll_free(batch: *mut IggyFfiPolledBatch) {
    if batch.is_null() {
        return;
    }
    let batch = unsafe { &mut *batch };
    if !batch.messages_ptr.is_null() && batch.messages_len > 0 {
        // reconstruct vector to drop and free
        let msgs = unsafe { Vec::from_raw_parts(batch.messages_ptr as *mut IggyFfiPolledMessage, batch.messages_len, batch.messages_len) };
        // messages payloads were leaked as Bytes; reconstruct and drop
        for m in msgs {
            if !m.payload_ptr.is_null() && m.payload_len > 0 {
                let slice = unsafe { std::slice::from_raw_parts(m.payload_ptr, m.payload_len) };
                let _ = Bytes::copy_from_slice(slice); // drop immediately
            }
        }
        batch.messages_ptr = ptr::null();
        batch.messages_len = 0;
    }
} 