/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "menu/menu_mute.h"

#include "boxes/ringtones_box.h"
#include "data/data_session.h"
#include "data/data_thread.h"
#include "data/data_peer.h"
#include "data/notify/data_notify_settings.h"
#include "data/notify/data_peer_notify_settings.h"
#include "info/profile/info_profile_values.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "ui/boxes/choose_time.h"
#include "ui/boxes/confirm_box.h"
#include "ui/boxes/time_picker_box.h"
#include "ui/effects/animation_value.h"
#include "ui/layers/generic_box.h"
#include "ui/text/format_values.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/menu/menu_action.h"
#include "ui/widgets/popup_menu.h"
#include "ui/painter.h"
#include "styles/style_boxes.h"
#include "styles/style_info.h" // infoTopBarMenu
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "core/enhanced_settings.h"
#include "base/unixtime.h"

namespace MuteMenu {
namespace {

constexpr auto kMuteDurSecondsDefault = crl::time(8) * 3600;
constexpr auto kMuteForeverValue = std::numeric_limits<TimeId>::max();
constexpr auto kSoftMuteDurSecondsDefault = crl::time(1) * 3600; // 1 hour

// Soft mute time picker values: 30 seconds to 1 day
[[nodiscard]] std::vector<TimeId> SoftMuteTimePickerValues() {
	return {
		30,          // 30 seconds
		60,          // 1 minute
		120,         // 2 minutes
		300,         // 5 minutes
		600,         // 10 minutes
		900,         // 15 minutes
		1800,        // 30 minutes
		3600,        // 1 hour
		7200,        // 2 hours
		14400,       // 4 hours
		28800,       // 8 hours
		43200,       // 12 hours
		86400,       // 1 day
	};
}

// Forward declarations
void SoftMuteBox(
	not_null<Ui::GenericBox*> box,
	not_null<Data::Thread*> thread);

class IconWithText final : public Ui::Menu::Action {
public:
	using Ui::Menu::Action::Action;

