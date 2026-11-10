/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <optional>

namespace Core::Updates::Zstd {

inline constexpr auto kSignatureSize = 128;
inline constexpr auto kHashSize = 20;
inline constexpr auto kHeaderSize = kSignatureSize + kHashSize + 4;
inline constexpr auto kMaxPackageSize = 256 * 1024 * 1024;
inline constexpr auto kMaxUncompressedSize = 1024 * 1024 * 1024;

struct VerifiedUpdate {
	quint32 version = 0;
	QByteArray files;
};

[[nodiscard]] QByteArray PublicKey();
[[nodiscard]] QByteArray PublicBetaKey();
[[nodiscard]] bool IsUpdateFileName(const QString &name);
[[nodiscard]] bool IsUpdateFile(const QByteArray &bytes);
[[nodiscard]] std::optional<VerifiedUpdate> VerifyUpdate(
	const QByteArray &bytes,
	quint32 runningVersion,
	const QByteArray &publicKey,
	const QByteArray &publicBetaKey,
	QString *error = nullptr);

} // namespace Core::Updates::Zstd
