/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "core/update_zstd.h"

namespace Core::Updates::Zstd {

[[nodiscard]] QByteArray PackUpdate(
	const QByteArray &files,
	const QByteArray &privateKeyPem,
	int compressionLevel,
	int threads,
	QString *error = nullptr);

} // namespace Core::Updates::Zstd
