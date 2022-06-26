/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/style/style_core.h"

#define DeclareReadSetting(Type, Name) extern Type g##Name; \
inline const Type &c##Name() { \
	return g##Name; \
}

#define DeclareSetting(Type, Name) DeclareReadSetting(Type, Name) \
inline void cSet##Name(const Type &Name) { \
	g##Name = Name; \
}

#define DeclareRefSetting(Type, Name) DeclareSetting(Type, Name) \
inline Type &cRef##Name() { \
	return g##Name; \
}

DeclareSetting(Qt::LayoutDirection, LangDir);
inline bool rtl() {
	return style::RightToLeft();
}

DeclareSetting(bool, InstallBetaVersion);
DeclareSetting(uint64, AlphaVersion);
DeclareSetting(uint64, RealAlphaVersion);
DeclareSetting(QByteArray, AlphaPrivateKey);

DeclareSetting(bool, AutoStart);
DeclareSetting(bool, StartMinimized);
DeclareSetting(bool, StartInTray);
DeclareSetting(bool, SendToMenu);
DeclareSetting(bool, UseExternalVideoPlayer);
DeclareSetting(bool, UseFreeType);
enum LaunchMode {
	LaunchModeNormal = 0,
	LaunchModeAutoStart,
	LaunchModeFixPrevious,
	LaunchModeCleanup,
};
DeclareReadSetting(LaunchMode, LaunchMode);
DeclareSetting(QString, WorkingDir);
inline void cForceWorkingDir(const QString &newDir) {
	cSetWorkingDir(newDir);
	if (!gWorkingDir.isEmpty()) {
		QDir().mkpath(gWorkingDir);
		QFile::setPermissions(gWorkingDir,
			QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser);
	}

}
DeclareReadSetting(QString, ExeName);
DeclareReadSetting(QString, ExeDir);
DeclareSetting(QString, DialogLastPath);
DeclareSetting(QString, DialogHelperPath);
inline const QString &cDialogHelperPathFinal() {
	return cDialogHelperPath().isEmpty() ? cExeDir() : cDialogHelperPath();
}

DeclareSetting(bool, AutoUpdate);

DeclareSetting(bool, SeenTrayTooltip);
DeclareSetting(bool, RestartingUpdate);
DeclareSetting(bool, Restarting);
DeclareSetting(bool, RestartingToSettings);
DeclareSetting(bool, WriteProtected);
DeclareSetting(int32, LastUpdateCheck);
DeclareSetting(bool, NoStartUpdate);
DeclareSetting(bool, StartToSettings);
DeclareSetting(bool, DebugMode);
DeclareReadSetting(bool, ManyInstance);
DeclareSetting(bool, Quit);

DeclareSetting(QByteArray, LocalSalt);
DeclareSetting(int, ScreenScale);
DeclareSetting(int, ConfigScale);
DeclareSetting(QString, DateFormat);
DeclareSetting(QString, TimeFormat);

class DocumentData;

typedef QList<QPair<DocumentData*, int16>> RecentStickerPackOld;
typedef QVector<QPair<uint64, ushort>> RecentStickerPreload;
typedef QVector<QPair<DocumentData*, ushort>> RecentStickerPack;
DeclareSetting(RecentStickerPreload, RecentStickersPreload);
DeclareRefSetting(RecentStickerPack, RecentStickers);

typedef QList<QPair<QString, ushort>> RecentHashtagPack;
DeclareRefSetting(RecentHashtagPack, RecentWriteHashtags);
DeclareSetting(RecentHashtagPack, RecentSearchHashtags);

class UserData;
typedef QVector<UserData*> RecentInlineBots;
DeclareRefSetting(RecentInlineBots, RecentInlineBots);

DeclareSetting(bool, PasswordRecovered);

DeclareSetting(int32, PasscodeBadTries);
DeclareSetting(crl::time, PasscodeLastTry);

DeclareSetting(QStringList, SendPaths);
DeclareSetting(QString, StartUrl);

DeclareSetting(int, OtherOnline);

inline void cChangeDateFormat(const QString &newFormat) {
	if (!newFormat.isEmpty()) cSetDateFormat(newFormat);
}

inline void cChangeTimeFormat(const QString &newFormat) {
	if (!newFormat.isEmpty()) cSetTimeFormat(newFormat);
}

inline bool passcodeCanTry() {
	if (cPasscodeBadTries() < 3) return true;
	auto dt = crl::now() - cPasscodeLastTry();
	switch (cPasscodeBadTries()) {
	case 3: return dt >= 5000;
	case 4: return dt >= 10000;
	case 5: return dt >= 15000;
	case 6: return dt >= 20000;
	case 7: return dt >= 25000;
	}
	return dt >= 30000;
}

inline float64 cRetinaFactor() {
	return style::DevicePixelRatio();
}

inline int32 cIntRetinaFactor() {
	return style::DevicePixelRatio();
}

inline int cEvalScale(int scale) {
	return (scale == style::kScaleAuto) ? cScreenScale() : scale;
}

inline int cScale() {
	return style::Scale();
}

inline void SetScaleChecked(int scale) {
	cSetConfigScale(style::CheckScale(scale));
}

inline void ValidateScale() {
	SetScaleChecked(cConfigScale());
	style::SetScale(cEvalScale(cConfigScale()));
}

DeclareSetting(bool, EnhancedFirstRun);
DeclareSetting(bool, VoiceChatPinned);
DeclareSetting(QList<int64>, BlockList);
typedef QHash<QString, QVariant> EnhancedSetting;
DeclareSetting(EnhancedSetting, EnhancedOptions);

DeclareSetting(int, NetRequestsCount);
DeclareSetting(int, NetUploadSessionsCount);
DeclareSetting(int, NetUploadRequestInterval);

inline bool GetEnhancedBool(const QString& key) {
	if (!gEnhancedOptions.contains(key)) {
		return false;
	}
	return gEnhancedOptions[key].toBool();
}

inline int GetEnhancedInt(const QString& key) {
	if (!gEnhancedOptions.contains(key)) {
		return 0;
	}
	return gEnhancedOptions[key].toInt();
}

inline QString GetEnhancedString(const QString& key) {
	if (!gEnhancedOptions.contains(key)) {
		return {};
	}
	return gEnhancedOptions[key].toString();
}

inline void SetEnhancedValue(const QString& key, const QVariant& value) {
	gEnhancedOptions.insert(key, value);
}

inline void SetNetworkBoost(int boost) {
	if (boost < 0) {
		gEnhancedOptions.insert("net_speed_boost", 0);
	} else if (boost > 3) {
		gEnhancedOptions.insert("net_speed_boost", 3);
	} else {
		gEnhancedOptions.insert("net_speed_boost", boost);
	}

	cSetNetRequestsCount(2 + (2 * GetEnhancedInt("net_speed_boost")));
	cSetNetUploadSessionsCount(2 + (2 * GetEnhancedInt("net_speed_boost")));
	cSetNetUploadRequestInterval(500 - (100 * GetEnhancedInt("net_speed_boost")));
}

inline bool blockExist(int64 id) {
	if (cBlockList().contains(id)) {
		return true;
	}
	return false;
}

inline void loadSettings(QJsonObject settings) {
	for (const auto & key : settings.keys()) {
		if (settings[key].type() == QJsonValue::Bool) {
			gEnhancedOptions.insert(key, settings[key].toBool());
		}
		else if (settings[key].type() == QJsonValue::Double) {
			gEnhancedOptions.insert(key, settings[key].toInt());
		}
		else if (settings[key].type() == QJsonValue::String) {
			gEnhancedOptions.insert(key, settings[key].toString());
		}
	}
}
