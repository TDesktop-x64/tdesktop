/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/effects/animations.h"

struct TextWithTags;

namespace ChatHelpers {
class Show;
} // namespace ChatHelpers

namespace Ui {
class EmojiButton;
class InputField;
class SendButton;
class RpWidget;
} // namespace Ui

namespace Calls::Group {

class MessageField final {
public:
	MessageField(
		not_null<QWidget*> parent,
		std::shared_ptr<ChatHelpers::Show> show);

	void resizeToWidth(int newWidth);
	void move(int x, int y);
	void toggle(bool shown);
	void raise();

	[[nodiscard]] int height() const;
	[[nodiscard]] rpl::producer<int> heightValue() const;

	[[nodiscard]] rpl::producer<TextWithTags> submitted() const;
	[[nodiscard]] rpl::producer<> closed() const;

	[[nodiscard]] rpl::lifetime &lifetime();

private:
	void createControls();
	void setupBackground();
	void shownAnimationCallback();

	const not_null<QWidget*> _parent;
	const std::shared_ptr<ChatHelpers::Show> _show;
	const std::unique_ptr<Ui::RpWidget> _wrap;

	Ui::InputField *_field = nullptr;
	Ui::SendButton *_send = nullptr;
	Ui::EmojiButton *_emojiToggle = nullptr;

	rpl::variable<int> _width;

	bool _shown = false;
	Ui::Animations::Simple _shownAnimation;

	rpl::event_stream<TextWithTags> _submitted;
	rpl::event_stream<> _closed;

	rpl::lifetime _lifetime;

};

} // namespace Calls::Group
