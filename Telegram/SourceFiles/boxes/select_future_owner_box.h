/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

class ChannelData;
class PeerData;
class UserData;

namespace Ui {
class GenericBox;
class Show;
} // namespace Ui

void SelectFutureOwnerbox(
	not_null<Ui::GenericBox*> box,
	not_null<ChannelData*> channel,
	not_null<UserData*> user);
