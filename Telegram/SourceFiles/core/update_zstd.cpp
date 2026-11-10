/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/update_zstd.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDataStream>
#include <QtCore/QRegularExpression>
#include <QtCore/QtEndian>

#ifndef TDESKTOP_DISABLE_AUTOUPDATE
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <zstd.h>
#endif

#include <memory>

namespace Core::Updates::Zstd {
namespace {

constexpr auto kPublicKey = "\
-----BEGIN RSA PUBLIC KEY-----\n\
MIGJAoGBAMJbQOSFyDYYBW+f+e/W945H49UZjzoz5fq7BuSEoaUAaYOMeFSKLI/I\n\
Ya7aWiYB906oclTU8K6K+T14DJiMsYMMiY6EDic2w2F7n/8QOw4k+c0Z3ctdYjVg\n\
dXvERtOgA3AI4fSUzQJ2O85XlugVhIi3w+lN6ShafBgN9gWp4MNxAgMBAAE=\n\
-----END RSA PUBLIC KEY-----\
";

constexpr auto kPublicBetaKey = "\
-----BEGIN RSA PUBLIC KEY-----\n\
MIGJAoGBAJy9ruj3bPO0BgOtGQrDJp5/Jr1y42MMsRksXBVnVESQuXdbrDN9rGV7\n\
H4RoYrDM/hlMKFlJNj3ISjPQmK1RskfWOtW+eyS3Av/6NoGV1ejtP4njHdm1IuuT\n\
maSD7hlVmttJGd/nzai+XFUxCLmESjfHoWrjNqkeleKShEgftrc3AgMBAAE=\n\
-----END RSA PUBLIC KEY-----\
";

#ifndef TDESKTOP_DISABLE_AUTOUPDATE

bool VerifySignature(
		const QByteArray &bytes,
		const QByteArray &hash,
		const QByteArray &key) {
	const auto bio = std::unique_ptr<BIO, decltype(&BIO_free)>(
		BIO_new_mem_buf(key.constData(), int(key.size())),
		BIO_free);
	const auto rsa = std::unique_ptr<RSA, decltype(&RSA_free)>(
		bio ? PEM_read_bio_RSAPublicKey(
			bio.get(), nullptr, nullptr, nullptr) : nullptr,
		RSA_free);
	return rsa
		&& RSA_size(rsa.get()) == kSignatureSize
		&& RSA_verify(
			NID_sha1,
			reinterpret_cast<const uchar*>(hash.constData()),
			unsigned(hash.size()),
			reinterpret_cast<const uchar*>(bytes.constData()),
			kSignatureSize,
			rsa.get()) == 1;
}

#endif

} // namespace

QByteArray PublicKey() {
	return QByteArray(kPublicKey);
}

QByteArray PublicBetaKey() {
	return QByteArray(kPublicBetaKey);
}

bool IsUpdateFileName(const QString &name) {
	static const auto expression = QRegularExpression(
		QString::fromLatin1("^(tupdate|tx64upd|tarm64upd|tmacupd|tarmacupd|tlinuxupd)[0-9]+$"));
	return expression.match(name).hasMatch();
}

bool IsUpdateFile(const QByteArray &bytes) {
	return bytes.size() > kHeaderSize
		&& bytes.mid(kHeaderSize, 4) == QByteArray::fromHex("28b52ffd");
}

#ifndef TDESKTOP_DISABLE_AUTOUPDATE

std::optional<VerifiedUpdate> VerifyUpdate(
		const QByteArray &bytes,
		quint32 runningVersion,
		const QByteArray &publicKey,
		const QByteArray &publicBetaKey,
		QString *error) {
	const auto fail = [&](const char *text) -> std::optional<VerifiedUpdate> {
		if (error) {
			*error = QString::fromLatin1(text);
		}
		return std::nullopt;
	};
	if (bytes.size() > kMaxPackageSize || !IsUpdateFile(bytes)) {
		return fail("Invalid zstd update size or format.");
	}
	const auto hash = QCryptographicHash::hash(
		QByteArrayView(bytes).sliced(kSignatureSize + kHashSize),
		QCryptographicHash::Sha1);
	if (hash != bytes.mid(kSignatureSize, kHashSize)
		|| (!VerifySignature(bytes, hash, publicKey)
			&& !VerifySignature(bytes, hash, publicBetaKey))) {
		return fail("Invalid zstd update signature.");
	}
	const auto originalSize = qFromLittleEndian<quint32>(
		bytes.constData() + kSignatureSize + kHashSize);
	if (!originalSize || originalSize > kMaxUncompressedSize) {
		return fail("Invalid uncompressed update size.");
	}
	const auto payload = bytes.constData() + kHeaderSize;
	const auto payloadSize = bytes.size() - kHeaderSize;
	if (ZSTD_getFrameContentSize(payload, payloadSize) != originalSize
		|| ZSTD_findFrameCompressedSize(payload, payloadSize) != payloadSize
		|| ZSTD_getDictID_fromFrame(payload, payloadSize) != 0) {
		return fail("Invalid zstd update frame.");
	}
	auto result = VerifiedUpdate();
	result.files.resize(originalSize);
	const auto size = ZSTD_decompress(
		result.files.data(), originalSize, payload, payloadSize);
	if (ZSTD_isError(size) || size != originalSize) {
		return fail("Could not decompress zstd update.");
	}
	auto stream = QDataStream(result.files);
	stream.setVersion(QDataStream::Qt_5_1);
	auto count = quint32(0);
	stream >> result.version >> count;
	if (stream.status() != QDataStream::Ok || !count
		|| result.version <= runningVersion || result.version <= 1016
		|| result.version > 999999999) {
		return fail("Invalid or outdated zstd update version.");
	}
	return result;
}

#endif

} // namespace Core::Updates::Zstd
