//------------------------------------------------------------------------------------------------
//! One row in the civilian dialogue. Add a value here, a line in GetChoices, and a case in Answer.
enum JOLP_CivilianTalkChoice
{
	REPUTATION = 0,
	ENEMY_HINT = 1,
	TAKE_COVER = 2
}

//------------------------------------------------------------------------------------------------
//! Server-side replies for the civilian dialogue. The client only receives the sentence.
class JOLP_CivilianTalk
{
	protected static const float HIDEOUT_OFFSET_MIN = 1000;
	protected static const float HIDEOUT_OFFSET_MAX = 4000;
	protected static const float FAKE_HIDEOUT_CLEARANCE = 4000;
	protected static ref array<EntityID> s_aHintTold;

	//------------------------------------------------------------------------------------------------
	static void GetChoices(notnull array<int> ids, notnull array<string> labels)
	{
		ids.Clear();
		labels.Clear();

		ids.Insert(JOLP_CivilianTalkChoice.REPUTATION);
		labels.Insert("What's the current reputation in the settlement?");

		ids.Insert(JOLP_CivilianTalkChoice.ENEMY_HINT);
		labels.Insert("Any hint about enemies?");

		ids.Insert(JOLP_CivilianTalkChoice.TAKE_COVER);
		labels.Insert("Hide & stay cover");
	}

