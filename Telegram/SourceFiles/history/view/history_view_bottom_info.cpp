﻿/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/view/history_view_bottom_info.h"

#include "ui/chat/message_bubble.h"
#include "ui/chat/chat_style.h"
#include "ui/text/text_options.h"
#include "ui/text/text_utilities.h"
#include "lang/lang_keys.h"
#include "history/history_item_components.h"
#include "history/history_message.h"
#include "history/history.h"
#include "history/view/history_view_message.h"
#include "history/view/history_view_cursor_state.h"
#include "history/view/history_view_react_animation.h"
#include "core/click_handler_types.h"
#include "main/main_session.h"
#include "lottie/lottie_icon.h"
#include "data/data_session.h"
#include "data/data_message_reactions.h"
#include "window/window_session_controller.h"
#include "styles/style_chat.h"
#include "styles/style_dialogs.h"

namespace HistoryView {

BottomInfo::BottomInfo(
	not_null<::Data::Reactions*> reactionsOwner,
	Data &&data)
: _reactionsOwner(reactionsOwner)
, _data(std::move(data)) {
	layout();
}

BottomInfo::~BottomInfo() = default;

void BottomInfo::update(Data &&data, int availableWidth) {
	_data = std::move(data);
	layout();
	if (width() > 0) {
		resizeGetHeight(std::min(maxWidth(), availableWidth));
	}
}

int BottomInfo::countReactionsMaxWidth() const {
	auto result = 0;
	for (const auto &reaction : _reactions) {
		result += st::reactionInfoSize;
		if (reaction.countTextWidth > 0) {
			result += st::reactionInfoSkip
				+ reaction.countTextWidth
				+ st::reactionInfoDigitSkip;
		} else {
			result += st::reactionInfoBetween;
		}
	}
	if (result) {
		result += (st::reactionInfoSkip - st::reactionInfoBetween);
	}
	return result;
}

int BottomInfo::countReactionsHeight(int newWidth) const {
	const auto left = 0;
	auto x = 0;
	auto y = 0;
	auto widthLeft = newWidth;
	for (const auto &reaction : _reactions) {
		const auto add = (reaction.countTextWidth > 0)
			? st::reactionInfoDigitSkip
			: st::reactionInfoBetween;
		const auto width = st::reactionInfoSize
			+ (reaction.countTextWidth > 0
				? (st::reactionInfoSkip + reaction.countTextWidth)
				: 0);
		if (x > left && widthLeft < width) {
			x = left;
			y += st::msgDateFont->height;
			widthLeft = newWidth;
		}
		x += width + add;
		widthLeft -= width + add;
	}
	if (x > left) {
		y += st::msgDateFont->height;
	}
	return y;
}

int BottomInfo::firstLineWidth() const {
	if (height() == minHeight()) {
		return width();
	}
	return maxWidth() - _reactionsMaxWidth;
}

bool BottomInfo::isWide() const {
	return (_data.flags & Data::Flag::Edited)
		|| !_data.author.isEmpty()
		|| !_views.isEmpty()
		|| !_replies.isEmpty()
		|| !_reactions.empty();
}

TextState BottomInfo::textState(
		not_null<const HistoryItem*> item,
		QPoint position) const {
	auto result = TextState(item);
	if (const auto link = revokeReactionLink(item, position)) {
		result.link = link;
		return result;
	}
	const auto textWidth = _authorEditedDate.maxWidth() + _type.maxWidth();
	auto withTicksWidth = textWidth;
	if (_data.flags & (Data::Flag::OutLayout | Data::Flag::Sending)) {
		withTicksWidth += st::historySendStateSpace;
	}
	const auto inTime = QRect(
		width() - withTicksWidth,
		0,
		withTicksWidth,
		st::msgDateFont->height
	).contains(position);
	if (inTime) {
		result.cursor = CursorState::Date;
	}
	return result;
}

ClickHandlerPtr BottomInfo::revokeReactionLink(
		not_null<const HistoryItem*> item,
		QPoint position) const {
	if (_reactions.empty()) {
		return nullptr;
	}
	auto left = 0;
	auto top = 0;
	auto available = width();
	if (height() != minHeight()) {
		available = std::min(available, _reactionsMaxWidth);
		left += width() - available;
		top += st::msgDateFont->height;
	}
	auto x = left;
	auto y = top;
	auto widthLeft = available;
	for (const auto &reaction : _reactions) {
		const auto chosen = (reaction.emoji == _data.chosenReaction);
		const auto add = (reaction.countTextWidth > 0)
			? st::reactionInfoDigitSkip
			: st::reactionInfoBetween;
		const auto width = st::reactionInfoSize
			+ (reaction.countTextWidth > 0
				? (st::reactionInfoSkip + reaction.countTextWidth)
				: 0);
		if (x > left && widthLeft < width) {
			x = left;
			y += st::msgDateFont->height;
			widthLeft = available;
		}
		const auto image = QRect(
			x,
			y,
			st::reactionInfoSize,
			st::msgDateFont->height);
		if (chosen && image.contains(position)) {
			if (!_revokeLink) {
				_revokeLink = revokeReactionLink(item);
			}
			return _revokeLink;
		}
		x += width + add;
		widthLeft -= width + add;
	}
	return nullptr;
}

ClickHandlerPtr BottomInfo::revokeReactionLink(
		not_null<const HistoryItem*> item) const {
	const auto itemId = item->fullId();
	const auto sessionId = item->history()->session().uniqueId();
	return std::make_shared<LambdaClickHandler>([=](
			ClickContext context) {
		const auto my = context.other.value<ClickHandlerContext>();
		if (const auto controller = my.sessionWindow.get()) {
			if (controller->session().uniqueId() == sessionId) {
				auto &owner = controller->session().data();
				if (const auto item = owner.message(itemId)) {
					const auto chosen = item->chosenReaction();
					if (!chosen.isEmpty()) {
						item->toggleReaction(chosen);
					}
				}
			}
		}
	});
}

bool BottomInfo::isSignedAuthorElided() const {
	return _authorElided;
}

void BottomInfo::paint(
		Painter &p,
		QPoint position,
		int outerWidth,
		bool unread,
		bool inverted,
		const PaintContext &context) const {
	const auto st = context.st;
	const auto stm = context.messageStyle();

	auto right = position.x() + width();
	const auto firstLineBottom = position.y() + st::msgDateFont->height;
	if (_data.flags & Data::Flag::OutLayout) {
		const auto &icon = (_data.flags & Data::Flag::Sending)
			? (inverted
				? st->historySendingInvertedIcon()
				: st->historySendingIcon())
			: unread
			? (inverted
				? st->historySentInvertedIcon()
				: stm->historySentIcon)
			: (inverted
				? st->historyReceivedInvertedIcon()
				: stm->historyReceivedIcon);
		icon.paint(
			p,
			QPoint(right, firstLineBottom) + st::historySendStatePosition,
			outerWidth);
		right -= st::historySendStateSpace;
	}

	const auto authorEditedWidth = _authorEditedDate.maxWidth();
	right -= authorEditedWidth;
	_authorEditedDate.drawLeft(
		p,
		right,
		position.y(),
		authorEditedWidth,
		outerWidth);

	const auto typeWidth = _type.maxWidth();
	right -= typeWidth;
	auto originalPen = p.pen();
	p.setPen(Qt::darkBlue);
	_type.drawLeft(
			p,
			right,
			position.y(),
			typeWidth,
			outerWidth);
	p.setPen(originalPen);

	if (_data.flags & Data::Flag::Pinned) {
		const auto &icon = inverted
			? st->historyPinInvertedIcon()
			: stm->historyPinIcon;
		right -= st::historyPinWidth;
		icon.paint(
			p,
			right,
			firstLineBottom + st::historyPinTop,
			outerWidth);
	}
	if (!_views.isEmpty()) {
		const auto viewsWidth = _views.maxWidth();
		right -= st::historyViewsSpace + viewsWidth;
		_views.drawLeft(p, right, position.y(), viewsWidth, outerWidth);

		const auto &icon = inverted
			? st->historyViewsInvertedIcon()
			: stm->historyViewsIcon;
		right -= st::historyViewsWidth;
		icon.paint(
			p,
			right,
			firstLineBottom + st::historyViewsTop,
			outerWidth);
	}
	if (!_replies.isEmpty()) {
		const auto repliesWidth = _replies.maxWidth();
		right -= st::historyViewsSpace + repliesWidth;
		_replies.drawLeft(p, right, position.y(), repliesWidth, outerWidth);

		const auto &icon = inverted
			? st->historyRepliesInvertedIcon()
			: stm->historyRepliesIcon;
		right -= st::historyViewsWidth;
		icon.paint(
			p,
			right,
			firstLineBottom + st::historyViewsTop,
			outerWidth);
	}
	if ((_data.flags & Data::Flag::Sending)
		&& !(_data.flags & Data::Flag::OutLayout)) {
		right -= st::historySendStateSpace;
		const auto &icon = inverted
			? st->historyViewsSendingInvertedIcon()
			: st->historyViewsSendingIcon();
		icon.paint(
			p,
			right,
			firstLineBottom + st::historyViewsTop,
			outerWidth);
	}
	if (!_reactions.empty()) {
		auto left = position.x();
		auto top = position.y();
		auto available = width();
		if (height() != minHeight()) {
			available = std::min(available, _reactionsMaxWidth);
			left += width() - available;
			top += st::msgDateFont->height;
		}
		paintReactions(p, position, left, top, available, context);
	}
}

void BottomInfo::paintReactions(
		Painter &p,
		QPoint origin,
		int left,
		int top,
		int availableWidth,
		const PaintContext &context) const {
	struct SingleAnimation {
		not_null<Reactions::Animation*> animation;
		QRect target;
	};
	std::vector<SingleAnimation> animations;

	auto x = left;
	auto y = top;
	auto widthLeft = availableWidth;
	for (const auto &reaction : _reactions) {
		if (context.reactionInfo
			&& reaction.animation
			&& reaction.animation->finished()) {
			reaction.animation = nullptr;
		}
		const auto animating = (reaction.animation != nullptr);
		const auto add = (reaction.countTextWidth > 0)
			? st::reactionInfoDigitSkip
			: st::reactionInfoBetween;
		const auto width = st::reactionInfoSize
			+ (reaction.countTextWidth > 0
				? (st::reactionInfoSkip + reaction.countTextWidth)
				: 0);
		if (x > left && widthLeft < width) {
			x = left;
			y += st::msgDateFont->height;
			widthLeft = availableWidth;
		}
		if (reaction.image.isNull()) {
			reaction.image = _reactionsOwner->resolveImageFor(
				reaction.emoji,
				::Data::Reactions::ImageSize::BottomInfo);
		}
		const auto image = QRect(
			x + (st::reactionInfoSize - st::reactionInfoImage) / 2,
			y + (st::msgDateFont->height - st::reactionInfoImage) / 2,
			st::reactionInfoImage,
			st::reactionInfoImage);
		const auto skipImage = animating
			&& (reaction.count < 2 || !reaction.animation->flying());
		if (!reaction.image.isNull() && !skipImage) {
			p.drawImage(image.topLeft(), reaction.image);
		}
		if (animating) {
			animations.push_back({
				.animation = reaction.animation.get(),
				.target = image,
			});
		}
		if (reaction.countTextWidth > 0) {
			p.drawText(
				x + st::reactionInfoSize + st::reactionInfoSkip,
				y + st::msgDateFont->ascent,
				reaction.countText);
		}
		x += width + add;
		widthLeft -= width + add;
	}
	if (!animations.empty()) {
		context.reactionInfo->effectPaint = [=](QPainter &p) {
			auto result = QRect();
			for (const auto &single : animations) {
				const auto area = single.animation->paintGetArea(
					p,
					origin,
					single.target);
				result = result.isEmpty() ? area : result.united(area);
			}
			return result;
		};
	}
}

QSize BottomInfo::countCurrentSize(int newWidth) {
	if (newWidth >= maxWidth()) {
		return optimalSize();
	}
	const auto noReactionsWidth = maxWidth() - _reactionsMaxWidth;
	accumulate_min(newWidth, std::max(noReactionsWidth, _reactionsMaxWidth));
	return QSize(
		newWidth,
		st::msgDateFont->height + countReactionsHeight(newWidth));
}

void BottomInfo::layout() {
	layoutDateText();
	layoutViewsText();
	layoutRepliesText();
	layoutReactionsText();
	initDimensions();
}

void BottomInfo::layoutDateText() {
	const auto edited = (_data.flags & Data::Flag::Edited)
		? (tr::lng_edited(tr::now) + ' ')
		: QString();
	const auto author = _data.author;
	const auto prefix = !author.isEmpty() ? qsl(", ") : QString();
	const auto date = edited + _data.date.toString(cTimeFormat()) + _data.msgId;
	const auto afterAuthor = prefix + date;
	const auto afterAuthorWidth = st::msgDateFont->width(afterAuthor);
	const auto authorWidth = st::msgDateFont->width(author);
	const auto maxWidth = st::maxSignatureSize;
	_authorElided = !author.isEmpty()
		&& (authorWidth + afterAuthorWidth > maxWidth);
	const auto name = _authorElided
		? st::msgDateFont->elided(author, maxWidth - afterAuthorWidth)
		: author;
	const auto full = (_data.flags & Data::Flag::Sponsored)
		? tr::lng_sponsored(tr::now)
		: (_data.flags & Data::Flag::Imported)
		? (date + ' ' + tr::lng_imported(tr::now))
		: name.isEmpty()
		? date
		: (name + afterAuthor);
	_type.setText(st::msgDateTextStyle, _data.type, Ui::NameTextOptions());
	_authorEditedDate.setText(
		st::msgDateTextStyle,
		full,
		Ui::NameTextOptions());
}

void BottomInfo::layoutViewsText() {
	if (!_data.views || (_data.flags & Data::Flag::Sending)) {
		_views.clear();
		return;
	}
	_views.setText(
		st::msgDateTextStyle,
		Lang::FormatCountToShort(std::max(*_data.views, 1)).string,
		Ui::NameTextOptions());
}

void BottomInfo::layoutRepliesText() {
	if (!_data.replies
		|| !*_data.replies
		|| (_data.flags & Data::Flag::RepliesContext)
		|| (_data.flags & Data::Flag::Sending)) {
		_replies.clear();
		return;
	}
	_replies.setText(
		st::msgDateTextStyle,
		Lang::FormatCountToShort(*_data.replies).string,
		Ui::NameTextOptions());
}

void BottomInfo::layoutReactionsText() {
	if (_data.reactions.empty()) {
		_reactions.clear();
		return;
	}
	auto sorted = ranges::view::all(
		_data.reactions
	) | ranges::view::transform([](const auto &pair) {
		return std::make_pair(pair.first, pair.second);
	}) | ranges::to_vector;
	ranges::sort(sorted, std::greater<>(), &std::pair<QString, int>::second);

	auto reactions = std::vector<Reaction>();
	reactions.reserve(sorted.size());
	for (const auto &[emoji, count] : sorted) {
		const auto i = ranges::find(_reactions, emoji, &Reaction::emoji);
		reactions.push_back((i != end(_reactions))
			? std::move(*i)
			: prepareReactionWithEmoji(emoji));
		setReactionCount(reactions.back(), count);
	}
	_reactions = std::move(reactions);
}

QSize BottomInfo::countOptimalSize() {
	auto width = 0;
	if (_data.flags & (Data::Flag::OutLayout | Data::Flag::Sending)) {
		width += st::historySendStateSpace;
	}
	width += _type.maxWidth();
	width += _authorEditedDate.maxWidth();
	if (!_views.isEmpty()) {
		width += st::historyViewsSpace
			+ _views.maxWidth()
			+ st::historyViewsWidth;
	}
	if (!_replies.isEmpty()) {
		width += st::historyViewsSpace
			+ _replies.maxWidth()
			+ st::historyViewsWidth;
	}
	if (_data.flags & Data::Flag::Pinned) {
		width += st::historyPinWidth;
	}
	_reactionsMaxWidth = countReactionsMaxWidth();
	width += _reactionsMaxWidth;
	return QSize(width, st::msgDateFont->height);
}

BottomInfo::Reaction BottomInfo::prepareReactionWithEmoji(
		const QString &emoji) {
	auto result = Reaction{ .emoji = emoji };
	_reactionsOwner->preloadImageFor(emoji);
	return result;
}

void BottomInfo::setReactionCount(Reaction &reaction, int count) {
	if (reaction.count == count) {
		return;
	}
	reaction.count = count;
	reaction.countText = (count > 1)
		? Lang::FormatCountToShort(count).string
		: QString();
	reaction.countTextWidth = (count > 1)
		? st::msgDateFont->width(reaction.countText)
		: 0;
}

void BottomInfo::animateReaction(
		ReactionAnimationArgs &&args,
		Fn<void()> repaint) {
	const auto i = ranges::find(_reactions, args.emoji, &Reaction::emoji);
	if (i == end(_reactions)) {
		return;
	}
	i->animation = std::make_unique<Reactions::Animation>(
		_reactionsOwner,
		args.translated(QPoint(width(), height())),
		std::move(repaint),
		st::reactionInfoImage);
}

auto BottomInfo::takeReactionAnimations()
-> base::flat_map<QString, std::unique_ptr<Reactions::Animation>> {
	auto result = base::flat_map<
		QString,
		std::unique_ptr<Reactions::Animation>>();
	for (auto &reaction : _reactions) {
		if (reaction.animation) {
			result.emplace(reaction.emoji, std::move(reaction.animation));
		}
	}
	return result;
}

void BottomInfo::continueReactionAnimations(base::flat_map<
		QString,
		std::unique_ptr<Reactions::Animation>> animations) {
	for (auto &[emoji, animation] : animations) {
		const auto i = ranges::find(_reactions, emoji, &Reaction::emoji);
		if (i != end(_reactions)) {
			i->animation = std::move(animation);
		}
	}
}

BottomInfo::Data BottomInfoDataFromMessage(not_null<Message*> message) {
	using Flag = BottomInfo::Data::Flag;
	const auto item = message->message();

	auto result = BottomInfo::Data();
	result.date = message->dateTime();
	if (message->embedReactionsInBottomInfo()) {
		result.reactions = item->reactions();
		result.chosenReaction = item->chosenReaction();
	}
	if (message->hasOutLayout()) {
		result.flags |= Flag::OutLayout;
	}
	if (message->context() == Context::Replies) {
		result.flags |= Flag::RepliesContext;
	}
	if (item->isSponsored()) {
		result.flags |= Flag::Sponsored;
	}
	if (item->isPinned() && message->context() != Context::Pinned) {
		result.flags |= Flag::Pinned;
	}
	if (const auto msgsigned = item->Get<HistoryMessageSigned>()) {
		 if (!msgsigned->isAnonymousRank) {
			result.author = msgsigned->author;
		 }
	}
	if (!item->hideEditedBadge()) {
		if (const auto edited = message->displayedEditBadge()) {
			result.flags |= Flag::Edited;
		}
	}
	if (const auto views = item->Get<HistoryMessageViews>()) {
		if (views->views.count >= 0) {
			result.views = views->views.count;
		}
		if (views->replies.count >= 0 && !views->commentsMegagroupId) {
			result.replies = views->replies.count;
		}
	}
	if (item->isSending() || item->hasFailed()) {
		result.flags |= Flag::Sending;
	}
	const auto forwarded = item->Get<HistoryMessageForwarded>();
	if (forwarded && forwarded->imported) {
		result.flags |= Flag::Imported;
	}
	// We don't want to pass and update it in Date for now.
	//if (item->unread()) {
	//	result.flags |= Flag::Unread;
	//}
	if (cShowMessagesID()) {
		if (item->fullId().msg > 0)
			result.msgId = QString(" (%1)").arg(item->fullId().msg.bare);
	}
	if (item->from()->isChannel()) {
		result.type = QString("[Channel]");
	}
	if (item->from()->isMegagroup()) {
		result.type = QString("[MegaGroup]");
	}
	return result;
}

} // namespace HistoryView
