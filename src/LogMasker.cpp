/*
    Copyright (C) 2026 Sebastian J. Wolf and other contributors

    This file is part of Zaster Banker.

    Zaster Banker is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Zaster Banker is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Zaster Banker. If not, see <http://www.gnu.org/licenses/>.
*/

#include "LogMasker.h"

#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QVector>
#include <QXmlStreamReader>

#include <algorithm>

namespace {

/*
 * FinTS 3.0 syntax: segments end with "'", data elements are separated by "+",
 * the parts of a data element group by ":", "?" escapes the next character, and
 * "@<length>@" is followed by that many bytes of binary data.
 */
struct Component {
    // As in the message, escapes included; for binary data, whatever follows
    // it. All three point into the message rather than copying from it: no
    // copy of the PIN or the TAN is ever made.
    QByteArray raw;
    bool binary = false;
    QByteArray length;
    QByteArray data;
};
typedef QVector<Component> Element;
typedef QVector<Element> Segment;

enum Action {
    Keep,
    Mask,
    Pin,
    Tan,
    // the bank's text in a result code
    FreeText,
    // a result code's parameters: TAN method numbers are kept, all else masked
    Parameter
};

// HNVSD holds the signed message, and statements hold no FinTS of their own.
const int MAX_DEPTH = 2;

/*
 * Which data elements of a segment are not personal, by position: "3" is the
 * third data element after the segment header, "4.2" the second part of the
 * fourth, "5+" the fifth and all after it, "*" all. All other elements are
 * masked, and so is every segment without a rule.
 */
struct Rule {
    const char *code;
    int minVersion;
    int maxVersion;
    const char *keep;
};

const Rule RULES[] = {
    // message header and end: size, FinTS version, dialog ID, message number
    { "HNHBK", 0, 99, "*" },
    { "HNHBS", 0, 99, "*" },
    // encryption and signature headers: all but the login (key name) and the
    // system ID (security identification)
    { "HNVSK", 0, 99, "1 2 3 4.1 5 6 7.1 7.2 7.4 7.5 7.6 8" },
    { "HNSHK", 0, 99, "1 2 3 4 5 6.1 7 8 9 10 11.1 11.2 11.4 11.5 11.6" },
    // control reference; PIN and TAN see actionFor()
    { "HNSHA", 0, 99, "1" },
    // bank code and status, not customer and system ID
    { "HKIDN", 0, 99, "1 4" },
    // BPD and UPD version, language, product version - not the registration number
    { "HKVVB", 0, 99, "1 2 3 5" },
    { "HKSYN", 0, 99, "*" },
    { "HISYN", 0, 99, "2 3 4" },
    { "HKEND", 0, 99, "*" },
    // the bank's parameters
    { "HIBPA", 0, 99, "*" },
    { "HIKOM", 0, 99, "*" },
    { "HISHV", 0, 99, "*" },
    { "HIKPV", 0, 99, "*" },
    // user parameters: the UPD version and usage, not login and name
    { "HIUPA", 0, 99, "2 3" },
    // an account's parameters: country and bank code, account type and
    // currency; the jobs allowed see actionFor()
    { "HIUPD", 6, 6, "1.3 1.4 4 5" },
    { "HIUPD", 0, 99, "1.3 1.4" },
    // TAN process, segment, order reference, further TANs, cancellation,
    // challenge class - not the account, the challenge or the TAN medium
    { "HKTAN", 6, 7, "1 2 5 6 7 9" },
    { "HKTAN", 0, 99, "1" },
    { "HITAN", 6, 7, "1 3 6" },
    { "HITAN", 0, 99, "1" },
    { "HKTAB", 0, 99, "*" },
    { "HITAB", 0, 99, "1" },
    // requests: all but the account and the attach point, which may encode it
    { "HKSAL", 0, 99, "2 3" },
    { "HKKAZ", 0, 99, "2 3 4 5" },
    { "HKCAZ", 0, 99, "2 3 4 5 6" },
    { "HKWPD", 0, 99, "2 3 4" },
    // balances: currency, date and time only
    { "HISAL", 0, 99, "3 4.3 4.4 4.5 5.3 5.4 5.5" },
    // CAMT: the format; the statements see maskedBinary()
    { "HICAZ", 0, 99, "2" },
    // SEPA details: whether SEPA, BIC, country and bank code
    { "HISPA", 0, 99, "1+.1 1+.3 1+.6 1+.7" },
};

QString stars(int count)
{
    return QString(count, QLatin1Char('*'));
}

int valueLength(const QByteArray &raw)
{
    int length = 0;
    for (int i = 0; i < raw.size(); i++) {
        if (raw.at(i) == '?' && i + 1 < raw.size()) {
            i++;
        }
        length++;
    }
    return length;
}

QString unescaped(const QByteArray &raw)
{
    QByteArray value;
    for (int i = 0; i < raw.size(); i++) {
        if (raw.at(i) == '?' && i + 1 < raw.size()) {
            i++;
        }
        value += raw.at(i);
    }
    return QString::fromLatin1(value);
}

QString escaped(const QString &value)
{
    QString result;
    for (const QChar c : value) {
        if (c == QLatin1Char('?') || c == QLatin1Char('+') || c == QLatin1Char(':') || c == QLatin1Char('\'')
                || c == QLatin1Char('@')) {
            result += QLatin1Char('?');
        }
        result += c;
    }
    return result;
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

QByteArray slice(const QByteArray &message, int start, int end)
{
    return QByteArray::fromRawData(message.constData() + start, end - start);
}

/*
 * Splits a message into segments; what follows the last complete one stays
 * in *rest. The message must outlive the result, which points into it.
 */
QVector<Segment> parse(const QByteArray &message, QByteArray *rest)
{
    QVector<Segment> segments;
    Segment segment;
    Element element;
    Component component;
    const int size = message.size();
    int componentStart = 0;
    int end = 0;
    int position = 0;
    const auto finishComponent = [&]() {
        component.raw = slice(message, componentStart, position);
        element.append(component);
        component = Component();
        componentStart = position + 1;
    };
    while (position < size) {
        const char c = message.at(position);
        if (c == '?' && position + 1 < size) {
            position += 2;
            continue;
        }
        if (c == '@' && position == componentStart && !component.binary) {
            int digits = position + 1;
            while (digits < size && isDigit(message.at(digits)) && digits - position <= 9) {
                digits++;
            }
            if (digits > position + 1 && digits < size && message.at(digits) == '@') {
                const QByteArray length = slice(message, position + 1, digits);
                const int count = length.toInt();
                if (count >= 0 && digits + 1 + count <= size) {
                    component.binary = true;
                    component.length = length;
                    component.data = slice(message, digits + 1, digits + 1 + count);
                    position = digits + 1 + count;
                    componentStart = position;
                    continue;
                }
            }
        }
        switch (c) {
        case ':':
            finishComponent();
            break;
        case '+':
            finishComponent();
            segment.append(element);
            element.clear();
            break;
        case '\'':
            finishComponent();
            segment.append(element);
            element.clear();
            segments.append(segment);
            segment.clear();
            end = position + 1;
            break;
        default:
            break;
        }
        position++;
    }
    *rest = slice(message, end, size);
    return segments;
}

bool keeps(const char *spec, int element, int component)
{
    const QList<QByteArray> tokens = QByteArray(spec).split(' ');
    for (const QByteArray &token : tokens) {
        if (token.isEmpty()) {
            continue;
        }
        if (token == "*") {
            return true;
        }
        const int dot = token.indexOf('.');
        QByteArray elementPart = dot < 0 ? token : token.left(dot);
        const bool onwards = elementPart.endsWith('+');
        if (onwards) {
            elementPart.chop(1);
        }
        const int keptElement = elementPart.toInt();
        const int keptComponent = dot < 0 ? 0 : token.mid(dot + 1).toInt();
        if ((onwards ? element >= keptElement : element == keptElement)
                && (keptComponent == 0 || keptComponent == component)) {
            return true;
        }
    }
    return false;
}

// The bank's parameters for a job, e.g. HIKAZS, HITANS, HIPINS, DIKKUS: the
// same for every customer. HIRMS (result codes) does not match.
bool isParameterSegment(const QByteArray &code)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Z]I[A-Z0-9]{3,4}S$"));
    return pattern.match(QString::fromLatin1(code)).hasMatch();
}

