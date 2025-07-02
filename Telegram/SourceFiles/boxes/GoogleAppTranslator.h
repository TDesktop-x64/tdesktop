#pragma once

#include "BaseTranslator.h"
#include <QRandomGenerator>

class GoogleAppTranslator : public BaseTranslator {
    Q_OBJECT

public:
    static GoogleAppTranslator* instance();

    QList<QString> getTargetLanguages() const override;

protected:
    Result translateImpl(const QString& query, const QString& fl, const QString& tl) override;

private:
    explicit GoogleAppTranslator(QObject* parent = nullptr);

    Result getResult(const QString& jsonData) const;
    QString sign(const QString& str);
    static int hash(const QString& input);
    static qint64 N(int i);
    static QString O(int i);

    QList<QString> targetLanguages;
};
