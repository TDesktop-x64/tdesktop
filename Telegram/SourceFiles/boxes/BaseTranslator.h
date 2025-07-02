#pragma once

#include <QEventLoop>
#include <QObject>
#include <QString>
#include <QList>
#include <QByteArray>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QUrl>
#include <QCryptographicHash>

class BaseTranslator : public QObject {
    Q_OBJECT

public:
    struct Result {
        QString translation;
        QString sourceLanguage;
    };

    BaseTranslator(QObject* parent = nullptr) : QObject(parent) {}
	
	Result translate(const QString& query, const QString& fl, const QString& tl) {
        QString fromLang = fl.isEmpty() ? QString() : convertLanguageCode(fl, false);
        QString toLang = convertLanguageCode(tl, false);

        Result result = translateImpl(query, fromLang, toLang);

        QString resolvedSource = result.sourceLanguage.isEmpty()
            ? QString()
            : convertLanguageCode(result.sourceLanguage, true);

        return Result{ result.translation, resolvedSource };
    }

    bool supportLanguage(const QString& language) const {
        return getTargetLanguages().contains(language);
    }

protected:
    virtual Result translateImpl(const QString& query, const QString& fl, const QString& tl) = 0;

    virtual QString convertLanguageCode(const QString& code, bool reverse) const { return code; }

    QString URLEncode(const QString& s) const {
        return QString::fromUtf8(QUrl::toPercentEncoding(s));
    }

    QString MD5(const QString& input) const {
        if (input.isEmpty()) return {};
        QByteArray hash = QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Md5);
        return QString(hash.toHex());
    }

    bool isEmpty(const QString& str) const {
        return str.trimmed().isEmpty();
    }

    virtual QList<QString> getTargetLanguages() const = 0;

    class Http {
    public:
        static Http url(const QString& url) {
            return Http(url);
        }

        Http& header(const QString& key, const QString& value) {
            netRequest.setRawHeader(key.toUtf8(), value.toUtf8());
            return *this;
        }

        Http& data(const QString& data, const QString& mediaType = "application/x-www-form-urlencoded") {
            postData = data.toUtf8();
            this->mediaType = mediaType;
            isPost = true;
            return *this;
        }

        QString request() {
            QNetworkAccessManager manager;
            QEventLoop loop;

            QNetworkReply* reply = nullptr;
            if (isPost) {
                netRequest.setHeader(QNetworkRequest::ContentTypeHeader, mediaType);
                reply = manager.post(netRequest, postData);
            } else {
                reply = manager.get(netRequest);
            }

            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            loop.exec();

            QByteArray response = reply->readAll();
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            reply->deleteLater();

            if (statusCode == 429) {
                throw std::runtime_error("HTTP 429 Too Many Requests");
            }

            return QString::fromUtf8(response);
        }

    private:
    explicit Http(const QString& url) : netRequest(QUrl(url)), isPost(false) {}

    QNetworkRequest netRequest;
    QByteArray postData;
    QString mediaType;
    bool isPost;
    };
};