// One of the jobs an account allows (HIUPD): "HKSAL:1", the job and the number
// of signatures it takes, perhaps with a limit after that. A name never looks
// like that: a data element of its own, its ":" would be escaped.
bool isAllowedJob(const Element &element)
{
    static const QRegularExpression job(QStringLiteral("^[A-Z]{5,6}$"));
    static const QRegularExpression signatures(QStringLiteral("^[0-9]{1,3}$"));
    return element.size() >= 2 && !element.at(0).binary && !element.at(1).binary
            && job.match(QString::fromLatin1(element.at(0).raw)).hasMatch()
            && signatures.match(QString::fromLatin1(element.at(1).raw)).hasMatch();
}

/* element and component count from 1, the segment header being element 0 */
Action actionFor(const QByteArray &code, int version, const Segment &segment, int element, int component)
{
    if (element == 0) {
        return Keep;
    }
    if (code == "HIRMG" || code == "HIRMS") {
        // code, reference element, text, parameters
        return component <= 2 ? Keep : (component == 3 ? FreeText : Parameter);
    }
    if (code == "HNSHA" && element == 3) {
        return component == 1 ? Pin : (component == 2 ? Tan : Mask);
    }
    if (code == "HIUPD" && element >= (version == 6 ? 10 : 2) && isAllowedJob(segment.at(element))) {
        return component <= 2 ? Keep : Mask;
    }
    if (isParameterSegment(code)) {
        return Keep;
    }
    for (const Rule &rule : RULES) {
        if (code == rule.code && version >= rule.minVersion && version <= rule.maxVersion) {
            return keeps(rule.keep, element, component) ? Keep : Mask;
        }
    }
    return Mask;
}

