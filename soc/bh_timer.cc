#include "riscv/sim.h"
#include "soc/bh_timer.h"
#include "soc/bh_debug.h"

#include <memory>
#include <chrono>

const reg_t bh_timer_t::DEVICE_BASE;

bh_timer_t::bh_timer_t() {
}
bh_timer_t::~bh_timer_t() {
}
bool bh_timer_t::load (reg_t addr, size_t len, uint8_t* bytes) {
  const size_t ADDR_MTIME_LO     = 0x0;
  const size_t ADDR_MTIME_HI     = 0x4;
  const size_t ADDR_MTIMECMP_LO  = 0x8;
  const size_t ADDR_MTIMECMP_HI  = 0xC;
  const size_t ADDR_MTIMECTRL_LO = 0x10;
  const size_t ADDR_MTIMECTRL_HI = 0x14;
  const size_t ADDR_AE_BASE      = 0x18;

  if (!len)
    return false;
  if (!bytes)
    return false;
  if (!ptr_proc)
    return false;

  if ((addr >= ADDR_MTIME_LO) && (addr + len <= ADDR_MTIME_LO + 4)) {
    std::memcpy(bytes, &mtime_cur_lo, len);
  }
  else if ((addr >= ADDR_MTIME_HI) && (addr + len <= ADDR_MTIME_HI + 4)) {
    std::memcpy(bytes, &mtime_cur_hi, len);
  }
  else if ((addr >= ADDR_MTIMECMP_LO) && (addr + len <= ADDR_MTIMECMP_LO + 4)) {
    std::memcpy(bytes, &mtime_cmp_lo, len);
  }
  else if ((addr >= ADDR_MTIMECMP_HI) && (addr + len <= ADDR_MTIMECMP_HI + 4)) {
    std::memcpy(bytes, &mtime_cmp_hi, len);
  }
  else if ((addr >= ADDR_MTIMECTRL_LO) && (addr + len <= ADDR_MTIMECTRL_LO + 4)) {
    std::memcpy(bytes, &mtime_inc_lo, len);
  }
  else if ((addr >= ADDR_MTIMECTRL_HI) && (addr + len <= ADDR_MTIMECTRL_HI + 4)) {
    std::memcpy(bytes, &mtime_inc_hi, len);
  }
  else if ((addr >= ADDR_AE_BASE) && (addr + len <= ADDR_AE_BASE + 8)) {
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t nanos =
      std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch())
        .count();
    std::memcpy(bytes, (uint8_t*)&nanos + (addr - ADDR_AE_BASE), len);
  }
  else {
    return false;
  }
  return true;
}
bool bh_timer_t::store (reg_t addr, size_t len, const uint8_t* bytes) {
  const size_t ADDR_MTIME_LO     = 0x0;
  const size_t ADDR_MTIME_HI     = 0x4;
  const size_t ADDR_MTIMECMP_LO  = 0x8;
  const size_t ADDR_MTIMECMP_HI  = 0xC;
  const size_t ADDR_MTIMECTRL_LO = 0x10;
  const size_t ADDR_MTIMECTRL_HI = 0x14;
  const size_t ADDR_AE_BASE      = 0x18;
  
  uint64_t mtime_cur = ((uint64_t)mtime_cur_hi << 32) | mtime_cur_lo;
  uint64_t mtime_cmp = ((uint64_t)mtime_cmp_hi << 32) | mtime_cmp_lo;
  
  if (!len)
    return false;
  if (!bytes)
    return false;
  if (!ptr_proc)
    return false;

  if ((addr >= ADDR_MTIME_LO) && (addr + len <= ADDR_MTIME_LO + 4)) {
    std::memcpy(&mtime_cur_lo, bytes, len);
    return true;
  }
  else if ((addr >= ADDR_MTIME_HI) && (addr + len <= ADDR_MTIME_HI + 4)) {
    std::memcpy(&mtime_cur_hi, bytes, len);
    return true;
  }
  else if ((addr >= ADDR_MTIMECMP_LO) && (addr + len <= ADDR_MTIMECMP_LO + 4)) {
    std::memcpy(&mtime_cmp_lo, bytes, len);
    return true;
  }
  else if ((addr >= ADDR_MTIMECMP_HI) && (addr + len <= ADDR_MTIMECMP_HI + 4)) {
    std::memcpy(&mtime_cmp_hi, bytes, len);
    return true;
  }
  else if ((addr >= ADDR_MTIMECTRL_LO) && (addr + len <= ADDR_MTIMECTRL_LO + 4)) {
    std::memcpy(&mtime_inc_lo, bytes, len);
    return true;
  }
  else if ((addr >= ADDR_MTIMECTRL_HI) && (addr + len <= ADDR_MTIMECTRL_HI + 4)) {
    std::memcpy(&mtime_inc_hi, bytes, len);
    return true;
  }
  else if ((addr >= 0) && ((addr + len) <= MMIO_SIZE)) {
    return true;
  }
  return false;
}
bool bh_timer_t::initialize(sim_t& sim) {
  ptr_proc = sim.get_core(0);
  assert(ptr_proc);
  ptr_proc->EXT_attach_timer(this);
  return true;
}
bool bh_timer_t::inc_timer () {

  const uint64_t mtime_inc = ((uint64_t)mtime_inc_hi << 32) | mtime_inc_lo;

  // if no mtime_tgt_clk is set, timer is not enabled
  if (!mtime_inc)
    return false;

  uint64_t mtime_tmp = ((uint64_t)mtime_tmp_hi << 32) | mtime_tmp_lo;
  uint64_t mtime_cur = ((uint64_t)mtime_cur_hi << 32) | mtime_cur_lo;
  uint64_t mtime_cmp = ((uint64_t)mtime_cmp_hi << 32) | mtime_cmp_lo;

  mtime_tmp++;
  if (mtime_tmp == mtime_inc) {
    mtime_tmp = 0;
    mtime_cur++;
  }
  processor_t& proc = *ptr_proc;
  // check MTIE in MIE
  bool timer_int_en = (proc.get_state()->mie & (1<<IRQ_M_TIMER));
  bool global_int_en = (proc.get_state()->mstatus & MSTATUS_MIE);

  if (mtime_cur >= mtime_cmp) {
    if (timer_int_en && global_int_en) {
      proc.get_state()->mip |= MIP_MTIP;
    }
    return timer_int_en && global_int_en;
  } else {
    proc.get_state()->mip &= ~MIP_MTIP;
    return false;
  }
}

