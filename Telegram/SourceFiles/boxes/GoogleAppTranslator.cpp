#include "GoogleAppTranslator.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QCryptographicHash>
#include <QUrl>

GoogleAppTranslator::GoogleAppTranslator(QObject* parent)
    : BaseTranslator(parent)
{
    targetLanguages = QList<QString>{
        "sq", "ar", "am", "as", "az", "ee", "ay", "ga", "et", "or", "om", "eu",
        "be", "bm", "bg", "is", "pl", "bs", "fa", "bho", "af", "tt", "da", "de",
        "dv", "ti", "doi", "ru", "fr", "sa", "tl", "fi", "fy", "km", "ka", "gom",
        "gu", "gn", "kk", "ht", "ko", "ha", "nl", "ky", "gl", "ca", "cs", "kn",
        "co", "kri", "hr", "qu", "ku", "ckb", "la", "lv", "lo", "lt", "ln", "lg",
        "lb", "rw", "ro", "mg", "mt", "mr", "ml", "ms", "mk", "mai", "mi", "mni-Mtei",
        "mn", "bn", "lus", "my", "hmn", "xh", "zu", "ne", "no", "pa", "pt", "ps",
        "ny", "ak", "ja", "sv", "sm", "sr", "nso", "st", "si", "eo", "sk", "sl",
        "sw", "gd", "ceb", "so", "tg", "te", "ta", "th", "tr", "tk", "cy", "ug",
        "ur", "uk", "uz", "es", "iw", "el", "haw", "sd", "hu", "sn", "hy", "ig",
        "ilo", "it", "yi", "hi", "su", "id", "jw", "en", "yo", "vi", "zh-TW", "zh-CN",
        "ts", "zh"
    };
}

GoogleAppTranslator* GoogleAppTranslator::instance() {
    static GoogleAppTranslator inst;
    return &inst;
}

QList<QString> GoogleAppTranslator::getTargetLanguages() const {
    return targetLanguages;
}

BaseTranslator::Result GoogleAppTranslator::translateImpl(const QString& query, const QString& fl, const QString& tl) {
    QString fromLang = isEmpty(fl) ? "auto" : fl;

    QString queryParams =
        QString("query.source_language=%1&query.target_language=%2&query.display_language=%2")
            .arg(fromLang, tl) +
        "&params.client=at"
        "&data_types=16&data_types=1&data_types=10&data_types=21"
        "&data_types=6&data_types=7&data_types=5&data_types=17"
        "&data_types=12&data_types=8&data_types=26";

    queryParams += "&params.request_token=" + sign(query);

    bool usePost = query.length() > 1200 && query.toUtf8().length() > 5000;
    QString queryText = "query.text=" + URLEncode(query);

    QString fullUrl = "https://translate-pa.googleapis.com/v1/translate?" +
                      (usePost ? queryParams : queryText + "&" + queryParams);

    Http http = Http::url(fullUrl)
        .header("User-Agent", "GoogleTranslate/9.10.70.766168802.3-release (Linux; U; Android 15; Pixel 8 Pro)")
        .header("x-goog-api-key", "AIzaSyB3hNT9hc3jh2EfvcW6Q7PcYg3F6pPlzso")
        .header("x-android-package", "com.google.android.apps.translate")
        .header("x-android-cert", "24bb24c05e47e0aefa68a58a766179d9b613a600");

    if (usePost) {
        http.header("x-http-method-override", "GET")
            .data(queryText);
    }

    QString response = http.request();
    return getResult(response);
}

BaseTranslator::Result GoogleAppTranslator::getResult(const QString& jsonData) const {
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    QJsonObject obj = doc.object();

    if (obj.contains("translation")) {
        return { obj["translation"].toString(), obj["sourceLanguage"].toString() };
    }

    if (obj.contains("error")) {
        QJsonObject errorObj = obj["error"].toObject();
        throw std::runtime_error(errorObj["message"].toString().toStdString());
    }

    throw std::runtime_error("Unexpected response: " + jsonData.toStdString());
}

QString GoogleAppTranslator::sign(const QString& str) {
    int i = QRandomGenerator::global()->generate();

    for (const auto& byte : str.toUtf8()) {
        int q = i + static_cast<quint8>(byte);
        int i2 = q + (q << 10);
        i = i2 ^ (i2 >> 6);
    }

    int i3 = i + (i << 3);
    int i4 = i3 ^ (i3 >> 11);
    int hashed = hash(O(i));
    int Nval = static_cast<int>(N(hashed ^ (i4 + (i4 << 15))) % N(1000000));

    return O(Nval) + "." + O(i ^ Nval);
}

int GoogleAppTranslator::hash(const QString& input) {
    static const QByteArray key = QByteArray::fromHex(
        "1e63550dfcb0d2d35d94196507ee3124fbff81dcea9f9e2f22d580a90e6d1e65");

    QMessageAuthenticationCode mac(QCryptographicHash::Sha256);
    mac.setKey(key);
    mac.addData(input.toLatin1());

    QByteArray h = mac.result();
    return ((h[3] & 255) << 24) | (h[0] & 255) | ((h[1] & 255) << 8) | ((h[2] & 255) << 16);
}

qint64 GoogleAppTranslator::N(int i) {
    return static_cast<qint64>(i) & 0xFFFFFFFFLL;
}

QString GoogleAppTranslator::O(int i) {
    return QString::number(N(i));
}
