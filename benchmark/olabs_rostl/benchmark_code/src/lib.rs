#![allow(warnings)]
use rand::Rng;
use rostl_oram::{
  circuit_oram::{Block, Bucket, CircuitORAM}, linear_oram::LinearORAM, recursive_oram::RecursivePositionMap, prelude::PositionType, heap_tree::HeapTree
};
use rostl_datastructures::array::DynamicArray;
pub mod common;
use common::{current_time_ns, get_mem_value, run_test_forked};
use std::hint::black_box;
use rostl_primitives::{
  cmov_body, cxchg_body, impl_cmov_for_pod,
  traits::{Cmov, _Cmovbase, cswap},
};
use bytemuck::{Zeroable, Pod};

#[repr(transparent)]
#[derive(Debug, Clone, Copy, Zeroable, Pod)]
struct u256([u64; 4]);
impl_cmov_for_pod!(u256);

#[repr(transparent)]
#[derive(Debug, Clone, Copy, Zeroable, Pod)]
struct u512([u64; 8]);
impl_cmov_for_pod!(u512);


#[no_mangle]
pub extern "C" fn explicit_cmov_64(a: &mut u512, b: &u512, cond: bool) {
  a.cmov(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cmov_32(a: &mut u256, b: &u256, cond: bool) {
  a.cmov(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cmov_16(a: &mut u128, b: &u128, cond: bool) {
  a.cmov(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cmov_8(a: &mut u64, b: &u64, cond: bool) {
  a.cmov(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cmov_4(a: &mut u32, b: &u32, cond: bool) {
  a.cmov(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cmov_2(a: &mut u16, b: &u16, cond: bool) {
  a.cmov(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cmov_1(a: &mut u8, b: &u8, cond: bool) {
  a.cmov(b, cond)
}

#[no_mangle]
pub extern "C" fn explicit_cxchg_64(a: &mut u512, b: &mut u512, cond: bool) {
  a.cxchg(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cxchg_32(a: &mut u256, b: &mut u256, cond: bool) {
  a.cxchg(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cxchg_16(a: &mut u128, b: &mut u128, cond: bool) {
  a.cxchg(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cxchg_8(a: &mut u64, b: &mut u64, cond: bool) {
  a.cxchg(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cxchg_4(a: &mut u32, b: &mut u32, cond: bool) {
  a.cxchg(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cxchg_2(a: &mut u16, b: &mut u16, cond: bool) {
  a.cxchg(b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cxchg_1(a: &mut u8, b: &mut u8, cond: bool) {
  a.cxchg(b, cond)
}

#[no_mangle]
pub extern "C" fn explicit_cswap_64(a: &mut u512, b: &mut u512, cond: bool) {
  cswap(a, b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cswap_32(a: &mut u256, b: &mut u256, cond: bool) {
  cswap(a, b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cswap_16(a: &mut u128, b: &mut u128, cond: bool) {
  cswap(a, b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cswap_8(a: &mut u64, b: &mut u64, cond: bool) {
  cswap(a, b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cswap_4(a: &mut u32, b: &mut u32, cond: bool) {
  cswap(a, b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cswap_2(a: &mut u16, b: &mut u16, cond: bool) {
  cswap(a, b, cond)
}
#[no_mangle]
pub extern "C" fn explicit_cswap_1(a: &mut u8, b: &mut u8, cond: bool) {
  cswap(a, b, cond)
}


#[no_mangle]
pub fn heap_tree_read_path(a: &mut HeapTree<Bucket<u64>>, b: PositionType, c: &mut [Block<u64>]) {
  a.read_path(b, c);
}