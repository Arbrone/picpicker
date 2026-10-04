#pragma once

#include "Shot.h"

// Reads capture time, exposure and the Fujifilm AF point from the JPG, or from the JPEG preview
// embedded in the RAF. Only a few hundred KiB of each file are read.
Metadata readMetadata(const QString &jpgPath, const QString &rafPath);
