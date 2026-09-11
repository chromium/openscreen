// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use simple_dns::rdata::RData;
use simple_dns::{Name, Packet, PacketFlag, Question, ResourceRecord, CLASS};

#[cxx::bridge(namespace = "openscreen::discovery")]
pub mod ffi {
    #[derive(Debug, Default, Clone)]
    pub struct DnsHeader {
        pub id: u16,
        pub flags: u16,
        pub is_response: bool,
        pub is_truncated: bool,
        pub is_authoritative: bool,
        pub question_count: u16,
        pub answer_count: u16,
        pub authority_count: u16,
        pub additional_count: u16,
    }

    #[derive(Debug, Default, Clone)]
    pub struct DnsQuestion {
        pub name_labels: Vec<String>,
        pub qtype: u16,
        pub qclass: u16,
        pub is_unicast_response: bool,
    }

    #[derive(Debug, Default, Clone)]
    pub struct DnsRecord {
        pub name_labels: Vec<String>,
        pub rtype: u16,
        pub rclass: u16,
        pub is_cache_flush: bool,
        pub ttl_seconds: u32,
        pub rdata_type: u16,
        pub rdata_bytes: Vec<u8>,
        pub ptr_target_labels: Vec<String>,
        pub srv_priority: u16,
        pub srv_weight: u16,
        pub srv_port: u16,
        pub srv_target_labels: Vec<String>,
        pub nsec_next_labels: Vec<String>,
    }

    #[derive(Debug, Default, Clone)]
    pub struct DnsMessage {
        pub header: DnsHeader,
        pub questions: Vec<DnsQuestion>,
        pub answers: Vec<DnsRecord>,
        pub authority_records: Vec<DnsRecord>,
        pub additional_records: Vec<DnsRecord>,
    }

    extern "Rust" {
        fn parse_header(buffer: &[u8], header: &mut DnsHeader) -> bool;
        fn parse_message(buffer: &[u8], message: &mut DnsMessage) -> bool;
    }
}

fn name_to_labels(name: &Name) -> Vec<String> {
    name.iter().map(|l| l.to_string()).collect()
}

fn class_to_u16(class: CLASS) -> u16 {
    match class {
        CLASS::IN => 1,
        CLASS::CS => 2,
        CLASS::CH => 3,
        CLASS::HS => 4,
        CLASS::NONE => 254,
    }
}

fn convert_question(q: &Question) -> ffi::DnsQuestion {
    let qclass_raw: u16 = q.qclass.into();
    ffi::DnsQuestion {
        name_labels: name_to_labels(&q.qname),
        qtype: q.qtype.into(),
        qclass: qclass_raw & 0x7FFF,
        is_unicast_response: q.unicast_response,
    }
}

fn convert_record(rr: &ResourceRecord) -> ffi::DnsRecord {
    let rclass_raw = class_to_u16(rr.class);
    let rtype_raw: u16 = rr.rdata.type_code().into();
    let mut record = ffi::DnsRecord {
        name_labels: name_to_labels(&rr.name),
        rtype: rtype_raw,
        rclass: rclass_raw & 0x7FFF,
        is_cache_flush: rr.cache_flush,
        ttl_seconds: rr.ttl,
        rdata_type: rtype_raw,
        ..Default::default()
    };

    match &rr.rdata {
        RData::A(a) => {
            record.rdata_bytes = a.address.to_be_bytes().to_vec();
        }
        RData::AAAA(aaaa) => {
            record.rdata_bytes = aaaa.address.to_be_bytes().to_vec();
        }
        RData::PTR(ptr) => {
            record.ptr_target_labels = name_to_labels(&ptr.0);
        }
        RData::TXT(txt) => {
            for (key, val) in txt.iter_raw() {
                let entry_len = if let Some(v) = val { key.len() + 1 + v.len() } else { key.len() };
                let entry_len: u8 = entry_len.try_into().expect("value doesn't fit in u8");
                record.rdata_bytes.push(entry_len);
                record.rdata_bytes.extend_from_slice(key);
                if let Some(v) = val {
                    record.rdata_bytes.push(b'=');
                    record.rdata_bytes.extend_from_slice(v);
                }
            }
        }
        RData::SRV(srv) => {
            record.srv_priority = srv.priority;
            record.srv_weight = srv.weight;
            record.srv_port = srv.port;
            record.srv_target_labels = name_to_labels(&srv.target);
        }
        RData::NSEC(nsec) => {
            record.nsec_next_labels = name_to_labels(&nsec.next_name);
            for map in &nsec.type_bit_maps {
                record.rdata_bytes.push(map.window_block);
                record.rdata_bytes.push(map.bitmap.len() as u8);
                record.rdata_bytes.extend_from_slice(map.bitmap.as_ref());
            }
        }
        RData::NULL(_, null_data) => {
            record.rdata_bytes = null_data.get_data().to_vec();
        }
        _ => {}
    }

    record
}

pub fn parse_header(buffer: &[u8], header: &mut ffi::DnsHeader) -> bool {
    let Ok(packet) = Packet::parse(buffer) else {
        return false;
    };
    *header = ffi::DnsHeader {
        id: packet.id(),
        flags: 0,
        is_response: packet.has_flags(PacketFlag::RESPONSE),
        is_truncated: packet.has_flags(PacketFlag::TRUNCATION),
        is_authoritative: packet.has_flags(PacketFlag::AUTHORITATIVE_ANSWER),
        question_count: packet.questions.len() as u16,
        answer_count: packet.answers.len() as u16,
        authority_count: packet.name_servers.len() as u16,
        additional_count: packet.additional_records.len() as u16,
    };
    true
}

pub fn parse_message(buffer: &[u8], message: &mut ffi::DnsMessage) -> bool {
    let Ok(packet) = Packet::parse(buffer) else {
        return false;
    };

    let mut header = ffi::DnsHeader::default();
    parse_header(buffer, &mut header);

    let questions: Vec<ffi::DnsQuestion> = packet.questions.iter().map(convert_question).collect();
    let answers: Vec<ffi::DnsRecord> = packet.answers.iter().map(convert_record).collect();
    let authority_records: Vec<ffi::DnsRecord> =
        packet.name_servers.iter().map(convert_record).collect();
    let additional_records: Vec<ffi::DnsRecord> =
        packet.additional_records.iter().map(convert_record).collect();

    *message =
        ffi::DnsMessage { header, questions, answers, authority_records, additional_records };

    true
}
