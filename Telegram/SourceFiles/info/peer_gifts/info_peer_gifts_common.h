/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/qt/qt_compare.h"
#include "data/data_star_gift.h"
#include "ui/abstract_button.h"
#include "ui/effects/premium_stars_colored.h"
#include "ui/text/text.h"

class StickerPremiumMark;

namespace ChatHelpers {
class Show;
} // namespace ChatHelpers

namespace Data {
struct UniqueGift;
struct CreditsHistoryEntry;
class SavedStarGiftId;
} // namespace Data

namespace HistoryView {
class StickerPlayer;
} // namespace HistoryView

namespace Main {
class Session;
} // namespace Main

namespace Ui {
class DynamicImage;
} // namespace Ui

namespace Ui::Text {
class CustomEmoji;
} // namespace Ui::Text

namespace Window {
class SessionController;
} // namespace Window

namespace Info::PeerGifts {

struct GiftTypePremium {
	int64 cost = 0;
	QString currency;
	int stars = 0;
	int months = 0;
	int discountPercent = 0;

	[[nodiscard]] friend inline bool operator==(
		const GiftTypePremium &,
		const GiftTypePremium &) = default;
};

struct GiftTypeStars {
	Data::StarGift info;
	PeerData *from = nullptr;
	TimeId date = 0;
	bool pinnedSelection : 1 = false;
	bool userpic : 1 = false;
	bool pinned : 1 = false;
	bool hidden : 1 = false;
	bool mine : 1 = false;

	[[nodiscard]] friend inline bool operator==(
		const GiftTypeStars&,
		const GiftTypeStars&) = default;
};

struct GiftDescriptor : std::variant<GiftTypePremium, GiftTypeStars> {
	using variant::variant;

	[[nodiscard]] friend inline bool operator==(
		const GiftDescriptor&,
		const GiftDescriptor&) = default;
};

struct GiftBadge {
	QString text;
	QColor bg1;
	QColor bg2 = QColor(0, 0, 0, 0);
	QColor fg;
	bool gradient = false;
	bool small = false;

	explicit operator bool() const {
		return !text.isEmpty();
	}

	friend std::strong_ordering operator<=>(
		const GiftBadge &a,
		const GiftBadge &b);

	friend inline bool operator==(
		const GiftBadge &,
		const GiftBadge &) = default;
};

enum class GiftButtonMode {
	Full,
	Minimal,
};

class GiftButtonDelegate {
public:
	[[nodiscard]] virtual TextWithEntities star() = 0;
	[[nodiscard]] virtual TextWithEntities ministar() = 0;
	[[nodiscard]] virtual Ui::Text::MarkedContext textContext() = 0;
	[[nodiscard]] virtual QSize buttonSize() = 0;
	[[nodiscard]] virtual QMargins buttonExtend() = 0;
	[[nodiscard]] virtual auto buttonPatternEmoji(
		not_null<Data::UniqueGift*> unique,
		Fn<void()> repaint)
	-> std::unique_ptr<Ui::Text::CustomEmoji> = 0;
	[[nodiscard]] virtual QImage background() = 0;
	[[nodiscard]] virtual rpl::producer<not_null<DocumentData*>> sticker(
		const GiftDescriptor &descriptor) = 0;
	[[nodiscard]] virtual not_null<StickerPremiumMark*> hiddenMark() = 0;
	[[nodiscard]] virtual QImage cachedBadge(const GiftBadge &badge) = 0;
};

class GiftButton final : public Ui::AbstractButton {
public:
	GiftButton(QWidget *parent, not_null<GiftButtonDelegate*> delegate);
	~GiftButton();

	using Mode = GiftButtonMode;
	void setDescriptor(const GiftDescriptor &descriptor, Mode mode);
	void setGeometry(QRect inner, QMargins extend);

	void toggleSelected(bool selected);

	[[nodiscard]] rpl::producer<QPoint> contextMenuRequests() const {
		return _contextMenuRequests.events();
	}

private:
	void paintEvent(QPaintEvent *e) override;
	void resizeEvent(QResizeEvent *e) override;
	void contextMenuEvent(QContextMenuEvent *e) override;

	void paintBackground(QPainter &p, const QImage &background);
	void cacheUniqueBackground(
		not_null<Data::UniqueGift*> unique,
		int width,
		int height);

	void setDocument(not_null<DocumentData*> document);
	[[nodiscard]] bool documentResolved() const;
	[[nodiscard]] QMargins currentExtend() const;

	void unsubscribe();

	const not_null<GiftButtonDelegate*> _delegate;
	rpl::event_stream<QPoint> _contextMenuRequests;
	QImage _hiddenBgCache;
	GiftDescriptor _descriptor;
	Ui::Text::String _text;
	Ui::Text::String _price;
	Ui::Text::String _byStars;
	std::shared_ptr<Ui::DynamicImage> _userpic;
	QImage _uniqueBackgroundCache;
	std::unique_ptr<Ui::Text::CustomEmoji> _uniquePatternEmoji;
	base::flat_map<float64, QImage> _uniquePatternCache;
	std::optional<Ui::Premium::ColoredMiniStars> _stars;
	Ui::Animations::Simple _selectedAnimation;
	bool _subscribed = false;
	bool _patterned = false;
	bool _selected = false;
	bool _small = false;

	QRect _button;
	QMargins _extend;

	std::unique_ptr<HistoryView::StickerPlayer> _player;
	rpl::lifetime _mediaLifetime;

};

class Delegate final : public GiftButtonDelegate {
public:
	Delegate(not_null<Main::Session*> session, GiftButtonMode mode);
	Delegate(Delegate &&other);
	~Delegate();

	TextWithEntities star() override;
	TextWithEntities ministar() override;
	Ui::Text::MarkedContext textContext() override;
	QSize buttonSize() override;
	QMargins buttonExtend() override;
	auto buttonPatternEmoji(
		not_null<Data::UniqueGift*> unique,
		Fn<void()> repaint)
	-> std::unique_ptr<Ui::Text::CustomEmoji> override;
	QImage background() override;
	rpl::producer<not_null<DocumentData*>> sticker(
		const GiftDescriptor &descriptor) override;
	not_null<StickerPremiumMark*> hiddenMark() override;
	QImage cachedBadge(const GiftBadge &badge) override;

private:
	const not_null<Main::Session*> _session;
	std::unique_ptr<StickerPremiumMark> _hiddenMark;
	base::flat_map<GiftBadge, QImage> _badges;
	QSize _single;
	QImage _bg;
	GiftButtonMode _mode = GiftButtonMode::Full;

};

[[nodiscard]] DocumentData *LookupGiftSticker(
	not_null<Main::Session*> session,
	const GiftDescriptor &descriptor);

[[nodiscard]] rpl::producer<not_null<DocumentData*>> GiftStickerValue(
	not_null<Main::Session*> session,
	const GiftDescriptor &descriptor);

[[nodiscard]] QImage ValidateRotatedBadge(const GiftBadge &badge, int added);

void SelectGiftToUnpin(
	std::shared_ptr<ChatHelpers::Show> show,
	const std::vector<Data::CreditsHistoryEntry> &pinned,
	Fn<void(Data::SavedStarGiftId)> chosen);

} // namespace Info::PeerGifts
