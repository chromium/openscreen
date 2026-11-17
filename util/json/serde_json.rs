// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use serde::de::{self, DeserializeSeed, Deserializer, MapAccess, SeqAccess, Visitor};
use serde_json_lenient::de::SliceRead;
use std::fmt;
use std::pin::Pin;
use std::result::Result;

// NOTE: It would be more efficient to stream chunks of the AST for
// conversion into a jsoncpp json::Value, to reduce the number of FFI calls.
#[cxx::bridge(namespace = "openscreen::json::serde")]
mod ffi {
    unsafe extern "C++" {
        include!("util/json/json_serialization_serde_internal.h");

        type ValueBuilder;
        type ValueVisitor;

        fn set_null(self: Pin<&mut ValueBuilder>);
        fn set_bool(self: Pin<&mut ValueBuilder>, val: bool);
        fn set_i64(self: Pin<&mut ValueBuilder>, val: i64);
        fn set_u64(self: Pin<&mut ValueBuilder>, val: u64);
        fn set_f64(self: Pin<&mut ValueBuilder>, val: f64);
        fn set_string(self: Pin<&mut ValueBuilder>, val: &str);

        fn begin_array(self: Pin<&mut ValueBuilder>);
        fn end_array(self: Pin<&mut ValueBuilder>);

        fn begin_object(self: Pin<&mut ValueBuilder>);
        fn set_object_key(self: Pin<&mut ValueBuilder>, key: &str);
        fn end_object(self: Pin<&mut ValueBuilder>);

        fn visit_node(self: Pin<&mut ValueVisitor>, writer: &mut SerdeJsonWriter) -> bool;
    }

    // NOTE #1: This would be more efficient by accepting an AST of the JSON, to
    // reduce the number of FFI calls.
    //
    // NOTE #2: An alternative would be to roll our own serialization in C++,
    // which becomes feasible once jsoncpp is eliminated as a third_party
    // dependency.
    extern "Rust" {
        type SerdeJsonWriter;

        fn write_null(writer: &mut SerdeJsonWriter);
        fn write_bool(writer: &mut SerdeJsonWriter, val: bool);
        fn write_i64(writer: &mut SerdeJsonWriter, val: i64);
        fn write_u64(writer: &mut SerdeJsonWriter, val: u64);
        fn write_f64(writer: &mut SerdeJsonWriter, val: f64);
        fn write_str(writer: &mut SerdeJsonWriter, val: &str);
        fn write_array_start(writer: &mut SerdeJsonWriter);
        fn write_array_end(writer: &mut SerdeJsonWriter);
        fn write_object_start(writer: &mut SerdeJsonWriter);
        fn write_object_key(writer: &mut SerdeJsonWriter, key: &str);
        fn write_object_end(writer: &mut SerdeJsonWriter);

        fn decode_json_to_builder(
            json: &[u8],
            builder: Pin<&mut ValueBuilder>,
            error_msg: &mut String,
        ) -> bool;

        fn encode_json(
            visitor: Pin<&mut ValueVisitor>,
            pretty: bool,
            output: &mut String,
            error_msg: &mut String,
        ) -> bool;
    }
}

pub struct SerdeJsonWriter {
    buffer: String,
    pretty: bool,
    indent_level: usize,
    needs_comma_stack: Vec<bool>,
    is_object_key_pending: bool,
}

impl SerdeJsonWriter {
    fn new(pretty: bool) -> Self {
        Self {
            buffer: String::new(),
            pretty,
            indent_level: 0,
            needs_comma_stack: Vec::new(),
            is_object_key_pending: false,
        }
    }

    fn write_prefix_for_value(&mut self) {
        if self.is_object_key_pending {
            self.is_object_key_pending = false;
            return;
        }
        if let Some(needs_comma) = self.needs_comma_stack.last_mut() {
            if *needs_comma {
                self.buffer.push(',');
                if self.pretty {
                    self.buffer.push('\n');
                    self.indent();
                }
            } else {
                *needs_comma = true;
                if self.pretty {
                    self.buffer.push('\n');
                    self.indent();
                }
            }
        }
    }

