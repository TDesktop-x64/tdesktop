/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/TDesktop-x64/tdesktop/blob/dev/LEGAL
*/
#pragma once

#include "boxes/abstract_box.h"
#include "data/filters/message_filter.h"

namespace Ui {
class VerticalLayout;
class InputField;
class Checkbox;
class RadiobuttonGroup;
class Radiobutton;
class FlatLabel;
class SettingsButton;
class LinkButton;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

class MessageFilterListBox : public Ui::BoxContent {
public:
	MessageFilterListBox(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

protected:
	void prepare() override;
	void showEvent(QShowEvent *e) override;

private:
	void addFilter();
	void editFilter(const QString &filterId);
	void deleteFilter(const QString &filterId);
	void moveFilter(const QString &filterId, int direction);
	void refreshList();

	const not_null<Window::SessionController*> _controller;
	object_ptr<Ui::VerticalLayout> _list = {nullptr};
	bool _prepared = false;
};

class MessageFilterEditBox : public Ui::BoxContent {
public:
	MessageFilterEditBox(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		const MessageFilters::MessageFilter &filter,
		bool isNew);

protected:
	void prepare() override;
	void setInnerFocus() override;

private:
	void save();
	void selectUsers();
	void selectChats();
	void updateUserLabel();
	void updateChatLabel();
	void updateGlobalState();
	void updateModeState();

	const not_null<Window::SessionController*> _controller;
	MessageFilters::MessageFilter _filter;
	bool _isNew = false;

	object_ptr<Ui::InputField> _name = {nullptr};
	object_ptr<Ui::InputField> _regex = {nullptr};
	object_ptr<Ui::InputField> _replacementText = {nullptr};
	object_ptr<Ui::Checkbox> _global = {nullptr};
	Ui::LinkButton *_chatSelectBtn = nullptr;
	Ui::FlatLabel *_chatLabel = nullptr;
	Ui::LinkButton *_userSelectBtn = nullptr;
	Ui::FlatLabel *_userLabel = nullptr;
	object_ptr<Ui::Checkbox> _enabled = {nullptr};
	std::shared_ptr<Ui::RadiobuttonGroup> _modeGroup;
	std::shared_ptr<Ui::RadiobuttonGroup> _displayGroup;
};

