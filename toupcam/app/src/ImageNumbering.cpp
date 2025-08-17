#include "ImageNumbering.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace ImageNumbering {

static int findMaxInDir(const QString& dir)
{
	int maxIdx = 0;
	QDir d(dir);
	if (!d.exists()) return 0;
	QStringList filters; filters << "IMG_*.raw";
	auto list = d.entryList(filters, QDir::Files, QDir::Name);
	QRegularExpression re("^IMG_([0-9]+)\\.raw$");
	for (const QString& f : list) {
		QRegularExpressionMatch m = re.match(f);
		if (m.hasMatch()) {
			int v = m.captured(1).toInt();
			if (v > maxIdx) maxIdx = v;
		}
	}
	return maxIdx;
}

int findMaxIndex(const QString& dir1, const QString& dir2)
{
	return qMax(findMaxInDir(dir1), findMaxInDir(dir2));
}

QString makeRawPath(const QString& dir, int index)
{
	return QDir(dir).filePath(QString("IMG_%1.raw").arg(index));
}

QString makeJsonPath(const QString& dir, int index)
{
	return QDir(dir).filePath(QString("IMG_%1.json").arg(index));
}

}