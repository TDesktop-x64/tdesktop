/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "mtproto/sender.h"
#include "base/algorithm.h"

namespace Ui {
struct ColorIndicesCompressed;
} // namespace Ui

namespace Main {

class Account;

class AppConfig final {
public:
	explicit AppConfig(not_null<Account*> account);
	~AppConfig();

	void start();

	template <typename Type>
	[[nodiscard]] Type get(const QString &key, Type fallback) const {
		if constexpr (std::is_same_v<Type, double>) {
			return getDouble(key, fallback);
		} else if constexpr (std::is_same_v<Type, int>) {
			return int(base::SafeRound(getDouble(key, double(fallback))));
		} else if constexpr (std::is_same_v<Type, QString>) {
			return getString(key, fallback);
		} else if constexpr (std::is_same_v<Type, std::vector<QString>>) {
			return getStringArray(key, std::move(fallback));
		} else if constexpr (
				std::is_same_v<Type, base::flat_map<QString, QString>>) {
			return getStringMap(key, std::move(fallback));
		} else if constexpr (std::is_same_v<Type, std::vector<int>>) {
			return getIntArray(key, std::move(fallback));
		} else if constexpr (std::is_same_v<Type, bool>) {
			return getBool(key, fallback);
		}
	}

	[[nodiscard]] rpl::producer<> refreshed() const;
	[[nodiscard]] rpl::producer<> value() const;

	[[nodiscard]] bool suggestionCurrent(const QString &key) const;
	[[nodiscard]] rpl::producer<> suggestionRequested(
		const QString &key) const;
	void dismissSuggestion(const QString &key);

	[[nodiscard]] bool newRequirePremiumFree() const;

	[[nodiscard]] auto ignoredRestrictionReasons() const
		-> const std::vector<QString> & {
		return _ignoreRestrictionReasons;
	}
	[[nodiscard]] auto ignoredRestrictionReasonsChanges() const {
		return _ignoreRestrictionChanges.events();
	}

	[[nodiscard]] int quoteLengthMax() const;
	[[nodiscard]] int stargiftConvertPeriodMax() const;

	[[nodiscard]] const std::vector<QString> &startRefPrefixes();
	[[nodiscard]] bool starrefSetupAllowed() const;
	[[nodiscard]] bool starrefJoinAllowed() const;
	[[nodiscard]] int starrefCommissionMin() const;
	[[nodiscard]] int starrefCommissionMax() const;

	[[nodiscard]] float64 starsWithdrawRate() const;
	[[nodiscard]] bool paidMessagesAvailable() const;
	[[nodiscard]] int paidMessageStarsMax() const;
	[[nodiscard]] int paidMessageCommission() const;

	[[nodiscard]] int pinnedGiftsLimit() const;

	void refresh(bool force = false);

private:
	void refreshDelayed();

	template <typename Extractor>
	[[nodiscard]] auto getValue(
		const QString &key,
		Extractor &&extractor) const;

	[[nodiscard]] bool getBool(
		const QString &key,
		bool fallback) const;
	[[nodiscard]] double getDouble(
		const QString &key,
		double fallback) const;
	[[nodiscard]] QString getString(
		const QString &key,
		const QString &fallback) const;
	[[nodiscard]] std::vector<QString> getStringArray(
		const QString &key,
		std::vector<QString> &&fallback) const;
	[[nodiscard]] base::flat_map<QString, QString> getStringMap(
		const QString &key,
		base::flat_map<QString, QString> &&fallback) const;
	[[nodiscard]] std::vector<int> getIntArray(
		const QString &key,
		std::vector<int> &&fallback) const;

	void updateIgnoredRestrictionReasons(std::vector<QString> was);

	const not_null<Account*> _account;
	std::optional<MTP::Sender> _api;
	mtpRequestId _requestId = 0;
	int32 _hash = 0;
	bool _pendingRefresh = false;
	base::flat_map<QString, MTPJSONValue> _data;
	rpl::event_stream<> _refreshed;
	base::flat_set<QString> _dismissedSuggestions;

	std::vector<QString> _ignoreRestrictionReasons;
	rpl::event_stream<std::vector<QString>> _ignoreRestrictionChanges;

	std::vector<QString> _startRefPrefixes;

	crl::time _lastFrozenRefresh = 0;
	rpl::lifetime _frozenTrackLifetime;

	rpl::lifetime _lifetime;

};

} // namespace Main
