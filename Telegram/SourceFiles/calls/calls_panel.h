/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/weak_ptr.h"
#include "base/timer.h"
#include "base/object_ptr.h"
#include "base/unique_qptr.h"
#include "calls/calls_call.h"
#include "calls/group/ui/desktop_capture_choose_source.h"
#include "ui/effects/animations.h"
#include "ui/gl/gl_window.h"
#include "ui/rp_widget.h"

class Image;

namespace base {
class PowerSaveBlocker;
} // namespace base

namespace Data {
class PhotoMedia;
} // namespace Data

namespace Ui {
class IconButton;
class CallButton;
class LayerManager;
class FlatLabel;
template <typename Widget>
class FadeWrap;
template <typename Widget>
class PaddingWrap;
class RpWindow;
class PopupMenu;
namespace GL {
enum class Backend;
} // namespace GL
namespace Platform {
struct SeparateTitleControls;
} // namespace Platform
} // namespace Ui

namespace style {
struct CallSignalBars;
struct CallBodyLayout;
} // namespace style

namespace Calls {

class Userpic;
class SignalBars;
class VideoBubble;
struct DeviceSelection;

class Panel final : private Group::Ui::DesktopCapture::ChooseSourceDelegate {
public:
	Panel(not_null<Call*> call);
	~Panel();

	[[nodiscard]] bool isVisible() const;
	[[nodiscard]] bool isActive() const;
	void showAndActivate();
	void minimize();
	void pinToTop(bool isPinned);
	void toggleFullScreen();
	void replaceCall(not_null<Call*> call);
	void closeBeforeDestroy();

	QWidget *chooseSourceParent() override;
	QString chooseSourceActiveDeviceId() override;
	bool chooseSourceActiveWithAudio() override;
	bool chooseSourceWithAudioSupported() override;
	rpl::lifetime &chooseSourceInstanceLifetime() override;
	void chooseSourceAccepted(
		const QString &deviceId,
		bool withAudio) override;
	void chooseSourceStop() override;

	[[nodiscard]] rpl::producer<bool> startOutgoingRequests() const;

	[[nodiscard]] rpl::lifetime &lifetime();

private:
	class Incoming;
	using State = Call::State;
	using Type = Call::Type;
	enum class AnswerHangupRedialState : uchar {
		Answer,
		Hangup,
		Redial,
		StartCall,
	};

	[[nodiscard]] not_null<Ui::RpWindow*> window() const;
	[[nodiscard]] not_null<Ui::RpWidget*> widget() const;

	void paint(QRect clip);

	void initWindow();
	void initWidget();
	void initControls();
	void reinitWithCall(Call *call);
	void initLayout();
	void initMediaDeviceToggles();
	void initGeometry();

	[[nodiscard]] bool handleClose() const;

	void requestControlsHidden(bool hidden);
	void controlsShownForce(bool shown);
	void updateControlsShown();
	void updateControlsGeometry();
	void updateHangupGeometry();
	void updateStatusGeometry();
	void updateOutgoingVideoBubbleGeometry();
	void stateChanged(State state);
	void showControls();
	void updateStatusText(State state);
	void startDurationUpdateTimer(crl::time currentDuration);
	void setIncomingSize(QSize size);
	void refreshIncomingGeometry();

	void refreshOutgoingPreviewInBody(State state);
	void toggleFullScreen(bool fullscreen);
	void createRemoteAudioMute();
	void createRemoteLowBattery();
	void showRemoteLowBattery();
	void refreshAnswerHangupRedialLabel();

	void showDevicesMenu(
		not_null<QWidget*> button,
		std::vector<DeviceSelection> types);

	[[nodiscard]] QRect incomingFrameGeometry() const;
	[[nodiscard]] QRect outgoingFrameGeometry() const;

	Call *_call = nullptr;
	not_null<UserData*> _user;

	Ui::GL::Window _window;
	const std::unique_ptr<Ui::LayerManager> _layerBg;
	std::unique_ptr<Incoming> _incoming;

#ifndef Q_OS_MAC
	std::unique_ptr<Ui::Platform::SeparateTitleControls> _controls;
#endif // !Q_OS_MAC

	std::unique_ptr<base::PowerSaveBlocker> _powerSaveBlocker;

	QSize _incomingFrameSize;

	rpl::lifetime _callLifetime;

	not_null<const style::CallBodyLayout*> _bodySt;
	object_ptr<Ui::CallButton> _answerHangupRedial;
	object_ptr<Ui::FadeWrap<Ui::CallButton>> _decline;
	object_ptr<Ui::FadeWrap<Ui::CallButton>> _cancel;
	bool _hangupShown = false;
	bool _outgoingPreviewInBody = false;
	std::optional<AnswerHangupRedialState> _answerHangupRedialState;
	Ui::Animations::Simple _hangupShownProgress;
	object_ptr<Ui::FadeWrap<Ui::CallButton>> _screencast;
	object_ptr<Ui::CallButton> _camera;
	Ui::CallButton *_cameraDeviceToggle = nullptr;
	base::unique_qptr<Ui::CallButton> _startVideo;
	object_ptr<Ui::FadeWrap<Ui::CallButton>> _mute;
	Ui::CallButton *_audioDeviceToggle = nullptr;
	object_ptr<Ui::FlatLabel> _name;
	object_ptr<Ui::FlatLabel> _status;
	object_ptr<Ui::RpWidget> _fingerprint = { nullptr };
	object_ptr<Ui::PaddingWrap<Ui::FlatLabel>> _remoteAudioMute = { nullptr };
	object_ptr<Ui::PaddingWrap<Ui::FlatLabel>> _remoteLowBattery
		= { nullptr };
	std::unique_ptr<Userpic> _userpic;
	std::unique_ptr<VideoBubble> _outgoingVideoBubble;
	QPixmap _bottomShadow;
	int _bodyTop = 0;
	int _buttonsTopShown = 0;
	int _buttonsTop = 0;

	base::Timer _hideControlsTimer;
	base::Timer _controlsShownForceTimer;
	std::unique_ptr<QObject> _hideControlsFilter;
	bool _hideControlsRequested = false;
	rpl::variable<bool> _fullScreenOrMaximized;
	Ui::Animations::Simple _controlsShownAnimation;
	bool _controlsShownForce = false;
	bool _controlsShown = true;
	bool _mouseInside = false;

	base::unique_qptr<Ui::PopupMenu> _devicesMenu;

	base::Timer _updateDurationTimer;
	base::Timer _updateOuterRippleTimer;

	rpl::event_stream<bool> _startOutgoingRequests;

};

} // namespace Calls
