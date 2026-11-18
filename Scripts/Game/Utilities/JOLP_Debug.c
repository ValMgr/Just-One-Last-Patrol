//------------------------------------------------------------------------------------------------
//! Session debug helpers. On by default everywhere except a dedicated server.
class JOLP_Debug : Managed
{
	protected static bool s_bResolved;
	protected static bool s_bEnabled;

	//------------------------------------------------------------------------------------------------
	//! Dedicated server → false. Offline / listen host / Workbench / clients → true.
	static bool IsEnabled()
	{
		if (!s_bResolved)
			Resolve();

		return s_bEnabled;
	}

	//------------------------------------------------------------------------------------------------
	//! Force override (tests / future mission toggle). Pass after Resolve to stick.
	static void SetEnabled(bool enabled)
	{
		s_bResolved = true;
		s_bEnabled = enabled;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Resolve()
	{
		s_bResolved = true;

		if (System.IsConsoleApp() || RplSession.Mode() == RplMode.Dedicated)
		{
			s_bEnabled = false;
			Print("[JOLP] Debug mode OFF (dedicated server)", LogLevel.NORMAL);
			return;
		}

		s_bEnabled = true;
		Print(string.Format("[JOLP] Debug mode ON (session=%1)", typename.EnumToString(RplMode, RplSession.Mode())), LogLevel.NORMAL);
	}
}
