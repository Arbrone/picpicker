#include "Xmp.h"
#include "Colors.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace {
const QByteArray kCreator = "xmp:CreatorTool=\"PicPicker\"";
}

namespace Xmp {

QString path(const Shot &shot)
{
    return QFileInfo(shot.displayPath()).dir().filePath(shot.stem + QStringLiteral(".xmp"));
}

Result write(const Shot &shot)
{
    const QString file = path(shot);
    QByteArray existing;
    if (QFile in(file); in.open(QIODevice::ReadOnly)) {
        existing = in.readAll();
        if (!existing.contains(kCreator)) return Result::Foreign;
    }

    // Lightroom convention: rating -1 means rejected. There is no standard field for "picked".
    const int rating = shot.mark == Mark::Rejected ? -1 : shot.rating;
    if (rating == 0 && shot.label == Label::None) {
        if (existing.isNull()) return Result::Unchanged;
        return QFile::remove(file) ? Result::Removed : Result::Error;
    }

    QByteArray xml =
        "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
        "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"PicPicker\">\n"
        " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
        "  <rdf:Description rdf:about=\"\"\n"
        "    xmlns:xmp=\"http://ns.adobe.com/xap/1.0/\"\n"
        "    " + kCreator + "\n"
        "    xmp:Rating=\"" + QByteArray::number(rating) + "\"";
    if (shot.label != Label::None) xml += "\n    xmp:Label=\"" + labelName(shot.label).toUtf8() + "\"";
    xml += "/>\n"
           " </rdf:RDF>\n"
           "</x:xmpmeta>\n"
           "<?xpacket end=\"w\"?>\n";

    if (xml == existing) return Result::Unchanged;
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly) || out.write(xml) != xml.size() || !out.commit()) return Result::Error;
    return Result::Written;
}

} // namespace Xmp