    fn indent(&mut self) {
        for _ in 0..self.indent_level {
            self.buffer.push_str("   ");
        }
    }
}

fn write_null(writer: &mut SerdeJsonWriter) {
    writer.write_prefix_for_value();
    writer.buffer.push_str("null");
}

fn write_bool(writer: &mut SerdeJsonWriter, val: bool) {
    writer.write_prefix_for_value();
    writer.buffer.push_str(if val { "true" } else { "false" });
}

fn write_i64(writer: &mut SerdeJsonWriter, val: i64) {
    writer.write_prefix_for_value();
    writer.buffer.push_str(&val.to_string());
}

fn write_u64(writer: &mut SerdeJsonWriter, val: u64) {
    writer.write_prefix_for_value();
    writer.buffer.push_str(&val.to_string());
}

fn write_f64(writer: &mut SerdeJsonWriter, val: f64) {
    writer.write_prefix_for_value();
    if val.is_nan() || val.is_infinite() {
        writer.buffer.push_str("null");
    } else {
        let s = val.to_string();
        writer.buffer.push_str(&s);
        if !s.contains('.') && !s.contains('e') && !s.contains('E') {
            writer.buffer.push_str(".0");
        }
    }
}

fn write_str(writer: &mut SerdeJsonWriter, val: &str) {
    writer.write_prefix_for_value();
    if let Ok(encoded) = serde_json_lenient::to_string(val) {
        writer.buffer.push_str(&encoded);
    } else {
        writer.buffer.push_str("\"\"");
    }
}

fn write_array_start(writer: &mut SerdeJsonWriter) {
    writer.write_prefix_for_value();
    writer.buffer.push('[');
    writer.indent_level += 1;
    writer.needs_comma_stack.push(false);
}

fn write_array_end(writer: &mut SerdeJsonWriter) {
    let had_items = writer.needs_comma_stack.pop().unwrap_or(false);
    writer.indent_level = writer.indent_level.saturating_sub(1);
    if had_items && writer.pretty {
        writer.buffer.push('\n');
        writer.indent();
    }
    writer.buffer.push(']');
}

fn write_object_start(writer: &mut SerdeJsonWriter) {
    writer.write_prefix_for_value();
    writer.buffer.push('{');
    writer.indent_level += 1;
    writer.needs_comma_stack.push(false);
}

fn write_object_key(writer: &mut SerdeJsonWriter, key: &str) {
    if let Some(needs_comma) = writer.needs_comma_stack.last_mut() {
        if *needs_comma {
            writer.buffer.push(',');
        } else {
            *needs_comma = true;
        }
        if writer.pretty {
            writer.buffer.push('\n');
            writer.indent();
        }
    }
    if let Ok(encoded) = serde_json_lenient::to_string(key) {
        writer.buffer.push_str(&encoded);
    } else {
        writer.buffer.push_str("\"\"");
    }
    if writer.pretty {
        writer.buffer.push_str(": ");
    } else {
        writer.buffer.push(':');
    }
    writer.is_object_key_pending = true;
}

fn write_object_end(writer: &mut SerdeJsonWriter) {
    let had_items = writer.needs_comma_stack.pop().unwrap_or(false);
    writer.indent_level = writer.indent_level.saturating_sub(1);
    if had_items && writer.pretty {
        writer.buffer.push('\n');
        writer.indent();
    }
    writer.buffer.push('}');
}

fn encode_json(
    mut visitor: Pin<&mut ffi::ValueVisitor>,
    pretty: bool,
    output: &mut String,
    error_msg: &mut String,
) -> bool {
    let mut writer = SerdeJsonWriter::new(pretty);
    if visitor.as_mut().visit_node(&mut writer) {
        *output = writer.buffer;
        true
    } else {
        *error_msg = "Serialization traversal failed".to_string();
        false
    }
}

struct JsonVisitor<'a, 'b> {
    builder: &'a mut Pin<&'b mut ffi::ValueBuilder>,
}