bool isText(const QByteArray &data)
{
    for (const char c : data) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 0x20 && c != '\r' && c != '\n' && c != '\t') {
            return false;
        }
    }
    return true;
}

bool looksLikeFinTs(const QByteArray &data)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Z]{4,6}:[0-9]{1,3}:[0-9]{1,3}[:+']"));
    return pattern.match(QString::fromLatin1(data.left(24))).hasMatch();
}

bool looksLikeXml(const QByteArray &data)
{
    QByteArray start = data.left(64);
    if (start.startsWith("\xef\xbb\xbf")) {
        start = start.mid(3);
    }
    return start.trimmed().startsWith('<');
}

// MT940/MT942 statements start with tag 20, MT535 with 16R; some banks put a
// line of their own in front ("@@", or nothing but a line break).
bool looksLikeSwift(const QByteArray &data)
{
    static const QRegularExpression pattern(QStringLiteral("^(\\s|@@)*:[0-9]{2}[A-Z]?:"));
    return pattern.match(QString::fromLatin1(data.left(64))).hasMatch();
}

/*
 * CAMT: element and attribute names stay, values are masked but for codes
 * (booked or pending, credit or debit, the kind of transaction, the kind of
 * balance), currencies and the statement's own times.
 */
QString maskedXml(const QByteArray &data)
{
    static const QSet<QString> keptElements = {
        QStringLiteral("Cd"), QStringLiteral("SubFmlyCd"), QStringLiteral("CdtDbtInd"), QStringLiteral("Sts"),
        QStringLiteral("RvslInd"), QStringLiteral("CreDtTm"), QStringLiteral("FrDtTm"), QStringLiteral("ToDtTm"),
        QStringLiteral("Ccy"), QStringLiteral("NbOfNtries")
    };
    QXmlStreamReader xml(data);
    QString result;
    QStringList path;
    while (!xml.atEnd()) {
        switch (xml.readNext()) {
        case QXmlStreamReader::StartDocument:
            result += QStringLiteral("<?xml version=\"%1\"").arg(xml.documentVersion().toString());
            if (!xml.documentEncoding().isEmpty()) {
                result += QStringLiteral(" encoding=\"%1\"").arg(xml.documentEncoding().toString().toHtmlEscaped());
            }
            result += QStringLiteral("?>");
            break;
        case QXmlStreamReader::StartElement: {
            result += QLatin1Char('<') + xml.qualifiedName().toString();
            for (const QXmlStreamNamespaceDeclaration &declaration : xml.namespaceDeclarations()) {
                result += QStringLiteral(" xmlns");
                if (!declaration.prefix().isEmpty()) {
                    result += QLatin1Char(':') + declaration.prefix().toString();
                }
                result += QStringLiteral("=\"%1\"").arg(declaration.namespaceUri().toString().toHtmlEscaped());
            }
            for (const QXmlStreamAttribute &attribute : xml.attributes()) {
                const QString value = attribute.value().toString();
                const bool kept = attribute.name() == QLatin1String("Ccy")
                        || attribute.name() == QLatin1String("schemaLocation");
                result += QStringLiteral(" %1=\"%2\"").arg(attribute.qualifiedName().toString(),
                                                           kept ? value.toHtmlEscaped() : stars(value.length()));
            }
            result += QLatin1Char('>');
            path.append(xml.name().toString());
            break;
        }
        case QXmlStreamReader::EndElement:
            result += QStringLiteral("</") + xml.qualifiedName().toString() + QLatin1Char('>');
            if (!path.isEmpty()) {
                path.removeLast();
            }
            break;
        case QXmlStreamReader::Characters: {
            if (xml.isWhitespace()) {
                break;
            }
            const QString text = xml.text().toString();
            // a purpose code (Purp/Cd, e.g. SALA) says what the money was for
            const bool kept = !path.isEmpty() && keptElements.contains(path.last())
                    && !(path.size() >= 2 && path.at(path.size() - 2) == QLatin1String("Purp"));
            result += kept ? text.toHtmlEscaped() : stars(text.length());
            break;
        }
        default:
            // comments, processing instructions, DTDs: nothing of use
            break;
        }
    }
    if (xml.hasError()) {
        result += QStringLiteral(" [not readable from here on: %1]").arg(xml.errorString());
    }
    return result;
}

