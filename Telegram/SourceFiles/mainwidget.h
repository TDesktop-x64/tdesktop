/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/weak_ptr.h"
#include "base/unique_qptr.h"
#include "chat_helpers/bot_command.h"
#include "ui/rp_widget.h"
#include "ui/effects/animations.h"
#include "media/player/media_player_float.h"
#include "mtproto/sender.h"
#include "data/data_pts_waiter.h"

struct HistoryMessageMarkupButton;
class MainWindow;
class HistoryWidget;
class StackItem;
struct FileLoadResult;
class History;
class Image;

namespace MTP {
class Error;
} // namespace MTP

namespace Api {
struct SendAction;
struct SendOptions;
} // namespace Api

namespace SendMenu {
enum class Type;
} // namespace SendMenu

namespace Main {
class Session;
} // namespace Main

namespace Data {
class WallPaper;
struct ForwardDraft;
} // namespace Data

namespace Dialogs {
struct RowDescriptor;
class Row;
class Key;
class Widget;
} // namespace Dialogs

namespace Media {
namespace Player {
class Widget;
class Panel;
struct TrackState;
} // namespace Player
} // namespace Media

namespace Export {
namespace View {
class TopBar;
class PanelController;
struct Content;
} // namespace View
} // namespace Export

namespace Ui {
class ConfirmBox;
class ResizeArea;
class PlainShadow;
class DropdownMenu;
enum class ReportReason;
template <typename Widget>
class SlideWrap;
} // namespace Ui

namespace Window {
class SessionController;
template <typename Inner>
class TopBarWrapWidget;
class SectionMemento;
class SectionWidget;
class AbstractSectionWidget;
class ConnectionState;
struct SectionSlideParams;
struct SectionShow;
enum class Column;
class HistoryHider;
} // namespace Window

namespace Calls {
class Call;
class GroupCall;
class TopBar;
} // namespace Calls

namespace Core {
class Changelogs;
} // namespace Core

namespace InlineBots {
namespace Layout {
class ItemBase;
} // namespace Layout
} // namespace InlineBots

