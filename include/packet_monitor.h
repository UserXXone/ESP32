#pragma once
#include "ui.h"

// Mode 5: Per-channel packet-rate monitor.
// Hops across all 2.4GHz channels, counts management+data frames seen per
// channel per hop cycle, and keeps a rolling baseline (mean/stddev) of
// total traffic per cycle. Flags a cycle as anomalous when it deviates
// from the baseline by more than PKTMON_ANOMALY_SIGMA standard
// deviations -- a generic "something unusual just happened on the air"
// signal (could be a flood, a sudden crowd of devices, a nearby attack
// tool, etc; this mode doesn't classify *what*, only *that*).
namespace packet_monitor {

void enter();
void exit();
void loop(ButtonEvent ev);

} // namespace packet_monitor