/*
 * MT940, MT942, MT535: the tags stay, their contents are masked but for the
 * names of the blocks (16R, 16S) and the subfield marks of the purpose (?20).
 */
QString maskedSwift(const QByteArray &data)
{
    static const QRegularExpression tag(QStringLiteral("^(:[0-9]{2}[A-Z]?:)(.*)$"));
    static const QRegularExpression subfield(QStringLiteral("\\?[0-9]{2}"));
    QStringList lines = QString::fromLatin1(data).split(QLatin1Char('\n'));
    for (QString &line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }
        QString kept;
        QString content = line;
        const QRegularExpressionMatch match = tag.match(line);
        if (match.hasMatch()) {
            kept = match.captured(1);
            content = match.captured(2);
            if (kept == QLatin1String(":16R:") || kept == QLatin1String(":16S:")) {
                continue;
            }
        } else if (line == QLatin1String("-") || line.trimmed() == QLatin1String("@@")) {
            continue;
        }
        QString masked;
        int position = 0;
        QRegularExpressionMatchIterator marks = subfield.globalMatch(content);
        while (marks.hasNext()) {
            const QRegularExpressionMatch mark = marks.next();
            masked += stars(mark.capturedStart() - position) + mark.captured();
            position = mark.capturedEnd();
        }
        masked += stars(content.length() - position);
        line = kept + masked;
    }
    return lines.join(QLatin1Char('\n'));
}

