namespace beads {
class GainControl {
 public:
  void Restart() { }
  bool adjusting() const { return false; }
  int vu_meter() const { return 128; }
};
}