	//------------------------------------------------------------------------------------------------
	static string Answer(int choice, IEntity speaker, IEntity player)
	{
		string reply = Resolve(choice, speaker, player);
		Print(string.Format("[JOLP] Civilian talk: %1", reply), LogLevel.NORMAL);
		return reply;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Resolve(int choice, IEntity speaker, IEntity player)
	{
		JOLP_CivilianManagerComponent civilians = JOLP_Global.GetCivilians();
		if (!civilians || !speaker || !player)
			return "They won't talk.";

		JOLP_CivilianLiveRecord record = civilians.FindFootRecord(speaker);
		if (record && record.m_bDead)
			return "They won't talk.";

		float talkDistance = 4;
		JOLP_CivilianConfig config = civilians.GetConfig();
		if (config.m_fTalkDistance > 0)
			talkDistance = config.m_fTalkDistance;

		if (JOLP_WorldUtils.DistanceFlat(player.GetOrigin(), speaker.GetOrigin()) > talkDistance)
			return "They're too far away.";

		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		JOLP_LocationData place;
		if (locations && record)
			place = locations.GetLocation(record.m_iSourceLocationId);
		else
			place = NearestSettlement(speaker.GetOrigin());

		switch (choice)
		{
			case JOLP_CivilianTalkChoice.REPUTATION:
				return ReputationReply(civilians, place);

			case JOLP_CivilianTalkChoice.ENEMY_HINT:
				return IntelReply(civilians, speaker, place, player.GetOrigin());

			case JOLP_CivilianTalkChoice.TAKE_COVER:
				return ShelterReply(civilians, speaker);
		}

		return "They don't understand.";
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReputationReply(notnull JOLP_CivilianManagerComponent civilians, JOLP_LocationData place)
	{
		if (!place)
			return "They don't know this place.";

		return BandName(civilians.GetStage(place));
	}

	//------------------------------------------------------------------------------------------------
	protected static string IntelReply(notnull JOLP_CivilianManagerComponent civilians, notnull IEntity speaker, JOLP_LocationData place, vector from)
	{
		if (HintAlreadyTold(speaker.GetID()))
			return "They've already told you what they know.";

		if (!place)
			return FinishHint(speaker, true, "They don't know this place.");

		float intelRange = 4000;
		if (civilians.GetConfig().m_fIntelRange > 0)
			intelRange = civilians.GetConfig().m_fIntelRange;

		JOLP_ReputationStage stage = civilians.GetStage(place);
		bool trustworthy = stage == JOLP_ReputationStage.HIGH || stage == JOLP_ReputationStage.VERY_HIGH;
		if (!trustworthy)
			return FinishHint(speaker, false, FalseIntel(from));

		int settlementId = -1;
		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		if (milician)
			settlementId = milician.FindNearestInboundSettlement(from, intelRange);

		JOLP_LocationData hideout = NearestLivingHideout(from);
		bool squad = settlementId >= 0;
		bool cache = hideout != null;
		if (!squad && !cache)
			return FinishHint(speaker, true, "They haven't seen anything unusual.");

		bool tellSquad = squad;
		if (squad && cache)
			tellSquad = Math.RandomInt(0, 2) == 0;

		if (tellSquad)
			return FinishHint(speaker, true, SquadSentence(SettlementName(settlementId)));

		return FinishHint(speaker, true, HideoutSentence(from, ReportedHideoutPoint(hideout)));
	}

	//------------------------------------------------------------------------------------------------
	protected static string FinishHint(notnull IEntity speaker, bool real, string sentence)
	{
		if (!s_aHintTold)
			s_aHintTold = new array<EntityID>();

		EntityID id = speaker.GetID();
		if (s_aHintTold.Find(id) < 0)
			s_aHintTold.Insert(id);

		string kind = "fake";
		if (real)
			kind = "real";

		Print(string.Format("[JOLP] Civilian hint (%1): %2", kind, sentence), LogLevel.NORMAL);
		return sentence;
	}

	//------------------------------------------------------------------------------------------------
	protected static string ShelterReply(notnull JOLP_CivilianManagerComponent civilians, IEntity speaker)
	{
		int inRange;
		int sheltered;
		civilians.OrderShelter(speaker, inRange, sheltered);

		if (inRange <= 0)
			return "Nobody nearby can hear that.";

		if (sheltered <= 0)
			return "They look around, but there is nowhere to hide.";

		if (sheltered == 1)
			return "They hurry indoors.";

		return string.Format("%1 of them hurry indoors.", sheltered);
	}

	//------------------------------------------------------------------------------------------------
	protected static string FalseIntel(vector from)
	{
		bool tellSquad = Math.RandomInt(0, 2) == 0;
		if (tellSquad)
		{
			string squad = FalseSquadSentence();
			if (!squad.IsEmpty())
				return squad;
		}

		string hideout = FalseHideoutText(from);
		if (!hideout.IsEmpty())
			return hideout;

		string squadFallback = FalseSquadSentence();
		if (!squadFallback.IsEmpty())
			return squadFallback;

		return "They haven't seen anything unusual.";
	}

	//------------------------------------------------------------------------------------------------
	protected static string FalseSquadSentence()
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return string.Empty;

		JOLP_MilicianManagerComponent milician = JOLP_Global.GetMilician();
		array<int> candidates = new array<int>();
		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.IsSettlement() || data.m_sName.IsEmpty())
				continue;

			if (milician && milician.HasUnarrivedInbound(data.m_iId))
				continue;

			candidates.Insert(data.m_iId);
		}

		if (candidates.IsEmpty())
			return string.Empty;