impl<'de, 'a, 'b> Visitor<'de> for JsonVisitor<'a, 'b> {
    type Value = ();

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("any valid JSON value")
    }

    fn visit_bool<E>(self, v: bool) -> Result<(), E>
    where
        E: de::Error,
    {
        self.builder.as_mut().set_bool(v);
        Ok(())
    }

    fn visit_i64<E>(self, v: i64) -> Result<(), E>
    where
        E: de::Error,
    {
        self.builder.as_mut().set_i64(v);
        Ok(())
    }

    fn visit_u64<E>(self, v: u64) -> Result<(), E>
    where
        E: de::Error,
    {
        if v <= i64::MAX as u64 {
            self.builder.as_mut().set_i64(v as i64);
        } else {
            self.builder.as_mut().set_u64(v);
        }
        Ok(())
    }

    fn visit_f64<E>(self, v: f64) -> Result<(), E>
    where
        E: de::Error,
    {
        self.builder.as_mut().set_f64(v);
        Ok(())
    }

    fn visit_str<E>(self, v: &str) -> Result<(), E>
    where
        E: de::Error,
    {
        self.builder.as_mut().set_string(v);
        Ok(())
    }

    fn visit_unit<E>(self) -> Result<(), E>
    where
        E: de::Error,
    {
        self.builder.as_mut().set_null();
        Ok(())
    }

    fn visit_seq<A>(self, mut seq: A) -> Result<(), A::Error>
    where
        A: SeqAccess<'de>,
    {
        self.builder.as_mut().begin_array();
        while let Some(()) = seq.next_element_seed(JsonSeed { builder: self.builder })? {}
        self.builder.as_mut().end_array();
        Ok(())
    }

    fn visit_map<M>(self, mut access: M) -> Result<(), M::Error>
    where
        M: MapAccess<'de>,
    {
        self.builder.as_mut().begin_object();
        while let Some(()) = access.next_key_seed(KeySeed { builder: self.builder })? {
            access.next_value_seed(JsonSeed { builder: self.builder })?;
        }
        self.builder.as_mut().end_object();
        Ok(())
    }
}

// Passes an object key to the builder. Keys must not be read with
// next_key::<&str>(): that only works when serde can borrow the key from the
// input, so it fails for any key with an escape sequence (for example "a\nb"),
// which serde_json has to unescape into a temporary buffer.
struct KeySeed<'a, 'b> {
    builder: &'a mut Pin<&'b mut ffi::ValueBuilder>,
}

impl<'de, 'a, 'b> DeserializeSeed<'de> for KeySeed<'a, 'b> {
    type Value = ();

    fn deserialize<D>(self, deserializer: D) -> Result<(), D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_str(KeyVisitor { builder: self.builder })
    }
}

struct KeyVisitor<'a, 'b> {
    builder: &'a mut Pin<&'b mut ffi::ValueBuilder>,
}

impl<'de, 'a, 'b> Visitor<'de> for KeyVisitor<'a, 'b> {
    type Value = ();

    fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        formatter.write_str("an object key")
    }

    // Borrowed and owned keys both arrive here.
    fn visit_str<E>(self, v: &str) -> Result<(), E>
    where
        E: de::Error,
    {
        self.builder.as_mut().set_object_key(v);
        Ok(())
    }
}

struct JsonSeed<'a, 'b> {
    builder: &'a mut Pin<&'b mut ffi::ValueBuilder>,
}

impl<'de, 'a, 'b> DeserializeSeed<'de> for JsonSeed<'a, 'b> {
    type Value = ();

    fn deserialize<D>(self, deserializer: D) -> Result<(), D::Error>
    where
        D: Deserializer<'de>,
    {
        deserializer.deserialize_any(JsonVisitor { builder: self.builder })
    }
}

fn decode_json_to_builder(
    json: &[u8],
    mut builder: Pin<&mut ffi::ValueBuilder>,
    error_msg: &mut String,
) -> bool {
    let mut deserializer = serde_json_lenient::Deserializer::new(SliceRead::new(
        json, false, // replace_invalid_characters
        false, // allow_newlines
        false, // allow_control_chars
        false, // allow_vert_tab
        false, // allow_x_escapes
    ));

    let result = deserializer.deserialize_any(JsonVisitor { builder: &mut builder });
    match result.and(deserializer.end()) {
        Ok(()) => true,
        Err(err) => {
            *error_msg = err.to_string();
            false
        }
    }
}
