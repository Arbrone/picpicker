#pragma once

#include "Shot.h"

#include <QString>

// Writes rating / colour label / reject into "<stem>.xmp" next to the shot, the sidecar name
// Lightroom and Capture One read. A sidecar that another application created is never touched.
namespace Xmp {

enum class Result { Written, Removed, Unchanged, Foreign, Error };

QString path(const Shot &shot);
Result write(const Shot &shot);

} // namespace Xmp
