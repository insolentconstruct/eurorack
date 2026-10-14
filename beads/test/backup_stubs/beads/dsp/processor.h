#ifndef BACKUP_TEST_PROCESSOR_H_
#define BACKUP_TEST_PROCESSOR_H_
#include <cstddef>
#include <cstdint>
namespace beads {
const size_t kDelayBufferSizeBits = 768 * 1000 * 8;
// The buffer bytes and the state the backup saves, nothing else.
class Processor {
 public:
  enum Media {
    MEDIA_DIRTY_TAPE,
    MEDIA_CLEAN_TAPE,
    MEDIA_VINTAGE_DIGITAL,
    MEDIA_DIGITAL
  };
  void Configure(Media media, bool mono) {
    media_ = media; mono_ = mono; ++configured_;
  }
  Media media() const { return media_; }
  bool mono() const { return mono_; }
  int* mutable_delay_write_ptr() { return &write_ptr_; }
  uint8_t* delay_buffer_raw_bytes(size_t offset) { return &data_[offset]; }
  uint8_t data_[kDelayBufferSizeBits / 8];
  Media media_;
  bool mono_;
  int write_ptr_;
  int configured_;
};
}  // namespace beads
#endif
