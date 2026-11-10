// Copyright 2026 Insolent Construct.
//
// Author: Insolent Construct (insolentconstruct@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Real codec reset/clock sequencing with an ordered fake HAL.

#include "beads/drivers/codec.h"

#include <vector>

#include "beads/test/test_checker.h"

using namespace beads;

GPIO_TypeDef fake_gpio_b;
GPIO_TypeDef fake_gpio_c;
GPIO_TypeDef fake_gpio_e;
SAI_Block_TypeDef fake_sai_tx;
SAI_Block_TypeDef fake_sai_rx;
DMA_Stream_TypeDef fake_dma_tx;
DMA_Stream_TypeDef fake_dma_rx;
DMA_Stream_TypeDef fake_dma_idr;
DMA_TypeDef fake_dma;

enum Operation { RESET_LOW, RESET_HIGH, ABORT, START, DEINIT, INIT, RESUME,
  DELAY, ENABLE_GENERATOR };
enum Device { NONE, TX, RX, IDR };

struct HalEvent {
  Operation operation;
  Device device;
  uint32_t tick;
  uint32_t argument;
  bool reset_low;
  bool clocks;
  bool silent;
  bool callback_attached;
};

static Codec codec;
static std::vector<HalEvent> events;
static uint32_t tick;
static bool reset_low;
static bool clocks;
static bool probe_callback;
static int callback_calls;
static size_t callback_size;
static int init_calls;
static int fail_init;
static Codec::Frame* tx_buffer;

void Process(const Codec::Event*, const Codec::Frame*, Codec::Frame* out,
    size_t size) {
  ++callback_calls;
  callback_size = size;
  for (size_t i = 0; i < size; ++i) {
    out[i].l = 123456;
    out[i].r = -123456;
  }
}

bool TxSilent() {
  if (!tx_buffer) {
    return true;
  }
  for (size_t i = 0; i < kMaxCodecBlockSize * 2; ++i) {
    if (tx_buffer[i].l || tx_buffer[i].r) {
      return false;
    }
  }
  return true;
}

void Record(Operation operation, Device device, uint32_t argument) {
  HalEvent event;
  event.operation = operation;
  event.device = device;
  event.tick = tick;
  event.argument = argument;
  event.reset_low = reset_low;
  event.clocks = clocks;
  event.silent = TxSilent();
  // A possible audio interrupt at each HAL boundary observes attachment.
  const int before = callback_calls;
  if (probe_callback) {
    codec.Fill(0);
  }
  event.callback_attached = callback_calls != before;
  events.push_back(event);
}

void ResetRegister::operator=(uint32_t value) {
  CHECK(this == &CODEC_RESET_GPIO_PORT->BSRR);
  CHECK(value == CODEC_RESET_GPIO_PIN ||
      value == (CODEC_RESET_GPIO_PIN << 16U));
  reset_low = value == (CODEC_RESET_GPIO_PIN << 16U);
  Record(reset_low ? RESET_LOW : RESET_HIGH, NONE, value);
}

Device DmaDevice(DMA_HandleTypeDef* dma) {
  return dma->Instance == DMA1_Stream0 ? TX :
      (dma->Instance == DMA1_Stream1 ? RX : IDR);
}

void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*) { }
void HAL_NVIC_EnableIRQ(int) { }
void HAL_NVIC_SetPriority(int, int, int) { }
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef*) { return HAL_OK; }
HAL_StatusTypeDef HAL_DMAEx_ConfigMuxSync(
    DMA_HandleTypeDef*, HAL_DMA_MuxSyncConfigTypeDef*) { return HAL_OK; }
HAL_StatusTypeDef HAL_DMAEx_ConfigMuxRequestGenerator(
    DMA_HandleTypeDef*, HAL_DMA_MuxRequestGeneratorConfigTypeDef*) {
  return HAL_OK;
}
HAL_StatusTypeDef HAL_DMAEx_EnableMuxRequestGenerator(DMA_HandleTypeDef* dma) {
  Record(ENABLE_GENERATOR, DmaDevice(dma), 0);
  return HAL_OK;
}

void HAL_Delay(uint32_t delay) {
  Record(DELAY, NONE, delay);
  // The HAL guarantees only the requested delay.
  tick += delay;
}

