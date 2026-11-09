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
// Minimal HAL interface for the real codec driver host tests.

#ifndef BEADS_TEST_CODEC_STUBS_STM32H7XX_HAL_H_
#define BEADS_TEST_CODEC_STUBS_STM32H7XX_HAL_H_

#include <stdint.h>

struct ResetRegister {
  void operator=(uint32_t value);
};
struct GPIO_TypeDef {
  uint32_t IDR;
  ResetRegister BSRR;
};
struct GPIO_InitTypeDef {
  uint32_t Mode;
  uint32_t Pull;
  uint32_t Speed;
  uint32_t Pin;
  uint32_t Alternate;
};
struct SAI_Block_TypeDef {
  uint32_t DR;
};
struct SAI_InitTypeDef {
  uint32_t AudioMode;
  uint32_t Synchro;
  uint32_t OutputDrive;
  uint32_t NoDivider;
  uint32_t FIFOThreshold;
  uint32_t AudioFrequency;
  uint32_t Mckdiv;
  uint32_t SynchroExt;
  uint32_t MonoStereoMode;
  uint32_t CompandingMode;
  uint32_t TriState;
};
struct SAI_HandleTypeDef {
  SAI_Block_TypeDef* Instance;
  SAI_InitTypeDef Init;
};
struct DMA_Stream_TypeDef {
  uint32_t CR;
};
struct DMA_TypeDef {
  uint32_t LISR;
  uint32_t LIFCR;
};
struct DMA_InitTypeDef {
  uint32_t Request;
  uint32_t Direction;
  uint32_t PeriphInc;
  uint32_t MemInc;
  uint32_t PeriphDataAlignment;
  uint32_t MemDataAlignment;
  uint32_t Mode;
  uint32_t Priority;
  uint32_t FIFOMode;
};
struct DMA_HandleTypeDef {
  DMA_Stream_TypeDef* Instance;
  DMA_InitTypeDef Init;
};
struct HAL_DMA_MuxSyncConfigTypeDef {
  uint32_t SyncSignalID;
  uint32_t SyncPolarity;
  uint32_t SyncEnable;
  uint32_t EventEnable;
  uint32_t RequestNumber;
};
struct HAL_DMA_MuxRequestGeneratorConfigTypeDef {
  uint32_t SignalID;
  uint32_t Polarity;
  uint32_t RequestNumber;
};

enum HAL_StatusTypeDef { HAL_OK, HAL_ERROR };
enum {
  GPIO_MODE_OUTPUT_PP, GPIO_MODE_AF_PP, GPIO_NOPULL,
  GPIO_SPEED_FREQ_LOW, GPIO_SPEED_FREQ_VERY_HIGH, GPIO_AF6_SAI1,
  SAI_MODEMASTER_TX, SAI_MODESLAVE_RX, SAI_ASYNCHRONOUS, SAI_SYNCHRONOUS,
  SAI_OUTPUTDRIVE_DISABLE, SAI_MASTERDIVIDER_ENABLE, SAI_FIFOTHRESHOLD_EMPTY,
  SAI_SYNCEXT_DISABLE, SAI_STEREOMODE, SAI_NOCOMPANDING, SAI_OUTPUT_NOTRELEASED,
  SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_32BIT,
  DMA_REQUEST_SAI1_A, DMA_REQUEST_SAI1_B, DMA_REQUEST_GENERATOR0,
  DMA_MEMORY_TO_PERIPH, DMA_PERIPH_TO_MEMORY, DMA_PINC_DISABLE, DMA_MINC_ENABLE,
  DMA_PDATAALIGN_WORD, DMA_MDATAALIGN_WORD, DMA_CIRCULAR,
  DMA_PRIORITY_HIGH, DMA_PRIORITY_MEDIUM, DMA_FIFOMODE_DISABLE,
  HAL_DMAMUX_SYNC_NO_EVENT, HAL_DMAMUX1_REQ_GEN_DMAMUX1_CH1_EVT,
  HAL_DMAMUX_REQ_GEN_RISING, DMA1_Stream1_IRQn
};

#define GPIO_PIN_2 (1U << 2)
#define GPIO_PIN_3 (1U << 3)
#define GPIO_PIN_4 (1U << 4)
#define GPIO_PIN_5 (1U << 5)
#define GPIO_PIN_6 (1U << 6)
#define GPIO_PIN_10 (1U << 10)
#define GPIO_PIN_11 (1U << 11)
#define GPIO_PIN_13 (1U << 13)
#define DMA_IT_TC (1U << 4)
#define DMA_IT_HT (1U << 3)
#define DMA_LISR_TCIF1 (1U << 11)
#define DMA_LISR_HTIF1 (1U << 10)
#define DISABLE 0
#define ENABLE 1
#define __HAL_RCC_SAI1_CLK_ENABLE() ((void)0)

extern GPIO_TypeDef fake_gpio_b;
extern GPIO_TypeDef fake_gpio_c;
extern GPIO_TypeDef fake_gpio_e;
extern SAI_Block_TypeDef fake_sai_tx;
extern SAI_Block_TypeDef fake_sai_rx;
extern DMA_Stream_TypeDef fake_dma_tx;
extern DMA_Stream_TypeDef fake_dma_rx;
extern DMA_Stream_TypeDef fake_dma_idr;
extern DMA_TypeDef fake_dma;
#define GPIOB (&fake_gpio_b)
#define GPIOC (&fake_gpio_c)
#define GPIOE (&fake_gpio_e)
#define SAI1_Block_A (&fake_sai_tx)
#define SAI1_Block_B (&fake_sai_rx)
#define DMA1_Stream0 (&fake_dma_tx)
#define DMA1_Stream1 (&fake_dma_rx)
#define DMA1_Stream6 (&fake_dma_idr)
#define DMA1 (&fake_dma)

void HAL_GPIO_Init(GPIO_TypeDef*, GPIO_InitTypeDef*);
void HAL_NVIC_EnableIRQ(int);
void HAL_NVIC_SetPriority(int, int, int);
void HAL_Delay(uint32_t delay);
HAL_StatusTypeDef HAL_SAI_InitProtocol(
    SAI_HandleTypeDef*, uint32_t, uint32_t, uint32_t);
HAL_StatusTypeDef HAL_SAI_DeInit(SAI_HandleTypeDef*);
HAL_StatusTypeDef HAL_SAI_DMAResume(SAI_HandleTypeDef*);
HAL_StatusTypeDef HAL_DMA_Init(DMA_HandleTypeDef*);
HAL_StatusTypeDef HAL_DMA_Start(
    DMA_HandleTypeDef*, uint32_t, uint32_t, uint32_t);
HAL_StatusTypeDef HAL_DMA_Abort(DMA_HandleTypeDef*);
HAL_StatusTypeDef HAL_DMAEx_ConfigMuxSync(
    DMA_HandleTypeDef*, HAL_DMA_MuxSyncConfigTypeDef*);
HAL_StatusTypeDef HAL_DMAEx_ConfigMuxRequestGenerator(
    DMA_HandleTypeDef*, HAL_DMA_MuxRequestGeneratorConfigTypeDef*);
HAL_StatusTypeDef HAL_DMAEx_EnableMuxRequestGenerator(DMA_HandleTypeDef*);

#endif  // BEADS_TEST_CODEC_STUBS_STM32H7XX_HAL_H_
