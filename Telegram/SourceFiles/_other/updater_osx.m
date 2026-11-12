/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#import <Cocoa/Cocoa.h>
#include <sys/xattr.h>

NSString *appName = @"64Gram.app";
NSString *appDir = nil;
NSString *workDir = nil;

#ifdef _DEBUG
BOOL _debug = YES;
#else
BOOL _debug = NO;
#endif

NSFileHandle *_logFile = nil;
void openLog() {
	if (!_debug || _logFile) return;
	NSString *logDir = [workDir stringByAppendingString:@"DebugLogs"];
	if (![[NSFileManager defaultManager] createDirectoryAtPath:logDir withIntermediateDirectories:YES attributes:nil error:nil]) {
		return;
	}

	NSDateFormatter *fmt = [[NSDateFormatter alloc] init];
	[fmt setLocale:[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"]];
	[fmt setDateFormat:@"'DebugLogs/'yyyyMMdd'_'HHmmss'_update.txt'"];
	NSString *logPath = [workDir stringByAppendingString:[fmt stringFromDate:[NSDate date]]];
	[[NSFileManager defaultManager] createFileAtPath:logPath contents:nil attributes:nil];
	_logFile = [NSFileHandle fileHandleForWritingAtPath:logPath];
}

void closeLog() {
	if (!_logFile) return;

	[_logFile closeFile];
}

void writeLog(NSString *msg) {
	if (!_logFile) return;

	[_logFile writeData:[[msg stringByAppendingString:@"\n"] dataUsingEncoding:NSUTF8StringEncoding]];
	[_logFile synchronizeFile];
}

void RemoveQuarantineAttribute(NSString *path) {
	const char *kQuarantineAttribute = "com.apple.quarantine";

	writeLog([@"Removing quarantine: " stringByAppendingString:path]);
	removexattr([path fileSystemRepresentation], kQuarantineAttribute, 0);
}

void RemoveQuarantineFromBundle(NSString *path) {
	RemoveQuarantineAttribute(path);
	RemoveQuarantineAttribute([path stringByAppendingString:@"/Contents/MacOS/64Gram"]);
	RemoveQuarantineAttribute([path stringByAppendingString:@"/Contents/Helpers/crashpad_handler"]);
	RemoveQuarantineAttribute([path stringByAppendingString:@"/Contents/Frameworks/Updater"]);
}

void delFolder() {
	writeLog([@"Fully clearing old path: " stringByAppendingString:[workDir stringByAppendingString:@"tupdates/ready"]]);
	if (![[NSFileManager defaultManager] removeItemAtPath:[workDir stringByAppendingString:@"tupdates/ready"] error:nil]) {
		writeLog(@"Failed to clear old path! :( New path was used?..");
	}
	writeLog([@"Fully clearing new path: " stringByAppendingString:[workDir stringByAppendingString:@"tupdates/temp"]]);
	if (![[NSFileManager defaultManager] removeItemAtPath:[workDir stringByAppendingString:@"tupdates/temp"] error:nil]) {
		writeLog(@"Error: failed to clear new path! :(");
	}
	rmdir([[workDir stringByAppendingString:@"tupdates"] fileSystemRepresentation]);
}

int main(int argc, const char * argv[]) {
	NSString *path = [[NSBundle mainBundle] bundlePath];
	if (!path) {
		return -1;
	}
	NSRange range = [path rangeOfString:@".app/" options:NSBackwardsSearch];
	if (range.location == NSNotFound) {
		return -1;
	}
	path = [path substringToIndex:range.location > 0 ? range.location : 0];

	range = [path rangeOfString:@"/" options:NSBackwardsSearch];
	NSString *appRealName = (range.location == NSNotFound) ? path : [path substringFromIndex:range.location + 1];
	appRealName = [[NSArray arrayWithObjects:appRealName, @".app", nil] componentsJoinedByString:@""];
	appDir = (range.location == NSNotFound) ? @"" : [path substringToIndex:range.location + 1];
	NSString *appDirFull = [appDir stringByAppendingString:appRealName];

	openLog();
	pid_t procId = 0;
	BOOL update = YES, toSettings = NO, autoStart = NO, startInTray = NO;
	BOOL customWorkingDir = NO;
	NSString *key = nil;
	for (int i = 0; i < argc; ++i) {
		if ([@"-workpath" isEqualToString:[NSString stringWithUTF8String:argv[i]]]) {
			if (++i < argc) {
				workDir = [NSString stringWithUTF8String:argv[i]];
			}
		} else if ([@"-procid" isEqualToString:[NSString stringWithUTF8String:argv[i]]]) {
			if (++i < argc) {
				NSNumberFormatter *formatter = [[NSNumberFormatter alloc] init];
				[formatter setNumberStyle:NSNumberFormatterDecimalStyle];
				procId = [[formatter numberFromString:[NSString stringWithUTF8String:argv[i]]] intValue];
			}
		} else if ([@"-noupdate" isEqualToString:[NSString stringWithUTF8String:argv[i]]]) {
			update = NO;
		} else if ([@"-tosettings" isEqualToString:[NSString stringWithUTF8String:argv[i]]]) {
			toSettings = YES;
		} else if ([@"-autostart" isEqualToString:[NSString stringWithUTF8String:argv[i]]]) {
			autoStart = YES;
		} else if ([@"-debug" isEqualToString:[NSString stringWithUTF8String:argv[i]]]) {
			_debug = YES;
		} else if ([@"-startintray" isEqualToString:[NSString stringWithUTF8String:argv[i]]]) {
			startInTray = YES;
		} else if ([@"-workdir_custom" isEqualToString:[NSString stringWithUTF8String:argv[i]]]) {
			customWorkingDir = YES;
		} else if ([@"-key" isEqualToString:[NSString stringWithUTF8String:argv[i]]]) {
			if (++i < argc) key = [NSString stringWithUTF8String:argv[i]];
		}
	}
	if (!workDir) {
		workDir = appDir;
		customWorkingDir = NO;
	}
	openLog();
	NSMutableArray *argsArr = [[NSMutableArray alloc] initWithCapacity:argc];
	for (int i = 0; i < argc; ++i) {
		[argsArr addObject:[NSString stringWithUTF8String:argv[i]]];
	}
	writeLog([[NSArray arrayWithObjects:@"Arguments: '", [argsArr componentsJoinedByString:@"' '"], @"'..", nil] componentsJoinedByString:@""]);
	if (key) writeLog([@"Key: " stringByAppendingString:key]);
	if (toSettings) writeLog(@"To Settings!");

	if (procId) {
		NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:procId];
		for (int i = 0; i < 5 && app != nil && ![app isTerminated]; ++i) {
			usleep(200000);
			app = [NSRunningApplication runningApplicationWithProcessIdentifier:procId];
		}
		if (app) [app forceTerminate];
		app = [NSRunningApplication runningApplicationWithProcessIdentifier:procId];
		for (int i = 0; i < 5 && app != nil && ![app isTerminated]; ++i) {
			usleep(200000);
			app = [NSRunningApplication runningApplicationWithProcessIdentifier:procId];
		}
	}

	if (update) {
		NSFileManager *fileManager = [NSFileManager defaultManager];
		NSString *readyFilePath = [workDir stringByAppendingString:@"tupdates/temp/ready"];
		NSString *srcDir = [workDir stringByAppendingString:@"tupdates/temp/"];
		if ([fileManager fileExistsAtPath:readyFilePath]) {
			writeLog([@"Ready file found! Using new path: " stringByAppendingString: srcDir]);
		} else {
			srcDir = [workDir stringByAppendingString:@"tupdates/ready/"]; // old
			writeLog([@"Ready file not found! Using old path: " stringByAppendingString: srcDir]);
		}

		if (![fileManager fileExistsAtPath:[srcDir stringByAppendingString:appName]]) {
			appName = @"Telegram.app";
		}
		NSString *sourceBundle = [srcDir stringByAppendingString:appName];
		NSString *oldVersionUpdaterPath = [appDirFull stringByAppendingString:@"/Contents/Frameworks/Updater"];
		NSString *newVersionUpdaterPath = [sourceBundle stringByAppendingString:@"/Contents/Frameworks/Updater"];
		if (![fileManager fileExistsAtPath:newVersionUpdaterPath]
			&& ![fileManager copyItemAtPath:oldVersionUpdaterPath toPath:newVersionUpdaterPath error:nil]) {
			writeLog(@"Failed to restore the staged update helper.");
			return -1;
		}
		BOOL renamed = [appRealName isEqualToString:@"Telegram.app"]
			&& [fileManager fileExistsAtPath:[sourceBundle stringByAppendingString:@"/Contents/MacOS/64Gram"]];
		NSString *destination = renamed ? [appDir stringByAppendingString:@"64Gram.app"] : appDirFull;
		if (renamed && [fileManager fileExistsAtPath:destination]) {
			writeLog(@"Cannot rename the bundle: 64Gram.app already exists.");
			return -1;
		}
		NSString *prepared = [appDir stringByAppendingString:
			[NSString stringWithFormat:@".64Gram-update-%@.app", [[NSUUID UUID] UUIDString]]];
		NSError *error = nil;
		if (![fileManager copyItemAtPath:sourceBundle toPath:prepared error:&error]) {
			writeLog([@"Could not prepare the new bundle: " stringByAppendingString:[error description]]);
			[fileManager removeItemAtPath:prepared error:nil];
			return -1;
		}
		if (![fileManager replaceItemAtURL:[NSURL fileURLWithPath:appDirFull]
			withItemAtURL:[NSURL fileURLWithPath:prepared]
			backupItemName:nil options:0 resultingItemURL:nil error:&error]) {
			writeLog([@"Could not install the new bundle: " stringByAppendingString:[error description]]);
			[fileManager removeItemAtPath:prepared error:nil];
			return -1;
		}
		if (renamed) {
			if ([fileManager moveItemAtPath:appDirFull toPath:destination error:&error]) {
				appRealName = @"64Gram.app";
			} else {
				writeLog([@"Could not rename the installed bundle: " stringByAppendingString:[error description]]);
			}
		}
		delFolder();
	}

	NSString *appPath = [[NSArray arrayWithObjects:appDir, appRealName, nil] componentsJoinedByString:@""];

	RemoveQuarantineFromBundle(appPath);

	NSMutableArray *args = [[NSMutableArray alloc] initWithObjects: @"-noupdate", nil];
	if (toSettings) [args addObject:@"-tosettings"];
	if (_debug) [args addObject:@"-debug"];
	if (startInTray) [args addObject:@"-startintray"];
	if (autoStart) [args addObject:@"-autostart"];
	if (key) {
		[args addObject:@"-key"];
		[args addObject:key];
	}
	if (customWorkingDir) {
		[args addObject:@"-workdir"];
		[args addObject:workDir];
	}
	writeLog([[NSArray arrayWithObjects:@"Running application '", appPath, @"' with args '", [args componentsJoinedByString:@"' '"], @"'..", nil] componentsJoinedByString:@""]);

	for (int i = 0; i < 5; ++i) {
		NSError *error = nil;
		NSRunningApplication *result = [[NSWorkspace sharedWorkspace]
					launchApplicationAtURL:[NSURL fileURLWithPath:appPath]
					options:NSWorkspaceLaunchDefault
					configuration:[NSDictionary
								   dictionaryWithObject:args
								   forKey:NSWorkspaceLaunchConfigurationArguments]
					error:&error];
		if (result) {
			closeLog();
			return 0;
		}
		writeLog([[NSString stringWithFormat:@"Could not run application, error %ld: ", (long)[error code]] stringByAppendingString: error ? [error localizedDescription] : @"(nil)"]);
		usleep(200000);
	}
	closeLog();
	return -1;
}