class MainWidget
	: public Ui::RpWidget
	, private Media::Player::FloatDelegate
	, private base::Subscriber {
public:
	using SectionShow = Window::SectionShow;

	MainWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller);
	~MainWidget();

	[[nodiscard]] Main::Session &session() const;
	[[nodiscard]] not_null<Window::SessionController*> controller() const;
	[[nodiscard]] PeerData *singlePeer() const;
	[[nodiscard]] bool isPrimary() const;
	[[nodiscard]] bool isMainSectionShown() const;
	[[nodiscard]] bool isThirdSectionShown() const;

	[[nodiscard]] Dialogs::RowDescriptor resolveChatNext(
		Dialogs::RowDescriptor from) const;
	[[nodiscard]] Dialogs::RowDescriptor resolveChatPrevious(
		Dialogs::RowDescriptor from) const;

	void returnTabbedSelector();

	void showAnimated(const QPixmap &bgAnimCache, bool back = false);

	void activate();

	void windowShown();

	void dialogsToUp();
	void checkHistoryActivation();

	PeerData *peer();

	int backgroundFromY() const;
	void showSection(
		std::shared_ptr<Window::SectionMemento> memento,
		const SectionShow &params);
	void updateColumnLayout();
	bool stackIsEmpty() const;
	void showBackFromStack(
		const SectionShow &params);
	void orderWidgets();
	QPixmap grabForShowAnimation(const Window::SectionSlideParams &params);
	void checkMainSectionToLayer();

	[[nodiscard]] SendMenu::Type sendMenuType() const;
	bool sendExistingDocument(not_null<DocumentData*> document);
	bool sendExistingDocument(
		not_null<DocumentData*> document,
		Api::SendOptions options);

	bool isActive() const;
	[[nodiscard]] bool doWeMarkAsRead() const;

	void saveFieldToHistoryLocalDraft();

	void showForwardLayer(Data::ForwardDraft &&draft);
	void showSendPathsLayer();
	void shareUrlLayer(const QString &url, const QString &text);
	void inlineSwitchLayer(const QString &botAndQuery);
	void hiderLayer(base::unique_qptr<Window::HistoryHider> h);
	bool setForwardDraft(PeerId peer, Data::ForwardDraft &&draft);
	bool shareUrl(
		PeerId peerId,
		const QString &url,
		const QString &text);
	bool inlineSwitchChosen(PeerId peerId, const QString &botAndQuery);
	bool sendPaths(PeerId peerId);
	void onFilesOrForwardDrop(const PeerId &peer, const QMimeData *data);
	bool selectingPeer() const;

	void sendBotCommand(Bot::SendCommandRequest request);
	void hideSingleUseKeyboard(PeerData *peer, MsgId replyTo);
	bool insertBotCommand(const QString &cmd);

	void searchMessages(const QString& query, Dialogs::Key inChat, PeerData* from);

	void setChatBackground(
		const Data::WallPaper &background,
		QImage &&image = QImage());
	bool chatBackgroundLoading();
	float64 chatBackgroundProgress() const;
	void checkChatBackground();
	Image *newBackgroundThumb();

	// Does offerPeer or showPeerHistory.
	void choosePeer(PeerId peerId, MsgId showAtMsgId);
	void clearBotStartToken(PeerData *peer);

	void ctrlEnterSubmitUpdated();
	void setInnerFocus();

	bool contentOverlapped(const QRect &globalRect);

	void searchInChat(Dialogs::Key chat);

	void showChooseReportMessages(
		not_null<PeerData*> peer,
		Ui::ReportReason reason,
		Fn<void(MessageIdsList)> done);
	void clearChooseReportMessages();

	void toggleChooseChatTheme(not_null<PeerData*> peer);

	void ui_showPeerHistory(
		PeerId peer,
		const SectionShow &params,
		MsgId msgId);

	bool notify_switchInlineBotButtonReceived(const QString &query, UserData *samePeerBot, MsgId samePeerReplyTo);

	using FloatDelegate::floatPlayerAreaUpdated;

	void closeBothPlayers();
	void stopAndClosePlayer();

	bool preventsCloseSection(Fn<void()> callback) const;
	bool preventsCloseSection(
		Fn<void()> callback,
		const SectionShow &params) const;

	void dialogsCancelled();
	void dialogsFocus();

protected:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	bool eventFilter(QObject *o, QEvent *e) override;

