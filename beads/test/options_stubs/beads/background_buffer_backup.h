namespace beads {
class BackgroundBufferBackup {
 public:
  bool busy_ = false;
  bool busy() const { return busy_; }
};
}
