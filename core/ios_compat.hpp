#pragma once
#include "jvalue.hpp"

namespace zsv {
// The Android and iOS Player::GetDataLabel(SkillCard) tables have the same
// ordinal-to-parameter mapping for IDs 0..73 (iOS VA 0x10578bc00). The last
// two iOS parameter indices 142/143 are reserved before the booster slots.
inline constexpr int kZiruSkillParameters[74] = {
  70,107,71,72,73,108,74,75,135,136,102,116,124,130,140,76,121,77,
  120,101,78,103,104,109,127,143,134,79,80,81,114,128,82,110,83,84,
  119,85,86,98,117,118,122,141,87,88,105,89,90,99,100,138,139,126,
  129,91,92,93,106,111,112,113,125,137,115,94,123,132,142,95,131,96,
  97,133
};
inline bool isZiruSkillId(int id) { return id >= 0 && id < 74; }
// The APK 1.3.2 catalog reserves these IDs for unnamed or unverified skills.
inline bool isZiruSelectableSkillId(int id) {
  return isZiruSkillId(id) && id != 9 && id != 14 && id != 25
      && id != 43 && id != 51 && id != 52 && id != 68;
}
// efootxi 1.3.2's verified special-skill draw pool. A manual 100-card
// selection may choose any verified skill; random training draws only here.
inline bool isZiruRandomSpecialSkillId(int id) {
  switch (id) {
    case 11: case 12: case 13: case 16: case 18: case 24: case 26:
    case 30: case 31: case 36: case 40: case 41: case 42: case 53:
    case 54: case 62: case 63: case 64: case 66: case 67: case 70:
    case 73: return true;
    default: return false;
  }
}
// 142/143 overlap Android's booster slots; never set them as saved skill bits.
inline bool isZiruSkillParameter(int index) { return index >= 70 && index < 142; }
inline zj::ArrP ziruSkillList(const zj::ArrP& params) {
  auto result = zj::Array::make();
  if (!params) return result;
  for (int id = 0; id < 74; ++id) {
    int index = kZiruSkillParameters[id];
    if (isZiruSkillParameter(index) && index < params->length()
        && params->optInt(index, 0) == 1) result->put(id);
  }
  return result;
}
inline int iosSkillId(int id, bool outgoing) {
  (void)outgoing;
  return id;
}
// iOS 11.1.1 PlayerBase::SetData accepts 147 parameters. Its indices 142/143
// are skill bits; booster IDs are 144..146 (10 bits each). Ziru 11.0.1 uses
// 142..144 for boosters, including its newer 149-element record templates.
// Keep Android/save/oracle data intact and translate only the iOS wire format.
inline bool adaptIosPlayerData(zj::Value& value, bool outgoing) {
  bool changed = false;
  if (value.t == zj::Value::ARR) {
    for (auto& item : value.a->v) changed |= adaptIosPlayerData(item, outgoing);
  } else if (value.t == zj::Value::OBJ) {
    // Player-detail menus also read the owned-player object from entry_info.
    // Emit the explicit skill list there, not only in START_GAME responses.
    auto playerParams = value.o->optJSONArray("parameters");
    if (outgoing && value.o->optJSONObject("gameplayer_id") && playerParams && playerParams->length() >= 142) {
      value.o->put("skillcard_id_list", ziruSkillList(playerParams));
      changed = true;
    }
    // Verified against dt870 Coach.bin (176-byte records) and native
    // CoachBase::SetWorkFromPesdb at 0x104b93814: ability_id[3] is the
    // 10-bit field at bit 10 of the qword at record+16. Two injected coach
    // templates had this field shifted once too far (12->24, 14->28).
    // Repair on the wire too, because older squad saves retain the templates.
    auto cid = value.o->optJSONObject("coach_id");
    auto abilities = value.o->optJSONArray("ability_id");
    if (outgoing && cid && abilities && abilities->length() == 4) {
      int64_t id = cid->optLong("id", 0);
      if (id == 17609097478250LL && abilities->optInt(3, 0) == 24) { abilities->put(3, 12); changed = true; }
      if (id == 17608560707469LL && abilities->optInt(3, 0) == 28) { abilities->put(3, 14); changed = true; }
    }
    for (const auto& key : value.o->keys()) {
      auto item = value.o->get(key);
      if ((key == "additional_skill_id" || key == "skillcard_id_list") && item.t == zj::Value::ARR) {
        auto skills = zj::Array::make();
        for (int i = 0; i < item.a->length(); ++i) skills->put(iosSkillId(item.a->getInt(i), outgoing));
        value.o->put(key, skills);
        changed = true;
      } else if ((key == "additional_skill_id" || key == "skill_id") && (item.t == zj::Value::INT || item.t == zj::Value::LONG)) {
        value.o->put(key, iosSkillId((int)item.n, outgoing));
        changed = true;
      } else if ((key == "parameters" || key == "original_parameters") && item.t == zj::Value::ARR) {
        auto src = item.a;
        int n = src->length();
        if ((outgoing && (n == 145 || n == 149)) || (!outgoing && n == 147)) {
          auto dst = zj::Array::make();
          for (int i = 0; i < 142; ++i) dst->put(src->get(i));
          if (outgoing) {
            dst->put(0)->put(0);
          }
          for (int i = 0; i < 3; ++i) dst->put(src->get((outgoing ? 142 : 144) + i));
          value.o->put(key, dst);
          changed = true;
        }
      } else {
        changed |= adaptIosPlayerData(item, outgoing);
      }
    }
  }
  return changed;
}
}

