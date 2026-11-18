class JOLP_ComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! One-line typed component finder. EnforceScript has no generic methods, so this is a generic class.
//! Usage: JOLP_ComponentFinder<JOLP_LocationManagerComponent>.Find(entity)
class JOLP_ComponentFinder<Class T>
{
	static T Find(IEntity entity)
	{
		if (!entity)
			return null;

		return T.Cast(entity.FindComponent(T));
	}
}

//------------------------------------------------------------------------------------------------
//! Base component for Just One Last Patrol systems.
class JOLP_Component : ScriptComponent
{
	//------------------------------------------------------------------------------------------------
	//! True when the owner is a Workbench preview / resource-browser instance, not a live world entity.
	static bool IsPreviewInstance(IEntity owner)
	{
		if (!owner)
			return true;

		BaseWorld ownerWorld = owner.GetWorld();
		if (!ownerWorld)
			return true;

		// Prefer identity with the active game world. ChimeraWorld.CastFrom can fail on valid
		// composition roots and would permanently skip FOB / location registration.
		BaseWorld gameWorld = GetGame().GetWorld();
		if (!gameWorld)
			return false;

		return ownerWorld != gameWorld;
	}

	//------------------------------------------------------------------------------------------------
	RplComponent GetRpl()
	{
		return RplComponent.Cast(GetOwner().FindComponent(RplComponent));
	}
}
