#pragma once

#include <QString>

namespace ImageNumbering {
	int findMaxIndex(const QString& dir1, const QString& dir2);
	QString makeRawPath(const QString& dir, int index);
	QString makeJsonPath(const QString& dir, int index);
}