		int index = Math.RandomInt(0, candidates.Count());
		return SquadSentence(SettlementName(candidates[index]));
	}

	//------------------------------------------------------------------------------------------------
	protected static string FalseHideoutText(vector from)
	{
		vector reported;
		if (!TryFalseHideoutPoint(from, reported))
			return string.Empty;

		return HideoutSentence(from, reported);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool TryFalseHideoutPoint(vector from, out vector reported)
	{
		reported = from;
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();

		for (int attempt = 0; attempt < 16; attempt++)
		{
			float distance = Math.RandomFloat(1500, 8000);
			float angle = Math.RandomFloat(0, Math.PI2);
			vector point = from + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			point[1] = from[1];

			if (locations && NearLivingHideout(locations, point, FAKE_HIDEOUT_CLEARANCE))
				continue;

			reported = point;
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static string SquadSentence(string settlement)
	{
		if (settlement.IsEmpty())
			return "They haven't seen anything unusual.";

		return string.Format("A group is moving toward %1.", settlement);
	}

	//------------------------------------------------------------------------------------------------
	protected static string HideoutSentence(vector from, vector reported)
	{
		float metres = JOLP_WorldUtils.DistanceFlat(from, reported);
		return string.Format("There's a hideout about %1 to the %2.", Kilometres(metres), Compass(from, reported));
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HintAlreadyTold(EntityID id)
	{
		if (!s_aHintTold)
			return false;

		return s_aHintTold.Find(id) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	protected static JOLP_LocationData NearestSettlement(vector from)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return null;

		JOLP_LocationData best;
		float bestDist = 0;
		foreach (JOLP_LocationData data : locations.GetAllLocations())
		{
			if (!data || !data.IsSettlement())
				continue;

			float dist = JOLP_WorldUtils.DistanceFlat(from, data.m_vPosition);
			if (!best || dist < bestDist)
			{
				best = data;
				bestDist = dist;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected static string SettlementName(int locationId)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return string.Empty;

		JOLP_LocationData data = locations.GetLocation(locationId);
		if (!data)
			return string.Empty;

		return data.m_sName;
	}

	//------------------------------------------------------------------------------------------------
	protected static JOLP_LocationData NearestLivingHideout(vector from)
	{
		JOLP_LocationManagerComponent locations = JOLP_Global.GetLocations();
		if (!locations)
			return null;

		JOLP_LocationData best;
		float bestDist = 0;
		foreach (JOLP_LocationData data : locations.GetHideouts())
		{
			if (!data || data.m_bHideoutDestroyed)
				continue;

			float dist = JOLP_WorldUtils.DistanceFlat(from, data.m_vPosition);
			if (!best)
			{
				best = data;
				bestDist = dist;
				continue;
			}

			if (dist < bestDist)
			{
				best = data;
				bestDist = dist;
			}
		}

		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Same point for the life of the server, so asking again cannot be averaged back to the real site.
	protected static vector ReportedHideoutPoint(notnull JOLP_LocationData hideout)
	{
		if (!hideout.m_bTalkOffsetReady)
		{
			float distance = Math.RandomFloat(HIDEOUT_OFFSET_MIN, HIDEOUT_OFFSET_MAX);
			float angle = Math.RandomFloat(0, Math.PI2);
			hideout.m_vTalkOffset = Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			hideout.m_bTalkOffsetReady = true;
		}

		vector reported = hideout.m_vPosition + hideout.m_vTalkOffset;
		reported[1] = hideout.m_vPosition[1];
		return reported;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool NearLivingHideout(notnull JOLP_LocationManagerComponent locations, vector point, float clearance)
	{
		foreach (JOLP_LocationData data : locations.GetHideouts())
		{
			if (!data || data.m_bHideoutDestroyed)
				continue;

			if (JOLP_WorldUtils.DistanceFlat(point, data.m_vPosition) < clearance)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static string BandName(JOLP_ReputationStage stage)
	{
		switch (stage)
		{
			case JOLP_ReputationStage.VERY_LOW:
				return "Very low";
			case JOLP_ReputationStage.LOW:
				return "Low";
			case JOLP_ReputationStage.MODERATE:
				return "Moderate";
			case JOLP_ReputationStage.HIGH:
				return "High";
			case JOLP_ReputationStage.VERY_HIGH:
				return "Very high";
		}

		return "Low";
	}

	//------------------------------------------------------------------------------------------------
	protected static string Kilometres(float metres)
	{
		int km = Math.Round(metres / 1000);
		if (km < 1)
			km = 1;

		if (km == 1)
			return "1 kilometre";

		return string.Format("%1 kilometres", km);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Compass(vector from, vector to)
	{
		vector direction = to - from;
		float yaw = Math.Atan2(direction[0], direction[2]) * Math.RAD2DEG;
		if (yaw < 0)
			yaw = yaw + 360;

		int sector = Math.Round(yaw / 45);
		if (sector >= 8)
			sector = 0;

		switch (sector)
		{
			case 0:
				return "north";
			case 1:
				return "north-east";
			case 2:
				return "east";
			case 3:
				return "south-east";
			case 4:
				return "south";
			case 5:
				return "south-west";
			case 6:
				return "west";
			case 7:
				return "north-west";
		}

		return "north";
	}
}