private:
	void animationCallback();
	void handleAdaptiveLayoutUpdate();
	void updateWindowAdaptiveLayout();
	void handleAudioUpdate(const Media::Player::TrackState &state);
	void updateMediaPlaylistPosition(int x);
	void updateControlsGeometry();
	void updateDialogsWidthAnimated();
	void updateThirdColumnToCurrentChat(
		Dialogs::Key key,
		bool canWrite);
	[[nodiscard]] bool saveThirdSectionToStackBack() const;
	[[nodiscard]] auto thirdSectionForCurrentMainSection(Dialogs::Key key)
		-> std::shared_ptr<Window::SectionMemento>;

	void setupConnectingWidget();
	void createPlayer();
	void playerHeightUpdated();

	void setCurrentCall(Calls::Call *call);
	void setCurrentGroupCall(Calls::GroupCall *call);
	void createCallTopBar();
	void destroyCallTopBar();
	void callTopBarHeightUpdated(int callTopBarHeight);

	void setCurrentExportView(Export::View::PanelController *view);
	void createExportTopBar(Export::View::Content &&data);
	void destroyExportTopBar();
	void exportTopBarHeightUpdated();

	Window::SectionSlideParams prepareShowAnimation(
		bool willHaveTopBarShadow);
	void showNewSection(
		std::shared_ptr<Window::SectionMemento> memento,
		const SectionShow &params);
	void dropMainSection(Window::SectionWidget *widget);

	Window::SectionSlideParams prepareThirdSectionAnimation(Window::SectionWidget *section);

	// All this methods use the prepareShowAnimation().
	Window::SectionSlideParams prepareMainSectionAnimation(Window::SectionWidget *section);
	Window::SectionSlideParams prepareHistoryAnimation(PeerId historyPeerId);
	Window::SectionSlideParams prepareDialogsAnimation();

	void saveSectionInStack();

	int getMainSectionTop() const;
	int getThirdSectionTop() const;

	void hideAll();
	void showAll();
	void clearHider(not_null<Window::HistoryHider*> instance);

	[[nodiscard]] auto floatPlayerDelegate()
		-> not_null<Media::Player::FloatDelegate*>;
	not_null<Ui::RpWidget*> floatPlayerWidget() override;
	not_null<Media::Player::FloatSectionDelegate*> floatPlayerGetSection(
		Window::Column column) override;
	void floatPlayerEnumerateSections(Fn<void(
		not_null<Media::Player::FloatSectionDelegate*> widget,
		Window::Column widgetColumn)> callback) override;
	bool floatPlayerIsVisible(not_null<HistoryItem*> item) override;
	void floatPlayerClosed(FullMsgId itemId);
	void floatPlayerDoubleClickEvent(
		not_null<const HistoryItem*> item) override;

	void refreshResizeAreas();
	template <typename MoveCallback, typename FinishCallback>
	void createResizeArea(
		object_ptr<Ui::ResizeArea> &area,
		MoveCallback &&moveCallback,
		FinishCallback &&finishCallback);
	void ensureFirstColumnResizeAreaCreated();
	void ensureThirdColumnResizeAreaCreated();

	bool isReadyChatBackground(
		const Data::WallPaper &background,
		const QImage &image) const;
	void setReadyChatBackground(
		const Data::WallPaper &background,
		QImage &&image);

	void handleHistoryBack();
	bool showHistoryInDifferentWindow(
		PeerId peerId,
		const SectionShow &params,
		MsgId showAtMsgId);

	bool isOneColumn() const;
	bool isNormalColumn() const;
	bool isThreeColumn() const;

	const not_null<Window::SessionController*> _controller;

	Ui::Animations::Simple _a_show;
	bool _showBack = false;
	QPixmap _cacheUnder, _cacheOver;

	int _dialogsWidth = 0;
	int _thirdColumnWidth = 0;
	Ui::Animations::Simple _a_dialogsWidth;

	const base::unique_qptr<Ui::PlainShadow> _sideShadow;
	object_ptr<Ui::PlainShadow> _thirdShadow = { nullptr };
	object_ptr<Ui::ResizeArea> _firstColumnResizeArea = { nullptr };
	object_ptr<Ui::ResizeArea> _thirdColumnResizeArea = { nullptr };
	const base::unique_qptr<Dialogs::Widget> _dialogs;
	const base::unique_qptr<HistoryWidget> _history;
	object_ptr<Window::SectionWidget> _mainSection = { nullptr };
	object_ptr<Window::SectionWidget> _thirdSection = { nullptr };
	std::shared_ptr<Window::SectionMemento> _thirdSectionFromStack;
	std::unique_ptr<Window::ConnectionState> _connecting;

	base::weak_ptr<Calls::Call> _currentCall;
	base::weak_ptr<Calls::GroupCall> _currentGroupCall;
	rpl::lifetime _currentCallLifetime;
	object_ptr<Ui::SlideWrap<Calls::TopBar>> _callTopBar = { nullptr };

	Export::View::PanelController *_currentExportView = nullptr;
	object_ptr<Window::TopBarWrapWidget<Export::View::TopBar>> _exportTopBar
		= { nullptr };
	rpl::lifetime _exportViewLifetime;

	object_ptr<Window::TopBarWrapWidget<Media::Player::Widget>> _player
		= { nullptr };
	object_ptr<Media::Player::Panel> _playerPlaylist;

	base::unique_qptr<Window::HistoryHider> _hider;
	std::vector<std::unique_ptr<StackItem>> _stack;

	int _playerHeight = 0;
	int _callTopBarHeight = 0;
	int _exportTopBarHeight = 0;
	int _contentScrollAddToY = 0;

	struct SettingBackground;
	std::unique_ptr<SettingBackground> _background;

	// _changelogs depends on _data, subscribes on chats loading event.
	const std::unique_ptr<Core::Changelogs> _changelogs;

};

namespace App {
MainWidget *main();
} // namespace App