QString maskedMessage(const QByteArray &message, int depth);

QString maskedBinary(const QByteArray &data, int depth)
{
    if (depth < MAX_DEPTH && looksLikeFinTs(data)) {
        return maskedMessage(data, depth + 1);
    }
    if (looksLikeXml(data)) {
        return maskedXml(data);
    }
    if (looksLikeSwift(data)) {
        return maskedSwift(data);
    }
    if (isText(data)) {
        return stars(data.size());
    }
    return QStringLiteral("[binary, %1 bytes]").arg(data.size());
}

QString keptBinary(const QByteArray &data, int depth)
{
    if (depth < MAX_DEPTH && looksLikeFinTs(data)) {
        return maskedMessage(data, depth + 1);
    }
    return isText(data) ? QString::fromLatin1(data) : QStringLiteral("[binary, %1 bytes]").arg(data.size());
}

QString shapesMasked(const QString &text);

QString rendered(const Component &component, Action action, int depth)
{
    const QString binary = component.binary ? QLatin1Char('@') + QString::fromLatin1(component.length) + QLatin1Char('@')
                                            : QString();
    switch (action) {
    case Keep:
        return component.binary ? binary + keptBinary(component.data, depth) + stars(valueLength(component.raw))
                                : QString::fromLatin1(component.raw);
    case Pin:
        return component.raw.isEmpty() && !component.binary ? QString() : QStringLiteral("[PIN]");
    case Tan:
        return component.raw.isEmpty() && !component.binary ? QString() : QStringLiteral("[TAN]");
    case FreeText:
        return component.binary ? binary + maskedBinary(component.data, depth)
                                : escaped(shapesMasked(unescaped(component.raw)));
    case Parameter: {
        static const QRegularExpression number(QStringLiteral("^[0-9]{1,4}$"));
        if (!component.binary && number.match(QString::fromLatin1(component.raw)).hasMatch()) {
            return QString::fromLatin1(component.raw);
        }
        return component.binary ? binary + maskedBinary(component.data, depth) : stars(valueLength(component.raw));
    }
    case Mask:
        break;
    }
    return component.binary ? binary + maskedBinary(component.data, depth) + stars(valueLength(component.raw))
                            : stars(valueLength(component.raw));
}

QString maskedMessage(const QByteArray &message, int depth)
{
    QByteArray rest;
    const QVector<Segment> segments = parse(message, &rest);
    QStringList result;
    for (const Segment &segment : segments) {
        const Element &header = segment.at(0);
        const QByteArray code = header.isEmpty() ? QByteArray() : header.at(0).raw;
        const int version = header.size() >= 3 ? header.at(2).raw.toInt() : 0;
        QStringList elements;
        for (int e = 0; e < segment.size(); e++) {
            QStringList components;
            for (int c = 0; c < segment.at(e).size(); c++) {
                components.append(rendered(segment.at(e).at(c), actionFor(code, version, segment, e, c + 1), depth));
            }
            elements.append(components.join(QLatin1Char(':')));
        }
        result.append(elements.join(QLatin1Char('+')) + QLatin1Char('\''));
    }
    // an incomplete segment, masked as a whole; a line break after the last is not one
    for (const char c : rest) {
        if (c != '\r' && c != '\n' && c != ' ') {
            result.append(stars(rest.size()));
            break;
        }
    }
    return result.join(QLatin1Char('|'));
}

