/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Calls::Group {

[[nodiscard]] QByteArray SerializeMessage(const MTPTextWithEntities &text);
[[nodiscard]] std::optional<MTPTextWithEntities> DeserializeMessage(
	const QByteArray &data);

} // namespace Calls::Group
