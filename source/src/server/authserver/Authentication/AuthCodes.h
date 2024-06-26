/*
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _AUTHCODES_H
#define _AUTHCODES_H

enum AuthResult
{
    WOW_SUCCESS                                  = 0x00,//Logs in.
    WOW_FAIL_BANNED                              = 0x03,//"Closed and no longer available for use." (Permaban)
    WOW_FAIL_UNKNOWN_ACCOUNT                     = 0x04,//"Your information is invalid. please check the..."
    WOW_FAIL_INCORRECT_PASSWORD                  = 0x05,//"Your information is invalid. please check the..."
    WOW_FAIL_ALREADY_ONLINE                      = 0x06,//"Your account is already logged into world of warcraft"
    WOW_FAIL_NO_TIME                             = 0x07,//"You have used up your prepaid time on this account."
    WOW_FAIL_DB_BUSY                             = 0x08,//"Could not log into wow at this time. try again later."
    WOW_FAIL_VERSION_INVALID                     = 0x09,//Unable to validate game version
    WOW_FAIL_VERSION_UPDATE                      = 0x0A,//Stuck at "Downloading..."
    WOW_FAIL_SUSPENDED                           = 0x0C,//"This wow account has been temporarily suspended"
    WOW_SUCCESS_SURVEY                           = 0x0E,//Fakes success, never loads. I guess it's looking for a survey from bnet.
    WOW_FAIL_PARENTCONTROL                       = 0x0F,//"Access to this account is restricted by parental controls"
    WOW_FAIL_LOCKED_ENFORCED                     = 0x10,//"You have applied a lock to your account"
    WOW_FAIL_TRIAL_ENDED                         = 0x11,//trial has expired
    WOW_FAIL_USE_BATTLENET                       = 0x12,//"This account is now attached to a battle.net account... Use your email address..."
    WOW_FAIL_TOO_FAST                            = 0x16,//"Closed due to chargeback"
    WOW_FAIL_CHARGEBACK                          = 0x17,//"In order to log in using IGR time, use a battle.net account"
    WOW_FAIL_GAME_ACCOUNT_LOCKED                 = 0x18,//temporarily disabled
    WOW_FAIL_UNLOCKABLE_LOCK                     = 0x19,//WOW_FAIL_UNLOCKABLE_LOCK
    WOW_FAIL_MUST_CONVERT_TO_BNET                = 0x20,//must be converted to battle.net
    WOW_FAIL_DISCONNECTED                        = 0xFF,//disconnected
};

enum LoginResult
{
    LOGIN_OK                                     = 0x00,
    LOGIN_FAILED                                 = 0x01,
    LOGIN_FAILED2                                = 0x02,
    LOGIN_BANNED                                 = 0x03,
    LOGIN_UNKNOWN_ACCOUNT                        = 0x04,
    LOGIN_UNKNOWN_ACCOUNT3                       = 0x05,
    LOGIN_ALREADYONLINE                          = 0x06,
    LOGIN_NOTIME                                 = 0x07,
    LOGIN_DBBUSY                                 = 0x08,
    LOGIN_BADVERSION                             = 0x09,
    LOGIN_DOWNLOAD_FILE                          = 0x0A,
    LOGIN_FAILED3                                = 0x0B,
    LOGIN_SUSPENDED                              = 0x0C,
    LOGIN_FAILED4                                = 0x0D,
    LOGIN_CONNECTED                              = 0x0E,
    LOGIN_PARENTALCONTROL                        = 0x0F,
    LOGIN_LOCKED_ENFORCED                        = 0x10,
};

enum ExpansionFlags
{
    POST_BC_EXP_FLAG                            = 0x2,
    PRE_BC_EXP_FLAG                             = 0x1,
    NO_VALID_EXP_FLAG                           = 0x0
};

struct RealmBuildInfo
{
    int Build;
    int MajorVersion;
    int MinorVersion;
    int BugfixVersion;
    int HotfixVersion;
};

namespace AuthHelper
{
    RealmBuildInfo const* GetBuildInfo(int build);
    bool IsAcceptedClientBuild(int build);
    bool IsPostBCAcceptedClientBuild(int build);
    bool IsPreBCAcceptedClientBuild(int build);
};

#endif