QString shapesMasked(const QString &text)
{
    static const QList<QRegularExpression> shapes = {
        // IBAN, with or without spaces
        QRegularExpression(QStringLiteral("\\b[A-Z]{2}[0-9]{2}(?: ?[A-Z0-9]{4}){2,7}(?: ?[A-Z0-9]{1,3})?\\b")),
        // e-mail address
        QRegularExpression(QStringLiteral("[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}")),
        // amount with a currency
        QRegularExpression(QStringLiteral("(?:EUR|€|USD|CHF|GBP) ?-?[0-9][0-9.,]*|-?[0-9][0-9.,]* ?(?:EUR|€|USD|CHF|GBP)")),
        // account numbers, customer IDs, card numbers
        QRegularExpression(QStringLiteral("[0-9]{5,}"))
    };
    QString result = text;
    for (const QRegularExpression &shape : shapes) {
        QRegularExpressionMatchIterator matches = shape.globalMatch(result);
        QString masked;
        int position = 0;
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            masked += result.midRef(position, match.capturedStart() - position);
            masked += stars(match.capturedLength());
            position = match.capturedEnd();
        }
        masked += result.midRef(position);
        result = masked;
    }
    return result;
}

QMutex personalDataMutex;
QSet<QString> personalData;
QRegularExpression personalPattern;

QString personalDataMasked(const QString &text)
{
    QRegularExpression pattern;
    {
        QMutexLocker locker(&personalDataMutex);
        pattern = personalPattern;
    }
    if (pattern.pattern().isEmpty()) {
        return text;
    }
    QRegularExpressionMatchIterator matches = pattern.globalMatch(text);
    if (!matches.hasNext()) {
        return text;
    }
    QString result;
    int position = 0;
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        result += text.midRef(position, match.capturedStart() - position);
        result += stars(match.capturedLength());
        position = match.capturedEnd();
    }
    result += text.midRef(position);
    return result;
}

void addValue(QSet<QString> *values, const QString &value)
{
    // shorter would mask what is not theirs, e.g. a sub-account "00" everywhere
    if (value.length() >= 4) {
        values->insert(value);
    }
}

}

namespace LogMasker
{

QString finTsMessage(const QByteArray &message)
{
    return personalDataMasked(maskedMessage(message, 0));
}

QString text(const QString &text)
{
    return personalDataMasked(shapesMasked(text));
}

QString everything(const QString &text)
{
    QString result = text;
    bool inTag = false;
    for (QChar &c : result) {
        if (c == QLatin1Char('<')) {
            inTag = true;
        } else if (c == QLatin1Char('>')) {
            inTag = false;
        } else if (!inTag && c.isLetterOrNumber()) {
            c = QLatin1Char('*');
        }
    }
    return result;
}

void addPersonalData(const QStringList &values, bool words)
{
    QSet<QString> added;
    for (const QString &value : values) {
        const QString trimmed = value.trimmed();
        addValue(&added, trimmed);
        // account numbers turn up with and without their leading zeros
        static const QRegularExpression digits(QStringLiteral("^0+([0-9]+)$"));
        const QRegularExpressionMatch number = digits.match(trimmed);
        if (number.hasMatch()) {
            addValue(&added, number.captured(1));
        }
        if (words) {
            for (const QString &word : trimmed.split(QRegularExpression(QStringLiteral("[\\s,;/]+")), QString::SkipEmptyParts)) {
                addValue(&added, word);
            }
        }
    }
    QMutexLocker locker(&personalDataMutex);
    const int before = personalData.size();
    personalData.unite(added);
    if (personalData.size() == before) {
        return;
    }
    QStringList sorted = personalData.toList();
    // the longest first, so that a name is masked as a whole before its parts
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) {
        return a.length() > b.length();
    });
    for (QString &value : sorted) {
        value = QRegularExpression::escape(value);
    }
    personalPattern = QRegularExpression(QStringLiteral("(?<![\\p{L}\\p{N}])(?:%1)(?![\\p{L}\\p{N}])").arg(sorted.join(QLatin1Char('|'))),
                                         QRegularExpression::CaseInsensitiveOption
                                         | QRegularExpression::UseUnicodePropertiesOption);
}

void clearPersonalData()
{
    QMutexLocker locker(&personalDataMutex);
    personalData.clear();
    personalPattern = QRegularExpression();
}

}
