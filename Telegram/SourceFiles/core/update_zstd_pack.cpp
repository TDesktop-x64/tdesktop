/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "core/update_zstd_pack.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QtEndian>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <zstd.h>

#include <memory>

namespace Core::Updates::Zstd {

QByteArray PackUpdate(
		const QByteArray &files,
		const QByteArray &privateKeyPem,
		int compressionLevel,
		int threads,
		QString *error) {
	const auto fail = [&](const char *text) {
		if (error) {
			*error = QString::fromLatin1(text);
		}
		return QByteArray();
	};
	if (files.isEmpty() || files.size() > kMaxUncompressedSize
		|| compressionLevel < 1 || compressionLevel > 19
		|| threads < 1 || threads > 32) {
		return fail("Invalid package size or compression settings.");
	}
	const auto bio = std::unique_ptr<BIO, decltype(&BIO_free)>(
		BIO_new_mem_buf(privateKeyPem.constData(), int(privateKeyPem.size())),
		BIO_free);
	const auto rsa = std::unique_ptr<RSA, decltype(&RSA_free)>(
		bio ? PEM_read_bio_RSAPrivateKey(
			bio.get(), nullptr, nullptr, nullptr) : nullptr,
		RSA_free);
	if (!rsa || RSA_size(rsa.get()) != kSignatureSize) {
		return fail("A v1 RSA-1024 update private key is required.");
	}
	const auto context = std::unique_ptr<ZSTD_CCtx, decltype(&ZSTD_freeCCtx)>(
		ZSTD_createCCtx(),
		ZSTD_freeCCtx);
	if (!context
		|| ZSTD_isError(ZSTD_CCtx_setParameter(
			context.get(),
			ZSTD_c_compressionLevel,
			compressionLevel))
		|| ZSTD_isError(ZSTD_CCtx_setParameter(
			context.get(),
			ZSTD_c_nbWorkers,
			threads == 1 ? 0 : threads))
		|| ZSTD_isError(ZSTD_CCtx_setParameter(
			context.get(),
			ZSTD_c_checksumFlag,
			1))) {
		return fail("Could not configure zstd compression.");
	}
	auto payload = QByteArray();
	payload.resize(qsizetype(ZSTD_compressBound(files.size())));
	const auto size = ZSTD_compress2(
		context.get(),
		payload.data(),
		payload.size(),
		files.constData(),
		files.size());
	if (ZSTD_isError(size)) {
		return fail(ZSTD_getErrorName(size));
	} else if (size > kMaxPackageSize - kHeaderSize) {
		return fail("Compressed update exceeds the download size limit.");
	}
	payload.resize(qsizetype(size));
	auto body = QByteArray(4, Qt::Uninitialized);
	qToLittleEndian(quint32(files.size()), body.data());
	body += payload;
	const auto hash = QCryptographicHash::hash(body, QCryptographicHash::Sha1);
	auto signature = QByteArray(kSignatureSize, Qt::Uninitialized);
	auto signatureSize = unsigned(0);
	if (RSA_sign(
			NID_sha1,
			reinterpret_cast<const uchar*>(hash.constData()),
			unsigned(hash.size()),
			reinterpret_cast<uchar*>(signature.data()),
			&signatureSize,
			rsa.get()) != 1
		|| signatureSize != kSignatureSize) {
		return fail("Could not sign update.");
	}
	return signature + hash + body;
}

} // namespace Core::Updates::Zstd
