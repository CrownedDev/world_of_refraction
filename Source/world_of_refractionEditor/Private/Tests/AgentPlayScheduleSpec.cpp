// AgentPlayScheduleSpec.cpp
// B-573, headless: a sequence's steps become events in time order (a tap is a press and a later release, a mouse step
// moves first), a release never shares its press's frame, and bad steps are refused with the reason.
// Unproven here: the inputs reaching the PIE game, the screenshots and the window size (the brain's live check).

#if WITH_DEV_AUTOMATION_TESTS

#include "AgentPlay/AgentPlaySchedule.h"
#include "Misc/AutomationTest.h"

BEGIN_DEFINE_SPEC(FAgentPlayScheduleSpec, "WoR.AgentPlay.Schedule", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
static FWoRAgentPlayStep Step(EWoRAgentPlayAction Action, int32 AtMs, const TCHAR *Key = TEXT(""), int32 X = 0, int32 Y = 0)
{
	FWoRAgentPlayStep Made;
	Made.Action = Action;
	Made.AtMs = AtMs;
	Made.Key = Key;
	Made.X = X;
	Made.Y = Y;
	return Made;
}
END_DEFINE_SPEC(FAgentPlayScheduleSpec)

void FAgentPlayScheduleSpec::Define()
{
	It("turns a tap into a press and a release HoldMs later, in time order with the other steps", [this]()
	{
		const TArray<FWoRAgentPlayStep> Steps = {Step(EWoRAgentPlayAction::KeyTap, 100, TEXT("Tab")), Step(EWoRAgentPlayAction::Screenshot, 120),
												 Step(EWoRAgentPlayAction::KeyDown, 0, TEXT("W"))};
		const TArray<FWoRAgentPlayEvent> Events = WoRAgentPlaySchedule::BuildEvents(Steps);
		if (!TestEqual(TEXT("four events"), Events.Num(), 4)) return;
		TestTrue(TEXT("W first"), Events[0].Key == EKeys::W);
		TestEqual(TEXT("Tab down at 100"), Events[1].AtMs, 100);
		TestTrue(TEXT("Tab down is a press"), Events[1].Action == EWoRAgentPlayAction::KeyDown && Events[1].Key == EKeys::Tab);
		TestTrue(TEXT("the screenshot between"), Events[2].Action == EWoRAgentPlayAction::Screenshot && Events[2].StepIndex == 1);
		TestTrue(TEXT("Tab up at 150"), Events[3].Action == EWoRAgentPlayAction::KeyUp && Events[3].AtMs == 100 + WoRAgentPlaySchedule::DEFAULT_HOLD_MS);
	});

	It("moves the pointer before a click, a wheel turn or a button step, and defaults to the left button", [this]()
	{
		const TArray<FWoRAgentPlayEvent> Click = WoRAgentPlaySchedule::BuildEvents({Step(EWoRAgentPlayAction::MouseClick, 0, TEXT(""), 10, 20)});
		if (!TestEqual(TEXT("move, down, up"), Click.Num(), 3)) return;
		TestTrue(TEXT("move first"), Click[0].Action == EWoRAgentPlayAction::MouseMove);
		TestTrue(TEXT("left down"), Click[1].Action == EWoRAgentPlayAction::MouseDown && Click[1].Key == EKeys::LeftMouseButton);
		TestTrue(TEXT("up later"), Click[2].Action == EWoRAgentPlayAction::MouseUp && Click[2].AtMs == WoRAgentPlaySchedule::DEFAULT_HOLD_MS);
		TestEqual(TEXT("a wheel turn: move, wheel"), WoRAgentPlaySchedule::BuildEvents({Step(EWoRAgentPlayAction::MouseWheel, 0)}).Num(), 2);
		TestEqual(TEXT("a move alone is one event"), WoRAgentPlaySchedule::BuildEvents({Step(EWoRAgentPlayAction::MouseMove, 0)}).Num(), 1);
	});

	It("sends what is due and keeps a release out of its press's frame, even when both are due", [this]()
	{
		FWoRAgentPlayStep Tap = Step(EWoRAgentPlayAction::KeyTap, 0, TEXT("Tab"));
		const TArray<FWoRAgentPlayEvent> Events = WoRAgentPlaySchedule::BuildEvents({Tap, Step(EWoRAgentPlayAction::KeyDown, 60, TEXT("W"))});
		TestEqual(TEXT("frame at 0 ms: the press only"), WoRAgentPlaySchedule::CountDue(Events, 0, 0), 1);
		TestEqual(TEXT("frame at 20 ms: the release is not due"), WoRAgentPlaySchedule::CountDue(Events, 1, 20), 0);
		TestEqual(TEXT("frame at 70 ms: the release and W"), WoRAgentPlaySchedule::CountDue(Events, 1, 70), 2);
		// A long first frame: everything is due at once, and the release still waits for the next frame.
		TestEqual(TEXT("a hitch: the press only"), WoRAgentPlaySchedule::CountDue(Events, 0, 500), 1);
		TestEqual(TEXT("the next frame: the rest, in order"), WoRAgentPlaySchedule::CountDue(Events, 1, 516), 2);
		TestEqual(TEXT("nothing left"), WoRAgentPlaySchedule::CountDue(Events, 3, 600), 0);
	});

	It("refuses bad steps with the reason", [this]()
	{
		const FIntPoint Picture(1280, 720);
		TestFalse(TEXT("no steps"), WoRAgentPlaySchedule::Validate({}, Picture).IsEmpty());
		TestFalse(TEXT("an unknown key"), WoRAgentPlaySchedule::Validate({Step(EWoRAgentPlayAction::KeyTap, 0, TEXT("NotAKey"))}, Picture).IsEmpty());
		TestFalse(TEXT("a key step with no key"), WoRAgentPlaySchedule::Validate({Step(EWoRAgentPlayAction::KeyDown, 0)}, Picture).IsEmpty());
		TestFalse(TEXT("a key as a mouse button"), WoRAgentPlaySchedule::Validate({Step(EWoRAgentPlayAction::MouseClick, 0, TEXT("Tab"), 5, 5)}, Picture).IsEmpty());
		TestFalse(TEXT("a point off the picture"), WoRAgentPlaySchedule::Validate({Step(EWoRAgentPlayAction::MouseMove, 0, TEXT(""), 1280, 10)}, Picture).IsEmpty());
		TestFalse(TEXT("a negative time"), WoRAgentPlaySchedule::Validate({Step(EWoRAgentPlayAction::Screenshot, -1)}, Picture).IsEmpty());
		TestFalse(TEXT("past the longest run"), WoRAgentPlaySchedule::Validate({Step(EWoRAgentPlayAction::Screenshot, WoRAgentPlaySchedule::MAX_SEQUENCE_MS + 1)}, Picture).IsEmpty());
		const TArray<FWoRAgentPlayStep> Good = {Step(EWoRAgentPlayAction::KeyTap, 0, TEXT("Tab")), Step(EWoRAgentPlayAction::MouseClick, 50, TEXT("RightMouseButton"), 1279, 719),
												Step(EWoRAgentPlayAction::Screenshot, 200)};
		TestTrue(TEXT("good steps pass"), WoRAgentPlaySchedule::Validate(Good, Picture).IsEmpty());
	});
}

#endif // WITH_DEV_AUTOMATION_TESTS
