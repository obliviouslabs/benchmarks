#![allow(warnings)]
use rand::Rng;
use oram::{DefaultOram, BlockValue, Oram};
pub mod common;
use common::{current_time_ns, get_mem_value, run_test_forked};
use std::hint::black_box;
