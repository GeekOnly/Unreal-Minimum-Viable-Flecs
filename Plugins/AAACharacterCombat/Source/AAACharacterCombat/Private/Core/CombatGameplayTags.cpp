#include "Core/CombatGameplayTags.h"

namespace CombatGameplayTags
{
	// State
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Attacking,       "State.Combat.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Countering,      "State.Combat.Countering");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Dodging,         "State.Combat.Dodging");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Stunned,         "State.Combat.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Dead,            "State.Combat.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Blocking,        "State.Combat.Blocking");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Running,       "State.Movement.Running");
	UE_DEFINE_GAMEPLAY_TAG(State_Movement_Boosting,      "State.Movement.Boosting");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_InHand,          "State.Weapon.InHand");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_Aiming,          "State.Weapon.Aiming");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_InFlight,        "State.Weapon.InFlight");
	UE_DEFINE_GAMEPLAY_TAG(State_Weapon_Returning,       "State.Weapon.Returning");
	UE_DEFINE_GAMEPLAY_TAG(State_Warp_Locked,            "State.Warp.Locked");
	UE_DEFINE_GAMEPLAY_TAG(State_Warp_Warping,           "State.Warp.Warping");
	UE_DEFINE_GAMEPLAY_TAG(State_Projectile_Charging,    "State.Projectile.Charging");

	// Ability
	UE_DEFINE_GAMEPLAY_TAG(Ability_Melee_Light,          "Ability.Melee.Light");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Melee_Heavy,          "Ability.Melee.Heavy");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Counter,              "Ability.Counter");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dodge,                "Ability.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_WarpStrike,           "Ability.WarpStrike");
	UE_DEFINE_GAMEPLAY_TAG(Ability_WeaponThrow,          "Ability.WeaponThrow");
	UE_DEFINE_GAMEPLAY_TAG(Ability_WeaponRecall,         "Ability.WeaponRecall");
	UE_DEFINE_GAMEPLAY_TAG(Ability_ProjectileShot,       "Ability.ProjectileShot");

	// Event
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Hit,             "Event.Combat.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Kill,            "Event.Combat.Kill");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Counter,         "Event.Combat.Counter");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_LastHit,         "Event.Combat.LastHit");
	UE_DEFINE_GAMEPLAY_TAG(Event_Weapon_Thrown,           "Event.Weapon.Thrown");
	UE_DEFINE_GAMEPLAY_TAG(Event_Weapon_Caught,          "Event.Weapon.Caught");
	UE_DEFINE_GAMEPLAY_TAG(Event_Warp_Started,           "Event.Warp.Started");
	UE_DEFINE_GAMEPLAY_TAG(Event_Warp_Landed,            "Event.Warp.Landed");
	UE_DEFINE_GAMEPLAY_TAG(Event_Projectile_Hit,         "Event.Projectile.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Event_Boost_Activated,        "Event.Boost.Activated");

	// Effect
	UE_DEFINE_GAMEPLAY_TAG(Effect_Damage,                "Effect.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Stun,                  "Effect.Stun");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Knockback,             "Effect.Knockback");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Boost,                 "Effect.Boost");

	// Invincibility / Hit React
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_Invincible,      "State.Combat.Invincible");
	UE_DEFINE_GAMEPLAY_TAG(State_Combat_HitReact,        "State.Combat.HitReact");

	// GameplayCue
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Combat_HitImpact,     "GameplayCue.Combat.HitImpact");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Combat_WeaponTrail,   "GameplayCue.Combat.WeaponTrail");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Combat_DodgeTrail,    "GameplayCue.Combat.DodgeTrail");

	// AI
	UE_DEFINE_GAMEPLAY_TAG(AI_State_Idle,                "AI.State.Idle");
	UE_DEFINE_GAMEPLAY_TAG(AI_State_Strafing,            "AI.State.Strafing");
	UE_DEFINE_GAMEPLAY_TAG(AI_State_Approaching,         "AI.State.Approaching");
	UE_DEFINE_GAMEPLAY_TAG(AI_State_Attacking,           "AI.State.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(AI_State_Retreating,          "AI.State.Retreating");
	UE_DEFINE_GAMEPLAY_TAG(AI_State_PreparingAttack,     "AI.State.PreparingAttack");
}
