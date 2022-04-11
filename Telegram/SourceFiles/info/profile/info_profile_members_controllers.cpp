/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "info/profile/info_profile_members_controllers.h"

#include "boxes/peers/edit_participants_box.h"
#include "data/data_chat.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "styles/style_info.h"

namespace Info {
namespace Profile {

MemberListRow::MemberListRow(
	not_null<UserData*> user,
	Type type)
: PeerListRowWithLink(user)
, _type(type) {
	PeerListRowWithLink::setActionLink(_type.adminRank);
}

void MemberListRow::setType(Type type) {
	_type = type;
	PeerListRowWithLink::setActionLink(_type.adminRank);
}

bool MemberListRow::rightActionDisabled() const {
	return !canRemove();
}

QSize MemberListRow::rightActionSize() const {
	return canRemove()
		? QRect(
			QPoint(),
			st::infoMembersRemoveIcon.size()).marginsAdded(
				st::infoMembersRemoveIconMargins).size()
		: PeerListRowWithLink::rightActionSize();
}

void MemberListRow::rightActionPaint(
		Painter &p,
		int x,
		int y,
		int outerWidth,
		bool selected,
		bool actionSelected) {
	if (_type.canRemove && selected) {
		x += st::infoMembersRemoveIconMargins.left();
		y += st::infoMembersRemoveIconMargins.top();
		(actionSelected
			? st::infoMembersRemoveIconOver
			: st::infoMembersRemoveIcon).paint(p, x, y, outerWidth);
	} else {
		PeerListRowWithLink::rightActionPaint(
			p,
			x,
			y,
			outerWidth,
			selected,
			actionSelected);
	}
}

QMargins MemberListRow::rightActionMargins() const {
	return canRemove()
		? QMargins()
		: PeerListRowWithLink::rightActionMargins();
}

// Source from kotatogram
int MemberListRow::adminTitleWidth() const {
	return st::normalFont->width(_type.adminTitle);
}

not_null<UserData*> MemberListRow::user() const {
	return peer()->asUser();
}

// Source from kotatogram
void MemberListRow::paintAdminTitle(
		Painter &p,
		int x,
		int y,
		int outerWidth,
		bool selected) {
	p.drawTextLeft(x, y, outerWidth, _type.adminTitle, adminTitleWidth());
}

void MemberListRow::refreshStatus() {
	if (user()->isBot()) {
		const auto seesAllMessages = (user()->botInfo->readsAllHistory
			|| _type.rights != Rights::Normal);
		setCustomStatus(seesAllMessages
			? tr::lng_status_bot_reads_all(tr::now)
			: tr::lng_status_bot_not_reads_all(tr::now));
	} else {
		PeerListRow::refreshStatus();
	}
}

bool MemberListRow::canRemove() const {
	return _type.canRemove;
}

std::unique_ptr<PeerListController> CreateMembersController(
		not_null<Window::SessionNavigation*> navigation,
		not_null<PeerData*> peer) {
	return std::make_unique<ParticipantsBoxController>(
		navigation,
		peer,
		ParticipantsBoxController::Role::Profile);
}

} // namespace Profile
} // namespace Info
