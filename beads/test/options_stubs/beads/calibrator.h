#include "beads/drivers/cv_adc.h"
namespace beads {
class Calibrator {
 public:
  int starts = 0;
  void Start() { ++starts; }
  void CalibrateOffsets() { }
  bool Calibrate(CvAdcChannel, int, float) { return true; }
};
}
