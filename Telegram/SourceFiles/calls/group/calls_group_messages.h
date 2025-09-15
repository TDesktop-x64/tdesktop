/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

namespace Calls {
class GroupCall;
} // namespace Calls

namespace Data {
class GroupCall;
} // namespace Data

namespace MTP {
class Sender;
struct Response;
} // namespace MTP

namespace Calls::Group {

class Messages final {
public:
	Messages(not_null<GroupCall*> call, not_null<MTP::Sender*> api);

	void send(TextWithTags text);

	void received(const MTPDupdateGroupCallMessage &data);
	void received(const MTPDupdateGroupCallEncryptedMessage &data);

private:
	struct Message {
		int id = 0;
		TimeId date = 0;
		not_null<PeerData*> peer;
		TextWithEntities text;
		bool failed = false;
	};

	[[nodiscard]] bool ready() const;
	void sendPending();

	void received(const MTPPeer &from, const MTPTextWithEntities &message);
	void sent(int id, const MTP::Response &response);
	void failed(int id);

	const not_null<GroupCall*> _call;
	const not_null<MTP::Sender*> _api;

	Data::GroupCall *_real = nullptr;

	std::vector<TextWithTags> _pending;

	std::vector<Message> _messages;

	int _autoincrementId = 0;

	rpl::lifetime _lifetime;

};

} // namespace Calls::Group