HAL_StatusTypeDef HAL_SAI_InitProtocol(SAI_HandleTypeDef* sai,
    uint32_t standard, uint32_t data_size, uint32_t channels) {
  CHECK(standard == SAI_I2S_STANDARD);
  CHECK(data_size == SAI_PROTOCOL_DATASIZE_32BIT);
  CHECK(channels == 2);
  Record(INIT, sai->Instance == SAI1_Block_A ? TX : RX,
      sai->Init.AudioFrequency);
  return ++init_calls == fail_init ? HAL_ERROR : HAL_OK;
}

HAL_StatusTypeDef HAL_SAI_DeInit(SAI_HandleTypeDef* sai) {
  const Device device = sai->Instance == SAI1_Block_A ? TX : RX;
  if (device == TX) {
    clocks = false;
  }
  Record(DEINIT, device, 0);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_SAI_DMAResume(SAI_HandleTypeDef* sai) {
  const Device device = sai->Instance == SAI1_Block_A ? TX : RX;
  if (device == TX) {
    clocks = true;
  }
  Record(RESUME, device, 0);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef* dma) {
  Record(ABORT, DmaDevice(dma), 0);
  return HAL_OK;
}

HAL_StatusTypeDef HAL_DMA_Start(DMA_HandleTypeDef* dma,
    uint32_t source, uint32_t destination, uint32_t size) {
  const Device device = DmaDevice(dma);
  if (device == TX) {
    tx_buffer = reinterpret_cast<Codec::Frame*>(uintptr_t(source));
    CHECK(destination == uintptr_t(&fake_sai_tx.DR));
  } else if (device == RX) {
    CHECK(source == uintptr_t(&fake_sai_rx.DR));
  } else {
    CHECK(source == uintptr_t(&SEED_GPIO_PORT->IDR));
  }
  Record(START, device, size);
  return HAL_OK;
}

void ResetHarness() {
  probe_callback = false;
  codec.Pause();
  clocks = false;
  reset_low = true;
  tick = 0;
  init_calls = 0;
  fail_init = 0;
  callback_calls = 0;
  callback_size = 0;
  fake_dma_rx.CR = 0;
  tx_buffer = NULL;
  CHECK(codec.Init());
  events.clear();
  probe_callback = true;
}

void RunningCodec(int32_t rate) {
  ResetHarness();
  CHECK(codec.Start(rate, rate / 2000, &Process));
  codec.Fill(0);
  codec.Fill(1);
  CHECK(!TxSilent());
  events.clear();
  tick = 0;
  init_calls = 0;
  callback_calls = 0;
}

void CheckEvent(size_t index, Operation operation, Device device) {
  if (CHECK(index < events.size())) {
    CHECK(events[index].operation == operation);
    CHECK(events[index].device == device);
  }
}

void TestRestartOrder() {
  test_checker.Start("TestRestartOrder");
  const int32_t rates[][2] = {
    { 48000, 32000 }, { 32000, 24000 },
    { 24000, 48000 }, { 48000, 24000 }
  };
  for (size_t r = 0; r < sizeof(rates) / sizeof(rates[0]); ++r) {
    const int32_t previous_rate = rates[r][0];
    const int32_t rate = rates[r][1];
    RunningCodec(previous_rate);
    CHECK(codec.Restart(previous_rate, rate, rate / 2000, &Process));
    printf("Restart %d -> %d: %u ms, %lu events\n", previous_rate, rate,
        tick, static_cast<unsigned long>(events.size()));
    CHECK(events.size() == 17);
    CheckEvent(0, RESET_LOW, NONE);
    CheckEvent(1, DELAY, NONE);
    CheckEvent(2, ABORT, TX);
    CheckEvent(3, ABORT, RX);
    CheckEvent(4, ABORT, IDR);
    CheckEvent(5, DEINIT, TX);
    CheckEvent(6, DEINIT, RX);
    CheckEvent(7, INIT, TX);
    CheckEvent(8, INIT, RX);
    CheckEvent(9, START, IDR);
    CheckEvent(10, ENABLE_GENERATOR, IDR);
    CheckEvent(11, START, TX);
    CheckEvent(12, START, RX);
    CheckEvent(13, RESUME, TX);
    CheckEvent(14, RESUME, RX);
    CheckEvent(15, DELAY, NONE);
    CheckEvent(16, RESET_HIGH, NONE);
    // 2048 old frames plus 1 ms for the real clocks' offset from nominal.
    const uint32_t minimum_hold = 2048000 + uint32_t(previous_rate);
    for (size_t i = 0; i < events.size(); ++i) {
      const HalEvent& event = events[i];
      CHECK(event.silent);
      CHECK(!event.callback_attached);
      CHECK(event.reset_low || event.clocks);
      if (i < 5) {
        CHECK(event.clocks);
      }
      if (event.operation != RESET_HIGH) {
        CHECK(event.reset_low);
      }
      if (event.operation == ABORT || event.operation == DEINIT) {
        CHECK(event.tick * uint32_t(previous_rate) >= minimum_hold);
      }
      if (event.operation == INIT && event.device == TX) {
        CHECK(event.argument == uint32_t(rate));
      }
      if (event.operation == START) {
        CHECK(event.argument == uint32_t(rate / 2000 * 4));
      }
      if (event.operation == RESET_HIGH) {
        CHECK(i > 14);
        CHECK(event.tick >= events[13].tick + 1);
      }
    }
    CHECK((fake_dma_rx.CR & (DMA_IT_TC | DMA_IT_HT)) ==
        (DMA_IT_TC | DMA_IT_HT));
    CHECK(callback_calls == 0);
    codec.Fill(0);
    CHECK(callback_calls == 1);
    CHECK(callback_size == size_t(rate / 2000));
    CHECK(!TxSilent());
  }
}

void TestStartUnchanged() {
  test_checker.Start("TestStartUnchanged");
  ResetHarness();
  CHECK(codec.Start(48000, 24, &Process));
  CHECK(events.size() == 9);
  CheckEvent(0, INIT, TX);
  CheckEvent(1, INIT, RX);
  CheckEvent(2, RESET_HIGH, NONE);
  CheckEvent(3, START, IDR);
  CheckEvent(4, ENABLE_GENERATOR, IDR);
  CheckEvent(5, START, TX);
  CheckEvent(6, START, RX);
  CheckEvent(7, RESUME, TX);
  CheckEvent(8, RESUME, RX);
  // The boot path attaches its callback and releases reset before clocks.
  CHECK(!events[2].clocks);
  CHECK(!events[2].reset_low);
  CHECK(events[2].callback_attached);
  CHECK(events[7].clocks);
  CHECK(tick == 0);
}

void TestRestartNullCallback() {
  test_checker.Start("TestRestartNullCallback");
  ResetHarness();
  CHECK(!codec.Restart(48000, 32000, 16, NULL));
  CHECK(events.empty());
  CHECK(reset_low && !clocks);
  codec.Fill(0);
  CHECK(callback_calls == 0);
  // Rejection on a running codec must also leave its state untouched.
  RunningCodec(48000);
  CHECK(!codec.Restart(48000, 32000, 16, NULL));
  CHECK(events.empty());
  CHECK(!reset_low && clocks);
  CHECK(!TxSilent());
  codec.Fill(0);
  CHECK(callback_calls == 1);
}

void TestRestartInitFailure() {
  test_checker.Start("TestRestartInitFailure");
  for (int failure = 1; failure <= 2; ++failure) {
    RunningCodec(48000);
    fail_init = failure;
    CHECK(!codec.Restart(48000, 32000, 16, &Process));
    CHECK(reset_low && !clocks);
    CHECK(TxSilent());
    CHECK(init_calls == failure);
    for (size_t i = 0; i < events.size(); ++i) {
      CHECK(events[i].reset_low);
      CHECK(!events[i].callback_attached);
      CHECK(events[i].operation != START && events[i].operation != RESUME);
    }
    codec.Fill(0);
    CHECK(callback_calls == 0);
  }
}

int main() {
  TestRestartOrder();
  TestStartUnchanged();
  TestRestartNullCallback();
  TestRestartInitFailure();
  printf("Codec: 4 tests, %d failures\n", test_checker.failures());
  return test_checker.failures() ? 1 : 0;
}