	void setData(const QString &text, const QPoint &iconPosition);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	QPoint _iconPosition;
	QString _text;

};

void IconWithText::setData(const QString &text, const QPoint &iconPosition) {
	_iconPosition = iconPosition;
	_text = text;
}

void IconWithText::paintEvent(QPaintEvent *e) {
	Ui::Menu::Action::paintEvent(e);

	auto p = QPainter(this);
	p.setFont(st::menuIconMuteForAnyTextFont);
	p.setPen(st::menuIconColor);
	p.drawText(_iconPosition, _text);
}

class MuteItem final : public Ui::Menu::Action {
public:
	MuteItem(
		not_null<RpWidget*> parent,
		const style::Menu &st,
		Descriptor descriptor);

protected:
	void paintEvent(QPaintEvent *e) override;

private:
	const QPoint _itemIconPosition;
	Ui::Animations::Simple _animation;
	bool _isMuted = false;
	bool _inited;

};

MuteItem::MuteItem(
	not_null<RpWidget*> parent,
	const style::Menu &st,
	Descriptor descriptor)
: Ui::Menu::Action(
	parent,
	st,
	Ui::CreateChild<QAction>(parent.get()),
	nullptr,
	nullptr)
, _itemIconPosition(st.itemIconPosition) {
	descriptor.isMutedValue(
	) | rpl::start_with_next([=](bool isMuted) {
		action()->setText(isMuted
			? tr::lng_mute_menu_duration_unmute(tr::now)
			: tr::lng_mute_menu_duration_forever(tr::now));
		if (_inited && isMuted == _isMuted) {
			return;
		}
		_inited = true;
		_isMuted = isMuted;
		_animation.start(
			[=] { update(); },
			isMuted ? 0. : 1.,
			isMuted ? 1. : 0.,
			st::defaultPopupMenu.showDuration);
	}, lifetime());
	_animation.stop();

	setClickedCallback([=] {
		descriptor.updateMutePeriod(_isMuted ? 0 : kMuteForeverValue);
	});
}

void MuteItem::paintEvent(QPaintEvent *e) {
	Painter p(this);

	const auto progress = _animation.value(_isMuted ? 1. : 0.);
	const auto color = anim::color(
		st::menuIconAttentionColor,
		st::boxTextFgGood,
		progress);
	p.setPen(color);

	Action::paintBackground(p, Action::isSelected());
	RippleButton::paintRipple(p, 0, 0);
	Action::paintText(p);

	const auto &icon = _isMuted ? st::menuIconUnmute : st::menuIconMute;
	icon.paint(p, _itemIconPosition, width(), color);
}

void MuteBox(not_null<Ui::GenericBox*> box, Descriptor descriptor) {
	struct State {
		int lastSeconds = 0;
	};

	auto chooseTimeResult = ChooseTimeWidget(box, kMuteDurSecondsDefault);
	box->addRow(std::move(chooseTimeResult.widget));

	const auto state = box->lifetime().make_state<State>();

	box->setTitle(tr::lng_mute_box_title());

	auto confirmText = std::move(
		chooseTimeResult.secondsValue
	) | rpl::map([=](int seconds) {
		state->lastSeconds = seconds;
		return !seconds
			? tr::lng_mute_menu_unmute()
			: tr::lng_mute_menu_mute();
	}) | rpl::flatten_latest();

	Ui::ConfirmBox(box, {
		.confirmed = [=] {
			descriptor.updateMutePeriod(state->lastSeconds);
			box->getDelegate()->hideLayer();
		},
		.confirmText = std::move(confirmText),
		.cancelText = tr::lng_cancel(),
	});
}

void PickMuteBox(
		not_null<Ui::GenericBox*> box,
		Descriptor descriptor) {
	struct State {
		base::unique_qptr<Ui::PopupMenu> menu;
	};
	const auto seconds = Ui::DefaultTimePickerValues();
	const auto phrases = ranges::views::all(
		seconds
	) | ranges::views::transform(Ui::FormatMuteFor) | ranges::to_vector;

	const auto state = box->lifetime().make_state<State>();

	const auto pickerCallback = TimePickerBox(box, seconds, phrases, 0);

	Ui::ConfirmBox(box, {
		.confirmed = [=] {
			const auto muteFor = pickerCallback();
			descriptor.updateMutePeriod(muteFor);
			descriptor.session->settings().addMutePeriod(muteFor);
			descriptor.session->saveSettings();
			box->closeBox();
		},
		.confirmText = tr::lng_mute_menu_mute(),
		.cancelText = tr::lng_cancel(),
	});

	box->setTitle(tr::lng_mute_box_title());

	const auto top = box->addTopButton(st::infoTopBarMenu);
	top->setClickedCallback([=] {
		if (state->menu) {
			return;
		}
		state->menu = base::make_unique_q<Ui::PopupMenu>(
			top,
			st::popupMenuWithIcons);
		state->menu->addAction(
			tr::lng_manage_messages_ttl_after_custom(tr::now),
			[=] { box->getDelegate()->show(Box(MuteBox, descriptor)); },
			&st::menuIconCustomize);
		state->menu->setDestroyedCallback(crl::guard(top, [=] {
			top->setForceRippled(false);
		}));
		top->setForceRippled(true);
		state->menu->popup(QCursor::pos());
	});
}

void PickSoftMuteBox(
		not_null<Ui::GenericBox*> box,
		not_null<Data::Thread*> thread) {
	struct State {
		base::unique_qptr<Ui::PopupMenu> menu;
	};
	const auto seconds = SoftMuteTimePickerValues();
	const auto phrases = ranges::views::all(
		seconds
	) | ranges::views::transform(Ui::FormatMuteFor) | ranges::to_vector;

	const auto state = box->lifetime().make_state<State>();

	const auto pickerCallback = TimePickerBox(box, seconds, phrases, 0);

	Ui::ConfirmBox(box, {
		.confirmed = [=] {
			const auto softMuteFor = pickerCallback();
			auto muteState = EnhancedSettings::SoftMuteState{};
			muteState.enabled = true;
			muteState.period = softMuteFor;
			muteState.lastNotificationTime = 0; // Will trigger on first message
			muteState.suppressionMode = GetEnhancedInt("soft_mute_default_mode");
			EnhancedSettings::SetSoftMuteState(thread->peer()->id.value, muteState);
			box->closeBox();
		},
		.confirmText = tr::lng_mute_menu_mute(),
		.cancelText = tr::lng_cancel(),
	});

	box->setTitle(tr::lng_soft_mute_box_title());

	const auto top = box->addTopButton(st::infoTopBarMenu);
	top->setClickedCallback([=] {
		if (state->menu) {
			return;
		}
		state->menu = base::make_unique_q<Ui::PopupMenu>(
			top,
			st::popupMenuWithIcons);
		state->menu->addAction(
			tr::lng_manage_messages_ttl_after_custom(tr::now),
			[=] { box->getDelegate()->show(Box(SoftMuteBox, thread)); },
			&st::menuIconCustomize);
		state->menu->setDestroyedCallback(crl::guard(top, [=] {
			top->setForceRippled(false);
		}));
		top->setForceRippled(true);
		state->menu->popup(QCursor::pos());
	});
}

void SoftMuteBox(
		not_null<Ui::GenericBox*> box,
		not_null<Data::Thread*> thread) {
	struct State {
		int lastSeconds = 0;
	};

	auto chooseTimeResult = ChooseTimeWidget(box, kSoftMuteDurSecondsDefault);
	box->addRow(std::move(chooseTimeResult.widget));

	const auto state = box->lifetime().make_state<State>();

	box->setTitle(tr::lng_soft_mute_box_title());

	auto confirmText = std::move(
		chooseTimeResult.secondsValue
	) | rpl::map([=](int seconds) {
		state->lastSeconds = seconds;
		return !seconds
			? tr::lng_cancel()
			: tr::lng_mute_menu_mute();
	}) | rpl::flatten_latest();

	Ui::ConfirmBox(box, {
		.confirmed = [=] {
			if (state->lastSeconds > 0) {
				auto muteState = EnhancedSettings::SoftMuteState{};
				muteState.enabled = true;
				muteState.period = state->lastSeconds;
				muteState.lastNotificationTime = 0; // Will trigger on first message
				muteState.suppressionMode = GetEnhancedInt("soft_mute_default_mode");
				EnhancedSettings::SetSoftMuteState(thread->peer()->id.value, muteState);
			}
			box->getDelegate()->hideLayer();
		},
		.confirmText = std::move(confirmText),
		.cancelText = tr::lng_cancel(),
	});
}

} // namespace

Descriptor ThreadDescriptor(not_null<Data::Thread*> thread) {
	const auto weak = base::make_weak(thread);
	const auto isMutedValue = [=]() -> rpl::producer<bool> {
		if (const auto strong = weak.get()) {
			return Info::Profile::NotificationsEnabledValue(
				strong
			) | rpl::map(!rpl::mappers::_1);
		}
		return rpl::single(false);
	};
	const auto currentSound = [=] {
		const auto strong = weak.get();
		return strong
			? strong->owner().notifySettings().sound(strong)
			: std::optional<Data::NotifySound>();
	};
	const auto updateSound = crl::guard(weak, [=](Data::NotifySound sound) {
		thread->owner().notifySettings().update(thread, {}, {}, sound);
	});
	const auto updateMutePeriod = crl::guard(weak, [=](TimeId mute) {
		const auto settings = &thread->owner().notifySettings();
		if (!mute) {
			settings->update(thread, { .unmute = true });
		} else if (mute == kMuteForeverValue) {
			settings->update(thread, { .forever = true });
		} else {
			settings->update(thread, { .period = mute });
		}
	});
	return {
		.session = &thread->session(),
		.isMutedValue = isMutedValue,
		.currentSound = currentSound,
		.updateSound = updateSound,
		.updateMutePeriod = updateMutePeriod,
		.volumeController = Data::ThreadRingtonesVolumeController(thread),
		.thread = thread.get(),
	};
}

Descriptor DefaultDescriptor(
		not_null<Main::Session*> session,
		Data::DefaultNotify type) {
	const auto settings = &session->data().notifySettings();
	const auto isMutedValue = [=]() -> rpl::producer<bool> {
		return rpl::single(
			rpl::empty
		) | rpl::then(
			settings->defaultUpdates(type)
		) | rpl::map([=] {
			return settings->isMuted(type);
		});
	};
	const auto currentSound = [=] {
		return settings->defaultSettings(type).sound();
	};
	const auto updateSound = [=](Data::NotifySound sound) {
		settings->defaultUpdate(type, {}, {}, sound);
	};
	const auto updateMutePeriod = [=](TimeId mute) {
		if (!mute) {
			settings->defaultUpdate(type, { .unmute = true });
		} else if (mute == kMuteForeverValue) {
			settings->defaultUpdate(type, { .forever = true });
		} else {
			settings->defaultUpdate(type, { .period = mute });
		}
	};
	return {
		.session = session,
		.isMutedValue = isMutedValue,
		.currentSound = currentSound,
		.updateSound = updateSound,
		.updateMutePeriod = updateMutePeriod,
		.volumeController = DefaultRingtonesVolumeController(session, type),
	};
}

void FillMuteMenu(
		not_null<Ui::PopupMenu*> menu,
		Descriptor descriptor,
		std::shared_ptr<Ui::Show> show) {
	const auto session = descriptor.session;
	const auto soundSelect = [=] {
		if (const auto currentSound = descriptor.currentSound()) {
			show->showBox(Box(
				RingtonesBox,
				session,
				*currentSound,
				descriptor.updateSound,
				descriptor.volumeController));
		}
	};
	menu->addAction(
		tr::lng_mute_menu_sound_select(tr::now),
		soundSelect,
		&st::menuIconSoundSelect);

	const auto soundIsNone = descriptor.currentSound().value_or(
		Data::NotifySound()
	).none;
	const auto toggleSound = [=] {
		if (auto sound = descriptor.currentSound()) {
			sound->none = !soundIsNone;
			descriptor.updateSound(*sound);
		}
	};
	menu->addAction(
		(soundIsNone
			? tr::lng_mute_menu_sound_on(tr::now)
			: tr::lng_mute_menu_sound_off(tr::now)),
		toggleSound,
		soundIsNone ? &st::menuIconSoundOn : &st::menuIconSoundOff);

	const auto &st = menu->st().menu;
	const auto iconTextPosition = st.itemIconPosition
		+ st::menuIconMuteForAnyTextPosition;
	for (const auto muteFor : session->settings().mutePeriods()) {
		const auto callback = [=, update = descriptor.updateMutePeriod] {
			update(muteFor);
		};

		auto item = base::make_unique_q<IconWithText>(
			menu,
			st,
			Ui::Menu::CreateAction(
				menu->menu().get(),
				tr::lng_mute_menu_duration_any(
					tr::now,
					lt_duration,
					Ui::FormatMuteFor(muteFor)),
				callback),
			&st::menuIconMuteForAny,
			&st::menuIconMuteForAny);
		item->setData(Ui::FormatMuteForTiny(muteFor), iconTextPosition);
		menu->addAction(std::move(item));
	}

	menu->addAction(
		tr::lng_mute_menu_duration(tr::now),
		[=] { show->showBox(Box(PickMuteBox, descriptor)); },
		&st::menuIconMuteFor);

	menu->addAction(
		base::make_unique_q<MuteItem>(menu, menu->st().menu, descriptor));

	// Add soft mute section (only for threads, not for default descriptors)
	if (descriptor.thread) {
		menu->addSeparator();

		const auto thread = descriptor.thread;
		const auto peerId = thread->peer()->id.value;
		const auto softMuteState = EnhancedSettings::GetSoftMuteState(peerId);

		if (softMuteState.enabled) {
			// Show disable soft mute option
			menu->addAction(
				tr::lng_soft_mute_menu_disable(tr::now),
				[=] {
					EnhancedSettings::RemoveSoftMute(peerId);
				},
				&st::menuIconUnmute);
		} else {
			// Show soft mute duration options - use custom soft mute time range
			const auto softMutePeriods = SoftMuteTimePickerValues();
			// Show only a few preset options (skip the very short ones for menu)
			const std::vector<TimeId> quickOptions = {
				60,    // 1 minute
				600,   // 10 minutes
			};
			
			for (const auto softMuteFor : quickOptions) {
				const auto callback = [=] {
					auto state = EnhancedSettings::SoftMuteState{};
					state.enabled = true;
					state.period = softMuteFor;
					state.lastNotificationTime = 0;
					state.suppressionMode = GetEnhancedInt("soft_mute_default_mode");
					EnhancedSettings::SetSoftMuteState(peerId, state);
				};

				auto item = base::make_unique_q<IconWithText>(
					menu,
					st,
					Ui::Menu::CreateAction(
						menu->menu().get(),
						tr::lng_soft_mute_menu_duration_any(
							tr::now,
							lt_duration,
							Ui::FormatMuteFor(softMuteFor)),
						callback),
					&st::menuIconMuteForAny,
					&st::menuIconMuteForAny);
				item->setData(Ui::FormatMuteForTiny(softMuteFor), iconTextPosition);
				menu->addAction(std::move(item));
			}

			menu->addAction(
				tr::lng_soft_mute_menu_duration(tr::now),
				[=, show = show] { show->showBox(Box(PickSoftMuteBox, thread)); },
				&st::menuIconMuteFor);
		}
	}
}

void SetupMuteMenu(
		not_null<Ui::RpWidget*> parent,
		rpl::producer<> triggers,
		Fn<std::optional<Descriptor>()> makeDescriptor,
		std::shared_ptr<Ui::Show> show) {
	struct State {
		base::unique_qptr<Ui::PopupMenu> menu;
	};
	const auto state = parent->lifetime().make_state<State>();
	std::move(
		triggers
	) | rpl::start_with_next([=] {
		if (state->menu) {
			return;
		} else if (const auto descriptor = makeDescriptor()) {
			state->menu = base::make_unique_q<Ui::PopupMenu>(
				parent,
				st::popupMenuWithIcons);
			FillMuteMenu(state->menu.get(), *descriptor, show);
			state->menu->popup(QCursor::pos());
		}
	}, parent->lifetime());
}

} // namespace MuteMenu
