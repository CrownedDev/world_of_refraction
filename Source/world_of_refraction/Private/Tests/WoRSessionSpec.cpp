// WoRSessionSpec.cpp
// B-233 (D-103, D-150): the settings a hosted co-op session advertises (the Null / LAN form and the online form).

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Online/WoRSessionSubsystem.h"

BEGIN_DEFINE_SPEC(FWoRSessionSpec, "WoR.Online.Session", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FWoRSessionSpec)

void FWoRSessionSpec::Define()
{
	It("hosts three players at most (D-150)", [this]()
	{
		TestEqual(TEXT("LAN"), WoRSession::MakeHostSettings(true).NumPublicConnections, WoRSession::MAX_PLAYERS);
		TestEqual(TEXT("online"), WoRSession::MakeHostSettings(false).NumPublicConnections, WoRSession::MAX_PLAYERS);
		TestEqual(TEXT("the host and two friends"), WoRSession::MAX_PLAYERS, 3);
	});

	It("advertises a LAN match without presence or lobbies on the Null subsystem", [this]()
	{
		const FOnlineSessionSettings Settings = WoRSession::MakeHostSettings(true);
		TestTrue(TEXT("LAN"), Settings.bIsLANMatch);
		TestTrue(TEXT("advertised"), Settings.bShouldAdvertise);
		TestFalse(TEXT("no presence"), Settings.bUsesPresence);
		TestFalse(TEXT("no lobbies"), Settings.bUseLobbiesIfAvailable);
	});

	It("uses presence and lobbies online, and lets friends join a run in progress", [this]()
	{
		const FOnlineSessionSettings Settings = WoRSession::MakeHostSettings(false);
		TestFalse(TEXT("not LAN"), Settings.bIsLANMatch);
		TestTrue(TEXT("presence"), Settings.bUsesPresence);
		TestTrue(TEXT("lobbies"), Settings.bUseLobbiesIfAvailable);
		TestTrue(TEXT("join in progress"), Settings.bAllowJoinInProgress);
		TestTrue(TEXT("invites"), Settings.bAllowInvites);
	});
}

#endif
