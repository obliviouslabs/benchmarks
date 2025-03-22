#![allow(warnings)]
use rand::Rng;
use rods_oram::{
  circuit_oram::CircuitORAM, linear_oram::LinearORAM, recursive_oram::RecursivePositionMap,
};
use rods_datastructures::array::DynamicArray;
pub mod common;
use common::{current_time_ns, get_mem_value, run_test_forked};
use std::hint::black_box;
use rods_primitives::{
  cmov_body, impl_cmov_for_pod,
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
