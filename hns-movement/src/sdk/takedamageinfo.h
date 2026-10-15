#pragma once
#include "utils/schema.h"
#include "ehandle.h"

class CBaseEntity;

// Damage description passed to CBaseEntity::TakeDamageOld. Not an entity and not networked, so the fields are read and written
// through the returned references (no NetworkStateChanged).
class CTakeDamageInfo
{
public:
	DECLARE_SCHEMA_CLASS_BASE(CTakeDamageInfo, 0)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hInflictor)
	SCHEMA_FIELD(CHandle<CBaseEntity>, m_hAttacker)
	SCHEMA_FIELD(float, m_flDamage)
	SCHEMA_FIELD(int32, m_bitsDamageType)
};
