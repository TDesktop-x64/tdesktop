/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "calls/group/calls_group_message_field.h"

#include "boxes/premium_preview_box.h"
#include "calls/group/calls_group_messages.h"
#include "chat_helpers/compose/compose_show.h"
#include "chat_helpers/emoji_suggestions_widget.h"
#include "chat_helpers/message_field.h"
#include "chat_helpers/tabbed_panel.h"
#include "chat_helpers/tabbed_selector.h"
#include "core/ui_integration.h"
#include "data/stickers/data_custom_emoji.h"
#include "data/stickers/data_stickers.h"
#include "data/data_document.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/controls/emoji_button.h"
#include "ui/controls/send_button.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/scroll_area.h"
#include "ui/painter.h"
#include "ui/ui_utility.h"
#include "ui/userpic_view.h"
#include "styles/style_calls.h"
#include "styles/style_chat_helpers.h"
#include "styles/style_chat.h"
#include "styles/style_media_view.h"

namespace Calls::Group {
namespace {

constexpr auto kWarnLimit = 24;
constexpr auto kErrorLimit = 99;

} // namespace

MessageField::MessageField(
	not_null<QWidget*> parent,
	std::shared_ptr<ChatHelpers::Show> show,
	PeerData *peer)
: _parent(parent)
, _show(std::move(show))
, _wrap(std::make_unique<Ui::RpWidget>(_parent))
, _limit(128) {
	createControls(peer);
}

void MessageField::createControls(PeerData *peer) {
	setupBackground();

	const auto &st = st::storiesComposeControls;
	_field = Ui::CreateChild<Ui::InputField>(
		_wrap.get(),
		st.field,
		Ui::InputField::Mode::MultiLine,
		tr::lng_message_ph());
	_field->setMaxLength(_limit + kErrorLimit);
	_field->setMinHeight(
		st::historySendSize.height() - 2 * st::historySendPadding);
	_field->setMaxHeight(st::historyComposeFieldMaxHeight);
	_field->setDocumentMargin(4.);
	_field->setAdditionalMargin(style::ConvertScale(4) - 4);

	const auto show = _show;
	const auto allow = [=](not_null<DocumentData*> emoji) {
		return peer
			? Data::AllowEmojiWithoutPremium(peer, emoji)
			: show->session().premium();
	};
	InitMessageFieldHandlers({
		.session = &show->session(),
		.show = show,
		.field = _field,
		.customEmojiPaused = [=] {
			return show->paused(ChatHelpers::PauseReason::Layer);
		},
		.allowPremiumEmoji = allow,
		.fieldStyle = &st.files.caption,
		.allowMarkdownTags = {
			Ui::InputField::kTagBold,
			Ui::InputField::kTagItalic,
			Ui::InputField::kTagUnderline,
			Ui::InputField::kTagStrikeOut,
			Ui::InputField::kTagSpoiler,
		},
	});
	Ui::Emoji::SuggestionsController::Init(
		_parent,
		_field,
		&_show->session(),
		{
			.suggestCustomEmoji = true,
			.allowCustomWithoutPremium = allow,
			.st = &st.suggestions,
		});

	_send = Ui::CreateChild<Ui::SendButton>(_wrap.get(), st.send);
	_send->show();

	using Selector = ChatHelpers::TabbedSelector;
	using Descriptor = ChatHelpers::TabbedPanelDescriptor;
	_emojiPanel = Ui::CreateChild<ChatHelpers::TabbedPanel>(
		_parent,
		ChatHelpers::TabbedPanelDescriptor{
			.ownedSelector = object_ptr<Selector>(
				nullptr,
				ChatHelpers::TabbedSelectorDescriptor{
					.show = _show,
					.st = st.tabbed,
					.level = ChatHelpers::PauseReason::Layer,
					.mode = ChatHelpers::TabbedSelector::Mode::EmojiOnly,
					.features = {
						.stickersSettings = false,
						.openStickerSets = false,
					},
				}),
		});
	_emojiPanel->setDesiredHeightValues(
		1.,
		st::emojiPanMinHeight / 2,
		st::emojiPanMinHeight);
	_emojiPanel->hide();
	_emojiPanel->selector()->setCurrentPeer(peer);
	_emojiPanel->selector()->emojiChosen(
	) | rpl::start_with_next([=](ChatHelpers::EmojiChosen data) {
		Ui::InsertEmojiAtCursor(_field->textCursor(), data.emoji);
	}, lifetime());
	_emojiPanel->selector()->customEmojiChosen(
	) | rpl::start_with_next([=](ChatHelpers::FileChosen data) {
		const auto info = data.document->sticker();
		if (info
			&& info->setType == Data::StickersType::Emoji
			&& !_show->session().premium()) {
			ShowPremiumPreviewBox(
				_show,
				PremiumFeature::AnimatedEmoji);
		} else {
			Data::InsertCustomEmoji(_field, data.document);
		}
	}, lifetime());

	_emojiToggle = Ui::CreateChild<Ui::EmojiButton>(_wrap.get(), st.emoji);
	_emojiToggle->show();

	_emojiToggle->installEventFilter(_emojiPanel);
	_emojiToggle->addClickHandler([=] {
		_emojiPanel->toggleAnimated();
	});

	_width.value(
	) | rpl::filter(
		rpl::mappers::_1 > 0
	) | rpl::start_with_next([=](int newWidth) {
		const auto fieldWidth = newWidth
			- st::historySendPadding
			- _emojiToggle->width()
			- _send->width();
		_field->resizeToWidth(fieldWidth);
		_field->moveToLeft(
			st::historySendPadding,
			st::historySendPadding,
			newWidth);
		updateWrapSize(newWidth);
	}, _lifetime);

	rpl::combine(
		_width.value(),
		_field->heightValue()
	) | rpl::start_with_next([=](int width, int height) {
		if (width <= 0) {
			return;
		}
		const auto minHeight = st::historySendSize.height()
			- 2 * st::historySendPadding;
		_send->moveToRight(0, height - minHeight, width);
		_emojiToggle->moveToRight(_send->width(), height - minHeight, width);
		updateWrapSize();
	}, _lifetime);

	_field->cancelled() | rpl::start_with_next([=] {
		_closeRequests.fire({});
	}, _lifetime);

	const auto updateLimitPosition = [=](QSize parent, QSize label) {
		const auto skip = st::historySendPadding;
		return QPoint(parent.width() - label.width() - skip, skip);
	};
	Ui::AddLengthLimitLabel(_field, _limit, {
		.customParent = _wrap.get(),
		.customUpdatePosition = updateLimitPosition,
	});

	rpl::merge(
		_field->submits() | rpl::to_empty,
		_send->clicks() | rpl::to_empty
	) | rpl::start_with_next([=] {
		auto text = _field->getTextWithTags();
		if (text.text.size() <= _limit) {
			_submitted.fire(std::move(text));
		}
	}, _lifetime);
}

void MessageField::updateEmojiPanelGeometry() {
	const auto parent = _emojiPanel->parentWidget();
	const auto global = _emojiToggle->mapToGlobal({ 0, 0 });
	const auto local = parent->mapFromGlobal(global);
	_emojiPanel->moveBottomRight(
		local.y(),
		local.x() + _emojiToggle->width() * 3);
}

void MessageField::setupBackground() {
	_wrap->paintRequest() | rpl::start_with_next([=] {
		const auto radius = st::historySendSize.height() / 2.;
		auto p = QPainter(_wrap.get());
		auto hq = PainterHighQualityEnabler(p);

		p.setPen(Qt::NoPen);
		p.setBrush(st::storiesComposeBg);
		p.drawRoundedRect(_wrap->rect(), radius, radius);
	}, _lifetime);
}

void MessageField::resizeToWidth(int newWidth) {
	_width = newWidth;
	if (_wrap->isHidden()) {
		Ui::SendPendingMoveResizeEvents(_wrap.get());
	}
	updateEmojiPanelGeometry();
}

void MessageField::move(int x, int y) {
	_wrap->move(x, y);
	if (_cache) {
		_cache->move(x, y);
	}
}

void MessageField::toggle(bool shown) {
	if (_shown == shown) {
		return;
	} else if (shown) {
		Assert(_width.current() > 0);
		Ui::SendPendingMoveResizeEvents(_wrap.get());
	}
	_shown = shown;
	if (!anim::Disabled()) {
		if (!_cache) {
			auto image = Ui::GrabWidgetToImage(_wrap.get());
			_cache = std::make_unique<Ui::RpWidget>(_parent);
			const auto raw = _cache.get();
			raw->paintRequest() | rpl::start_with_next([=] {
				auto p = QPainter(raw);
				auto hq = PainterHighQualityEnabler(p);
				const auto scale = raw->height() / float64(_wrap->height());
				const auto target = _wrap->rect();
				const auto center = target.center();
				p.translate(center);
				p.scale(scale, scale);
				p.translate(-center);
				p.drawImage(target, image);
			}, raw->lifetime());
			raw->show();
			raw->move(_wrap->pos());
			raw->resize(_wrap->width(), 0);

			_wrap->hide();
		}
		_shownAnimation.start(
			[=] { shownAnimationCallback(); },
			shown ? 0. : 1.,
			shown ? 1. : 0.,
			st::slideWrapDuration,
			anim::easeOutCirc);
	}
	shownAnimationCallback();
}

void MessageField::raise() {
	_wrap->raise();
	if (_cache) {
		_cache->raise();
	}
}

void MessageField::updateWrapSize(int widthOverride) {
	const auto width = widthOverride ? widthOverride : _wrap->width();
	const auto height = _field->height() + 2 * st::historySendPadding;
	_wrap->resize(width, height);
	updateHeight();
}

void MessageField::updateHeight() {
	_height = int(base::SafeRound(
		_shownAnimation.value(_shown ? 1. : 0.) * _wrap->height()));
}

void MessageField::shownAnimationCallback() {
	updateHeight();
	if (_shownAnimation.animating()) {
		Assert(_cache != nullptr);
		_cache->resize(_cache->width(), _height.current());
		_cache->update();
	} else if (_shown) {
		_cache = nullptr;
		_wrap->show();
		_field->setFocusFast();
	} else {
		_closed.fire({});
	}
}

int MessageField::height() const {
	return _height.current();
}

rpl::producer<int> MessageField::heightValue() const {
	return _height.value();
}

rpl::producer<TextWithTags> MessageField::submitted() const {
	return _submitted.events();
}

rpl::producer<> MessageField::closeRequests() const {
	return _closeRequests.events();
}

rpl::producer<> MessageField::closed() const {
	return _closed.events();
}

rpl::lifetime &MessageField::lifetime() {
	return _lifetime;
}

} // namespace Calls::Group
