/*
This file is part of 64Gram Desktop,
the unofficial app based on Telegram Desktop.
For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include <facades.h>
#include <ui/toast/toast.h>
#include "boxes/enhanced_options_box.h"

#include "lang/lang_keys.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"
#include "ui/boxes/confirm_box.h"
#include "core/application.h"
#include "core/enhanced_settings.h"
#include "settings/settings_enhanced.h"

NetBoostBox::NetBoostBox(QWidget *parent) {
}

void NetBoostBox::prepare() {
	setTitle(tr::lng_settings_net_upload_speed_boost());

	addButton(tr::lng_settings_save(), [=] { save(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	auto y = st::boxOptionListPadding.top();
	_description.create(
			this,
			tr::lng_net_speed_boost_desc(tr::now),
			st::boxLabel);
	_description->moveToLeft(st::boxPadding.left(), y);

	y += _description->height() + st::boxMediumSkip;

	_boostGroup = std::make_shared<Ui::RadiobuttonGroup>(GetEnhancedInt("net_speed_boost"));
	

	for (int i = 0; i <= 3; i++) {
		const auto button = Ui::CreateChild<Ui::Radiobutton>(
				this,
				_boostGroup,
				i,
				BoostLabel(i),
				st::autolockButton);
		button->moveToLeft(st::boxPadding.left(), y);
		y += button->heightNoMargins() + st::boxOptionListSkip;
	}
	showChildren();
	setDimensions(st::boxWidth, y);
}

QString NetBoostBox::BoostLabel(int boost) {
	switch (boost) {
		case 0:
			return tr::lng_net_speed_boost_default(tr::now);
		case 1:
			return tr::lng_net_speed_boost_slight(tr::now);
		case 2:
			return tr::lng_net_speed_boost_medium(tr::now);
		case 3:
			return tr::lng_net_speed_boost_big(tr::now);
		default:
			Unexpected("Boost in NetBoostBox::BoostLabel.");
	}
}

void NetBoostBox::save() {
	const auto changeBoost = [=](Fn<void()> &&close) {
		SetNetworkBoost(_boostGroup->current());
		EnhancedSettings::Write();
		Core::Restart();
	};

	const auto box = std::make_shared<QPointer<BoxContent>>();

	*box = getDelegate()->show(
		Ui::MakeConfirmBox({
				.text = tr::lng_net_boost_restart_desc(tr::now),
				.confirmed = changeBoost,
				.confirmText = tr::lng_settings_restart_now(tr::now),
				.cancelText = tr::lng_cancel(tr::now),
		}));
}

AlwaysDeleteBox::AlwaysDeleteBox(QWidget *parent) {
}

void AlwaysDeleteBox::prepare() {
	setTitle(tr::lng_settings_always_delete_for());

	addButton(tr::lng_box_ok(), [=] { closeBox(); });

	auto y = st::boxOptionListPadding.top();
	_optionGroup = std::make_shared<Ui::RadiobuttonGroup>(GetEnhancedInt("always_delete_for"));

	for (int i = 0; i <= 3; i++) {
		const auto button = Ui::CreateChild<Ui::Radiobutton>(
				this,
				_optionGroup,
				i,
				DeleteLabel(i),
				st::autolockButton);
		button->moveToLeft(st::boxPadding.left(), y);
		y += button->heightNoMargins() + st::boxOptionListSkip;
	}
	_optionGroup->setChangedCallback([=](int value) { save(); });
	setDimensions(st::boxWidth, y);
}

QString AlwaysDeleteBox::DeleteLabel(int boost) {
	switch (boost) {
		case 0:
			return tr::lng_settings_delete_disabled(tr::now);
		case 1:
			return tr::lng_settings_delete_for_group(tr::now);
		case 2:
			return tr::lng_settings_delete_for_person(tr::now);
		case 3:
			return tr::lng_settings_delete_for_both(tr::now);
		default:
			Unexpected("Delete in AlwaysDeleteBox::DeleteLabel.");
	}
}

void AlwaysDeleteBox::save() {
	SetEnhancedValue("always_delete_for", _optionGroup->current());
	EnhancedSettings::Write();
	closeBox();
}

RadioController::RadioController(QWidget *parent)
		: _url(this, st::defaultInputField, tr::lng_formatting_link_url()) {
}

void RadioController::prepare() {
	setTitle(tr::lng_settings_radio_controller());

	addButton(tr::lng_settings_save(), [=] { save(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	_url->setText(GetEnhancedString("radio_controller"));

	setDimensions(st::boxWidth, _url->height());
}

void RadioController::setInnerFocus() {
	_url->setFocusFast();
}

void RadioController::resizeEvent(QResizeEvent *e) {
	BoxContent::resizeEvent(e);

	int32 w = st::boxWidth - st::boxPadding.left() - st::boxPadding.right();
	_url->resize(w, _url->height());
	_url->moveToLeft(st::boxPadding.left(), 0);
}

void RadioController::save() {
	auto host = _url->getLastText().trimmed();
	if (host == "") {
		host = "http://localhost:2468";
	}
	SetEnhancedValue("radio_controller", host);
	EnhancedSettings::Write();
	closeBox();
}

BitrateController::BitrateController(QWidget *parent) {
}

void BitrateController::prepare() {
	setTitle(tr::lng_bitrate_controller());

	addButton(tr::lng_settings_save(), [=] { save(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	auto y = st::boxOptionListPadding.top();
	_description.create(
			this,
			tr::lng_bitrate_controller_desc(tr::now),
			st::boxLabel);
	_description->moveToLeft(st::boxPadding.left(), y);

	y += _description->height() + st::boxMediumSkip;

	_bitrateGroup = std::make_shared<Ui::RadiobuttonGroup>(GetEnhancedInt("bitrate"));

	for (int i = 0; i <= 7; i++) {
		const auto button = Ui::CreateChild<Ui::Radiobutton>(
				this,
				_bitrateGroup,
				i,
				BitrateLabel(i),
				st::autolockButton);
		button->moveToLeft(st::boxPadding.left(), y);
		y += button->heightNoMargins() + st::boxOptionListSkip;
	}
	showChildren();
	setDimensions(st::boxWidth, y);
}

QString BitrateController::BitrateLabel(int boost) {
	switch (boost) {
		case 0:
			return tr::lng_bitrate_controller_default(tr::now);
		case 1:
			return tr::lng_bitrate_controller_64k(tr::now);
		case 2:
			return tr::lng_bitrate_controller_96k(tr::now);
		case 3:
			return tr::lng_bitrate_controller_128k(tr::now);
		case 4:
			return tr::lng_bitrate_controller_160k(tr::now);
		case 5:
			return tr::lng_bitrate_controller_192k(tr::now);
		case 6:
			return tr::lng_bitrate_controller_256k(tr::now);
		case 7:
			return tr::lng_bitrate_controller_320k(tr::now);
		default:
			Unexpected("Bitrate not found.");
	}
}

void BitrateController::save() {
	SetEnhancedValue("bitrate", _bitrateGroup->current());
	EnhancedSettings::Write();
	Ui::Toast::Show(tr::lng_bitrate_controller_hint(tr::now));
	closeBox();
}

RecentDisplayLimitController::RecentDisplayLimitController(QWidget *parent) {
}

void RecentDisplayLimitController::prepare() {
	setTitle(tr::lng_settings_recent_display_limit());

	addButton(tr::lng_settings_save(), [=] { save(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	auto y = st::boxOptionListPadding.top();

	_optionGroup = std::make_shared<Ui::RadiobuttonGroup>(GetEnhancedInt("recent_display_limit"));

	for (int i = 0; i <= 5; i++) {
		const auto button = Ui::CreateChild<Ui::Radiobutton>(
				this,
				_optionGroup,
				i,
				Label(i),
				st::autolockButton);
		button->moveToLeft(st::boxPadding.left(), y);
		y += button->heightNoMargins() + st::boxOptionListSkip;
	}
	showChildren();
	setDimensions(st::boxWidth, y);
}

QString RecentDisplayLimitController::Label(int limit) {
	switch (limit) {
		case 1:
			return QString("40");
		case 2:
			return QString("60");
		case 3:
			return QString("80");
		case 4:
			return QString("100");
		case 5:
			return QString("120");
		default:
			return tr::lng_settings_recent_display_limit_default(tr::now);
	}
}

void RecentDisplayLimitController::save() {
	SetEnhancedValue("recent_display_limit", _optionGroup->current());
	EnhancedSettings::Write();
	closeBox();
}
