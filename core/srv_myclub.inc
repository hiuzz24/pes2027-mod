// ---- MyClub: recruit / training / skills (OfflineServer.java ~105-800) ----

static int ziruUnspentNativePoints(const ObjP& player) {
  ObjP li = player ? player->optJSONObject("level_info") : nullptr;
  if (!li) return 0;
  int limit = std::max(1, li->optInt("level_limit", 1));
  int level = std::max(1, std::min(limit, li->optInt("level", 1)));
  int total = std::max(0, limit - 1) * 2;
  int unspent = std::max(0, li->optInt("customize_point", 0));
  return std::min(total, unspent + (limit - level) * 2);
}

void applyZiruBoosterTrainingResponse(const ObjP& r13) {
  try {
    if (!ziruItemType) return;
    std::string r0 = *ziruItemType;
    int32_t r2;
    if (r0 == "ABILITY_TRAINING_RANDOM") {
      r2 = z28RandomBooster();
    } else if (r0 == "ABILITY_TRAINING_SELECT") {
      r2 = ziruAbilityId;
      if (r2 <= 0) { logRequest("ZIRU_BOOSTER_V23", "[ZIRU_BOOSTER_V23_FAIL] missing target/ability"); return; }
    } else {
      return;
    }
    int32_t r3 = ziruTrainingIndex;
    if (!(r3 >= 0 && r3 < 3)) r3 = 1;
    int64_t r4 = ziruTargetSerial;
    if (r4 <= 0) { logRequest("ZIRU_BOOSTER_V23", "[ZIRU_BOOSTER_V23_FAIL] missing target/ability"); return; }
    JStr r7 = loadPlayer(r4);
    ObjP r8;
    if (r7 && !r7->empty()) {
      r8 = Object::parse(*r7);
    } else {
      r8 = z27OwnedPlayer(r4);
      if (!r8) { logRequest("ZIRU_BOOSTER_V23", "[ZIRU_BOOSTER_V23_FAIL] missing target/ability"); return; }
    }
    r8 = ziruSyncActiveBooster(r8);
    ArrP r9 = nn(r8)->optJSONArray("parameters");
    if (r9 && r9->length() >= 145) r9->put(142 + r3, r2);
    ArrP ib = r8->optJSONArray("initial_booster_id_list");
    if (!ib) { ib = newArr()->put(0)->put(0)->put(0); r8->put("initial_booster_id_list", ib); }
    while (ib->length() < 3) ib->put(0);
    ib->put(r3, r2);
    saveZiruPlayerToDb(r8);
    ObjP res = r13->optJSONObject("ability_training_result");
    if (!res) { res = newObj(); r13->put("ability_training_result", res); }
    res->put("ability_Id", r2);
    res->put("ability_id", r2);
    res->put("is_success", "YES");
    logRequest("ZIRU_BOOSTER_V23", "[ZIRU_BOOSTER_V23] type=" + r0 + " serial=" + str(r4) + " slot=" + str(r3) + " ability_id=" + str(r2));
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_BOOSTER_V23_ERR] ") + e.what());
    throw;
  }
}

void applyZiruDynamicRecruit(const ObjP& jo) {
  try {
    int64_t j = ziruLastAgentId;
    if (j <= 0) return;
    int32_t i = ziruLastCount;
    if (i <= 0) i = 1;
    ArrP packs = Object::parse(readAsset("responses/ZIRU_PACKS_CONFIG.json"))->getJSONArray("packs");
    ObjP pack;
    for (int i2 = 0; i2 < packs->length(); i2++) {
      ObjP p = packs->optJSONObject(i2);
      if (p && p->optLong("agent_id") == j) { pack = p; break; }
    }
    if (!pack) { zs::logi("OFFLINE_SERVER", "[ZIRU_V10_DRAW] agent_id not in ZIRU config; leaving template unchanged"); return; }
    ArrP ids = pack->getJSONArray("player_ids");
    zs::Prefs& prefs = sp("almanac_state");
    std::string key = "ziru_remaining_" + str(j);
    std::string s = prefs.getString(key, "");
    ArrP remaining = s.empty() ? Array::parse(ids->toString()) : Array::parse(s);
    int length = remaining->length();
    if (length <= 0) { zs::logw("OFFLINE_SERVER", "[ZIRU_V10_DRAW] pack exhausted"); return; }
    if (i > length) i = length;
    ArrP list = jo->optJSONArray("gameplayer_list");
    ObjP o1, o2, o3;
    if (!list || list->length() <= 0 || !(o1 = list->optJSONObject(0)) || !(o2 = o1->optJSONObject("gameplayer")) ||
        !(o3 = o2->optJSONObject("gameplayer_id")))
      return;
    int64_t j2 = prefs.getLong("ziru_next_serial", o3->optLong("serial"));
    ArrP out = newArr();
    zc::JavaRandom random;
    for (int i3 = 0; i3 < i; i3++) {
      Value removed = remaining->remove(random.nextInt(remaining->length()));
      ObjP w = getZiruPlayerWrapper(removed, true);
      if (!w) {
        w = Object::parse(o1->toString());
        w->getJSONObject("gameplayer")->getJSONObject("gameplayer_id")->put("id", removed);
      }
      j2++;
      w->getJSONObject("gameplayer")->getJSONObject("gameplayer_id")->put("serial", (long long)j2);
      refreshZiruGameplayerFromCanonical(w->getJSONObject("gameplayer"));
      saveZiruPlayerToDb(w->getJSONObject("gameplayer"));
      out->put(w);
    }
    jo->put("gameplayer_list", out);
    prefs.edit().putString(key, remaining->toString()).putLong("ziru_next_serial", j2).commit();
    std::string msg = "[ZIRU_V10_DRAW] count=" + str(i) + " remaining=" + str(remaining->length());
    zs::logi("OFFLINE_SERVER", msg);
    logRequest("ZIRU_V10_DRAW", msg);
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V10_DRAW_ERR] dynamic recruit failed ") + e.what());
    throw;
  }
}

static inline int z27GetPositionIndexByName(const std::string& name) {
  static const char* const names[] = {
    "", "CF", "ST", "LWG", "RWG", "OMF", "DMF", "CMF", "LMF", "RMF", "CB", "LSB", "RSB"
  };
  for (int i = 1; i <= 12; i++) {
    if (name == names[i]) return i;
  }
  if (name == "SS") return 2;
  if (name == "AMF") return 5;
  if (name == "LB") return 11;
  if (name == "RB") return 12;
  return -1;
}

static inline const char* z27PositionNameFromPlayerId(int id) {
  // Player position IDs on the wire differ from the aptitude indices in parameters.
  static const char* const names[] = {
    "", "GK", "CB", "LSB", "RSB", "DMF", "CMF", "LMF", "RMF", "OMF", "LWG", "RWG", "ST", "CF"
  };
  return id >= 1 && id <= 13 ? names[id] : "";
}

void applyZiruPositionTrainingResponse(const ObjP& jo) {
  try {
    if (!(ziruItemType && (*ziruItemType == "POSITION_TRAINING" || *ziruItemType == "POSITION_TRAINING_SELECT"))) return;
    int64_t j = ziruTargetSerial;
    if (j > 0) {
      JStr lp = loadPlayer(j);
      ObjP p;
      if (!lp || lp->empty()) {
        ObjP w = getZiruPlayerWrapper(Value((long long)ziruTargetPlayerId), false);
        if (w && (p = w->optJSONObject("gameplayer"))) {
          ObjP gid = p->optJSONObject("gameplayer_id");
          if (!gid) { gid = newObj(); p->put("gameplayer_id", gid); }
          gid->put("serial", (long long)j);
          gid->put("id", (long long)ziruTargetPlayerId);
        }
      } else {
        p = Object::parse(*lp);
      }
      if (!p) {
        logRequest("ZIRU_POSITION_V26", "[ZIRU_POSITION_V26_FAIL] missing player");
        return;
      }
      ArrP params = p->optJSONArray("parameters");
      if (params && params->length() >= 14) {
        if (params->optInt(0) != 0) {
          static const char* const names[] = {
            "", "CF", "ST", "LWG", "RWG", "OMF", "DMF", "CMF", "LMF", "RMF", "CB", "LSB", "RSB"
          };
          ArrP add = p->optJSONArray("additional_position");
          if (!add) { add = newArr(); p->put("additional_position", add); }

          int slot = ziruTrainingIndex;
          if (slot != 0 && slot != 1) {
            slot = (add->length() == 0) ? 0 : 1;
          }
          if (add->length() == 0) {
            slot = 0;
          }

          ObjP baseWrapper = getZiruPlayerWrapper(Value((long long)ziruTargetPlayerId), false);
          ObjP baseP;
          ArrP baseParams;
          if (baseWrapper && (baseP = baseWrapper->optJSONObject("gameplayer"))) {
            baseParams = baseP->optJSONArray("parameters");
          }

          int other_slot = (slot == 0) ? 1 : 0;
          std::string other_pos = (other_slot < add->length()) ? add->optString(other_slot, "") : "";
          int other_pos_idx = z27GetPositionIndexByName(other_pos);

          if (baseParams && baseParams->length() >= 14) {
            for (int k = 1; k <= 12; k++) {
              params->put(k, baseParams->optInt(k));
            }
          } else {
            std::string old_pos = (slot < add->length()) ? add->optString(slot, "") : "";
            int old_idx = z27GetPositionIndexByName(old_pos);
            if (old_idx >= 1 && old_idx <= 12) {
              params->put(old_idx, 0);
            }
          }

          if (other_pos_idx >= 1 && other_pos_idx <= 12) {
            params->put(other_pos_idx, 2);
          }

          std::vector<int> candidates;
          for (int k = 1; k <= 12; k++) {
            if (k == other_pos_idx) continue;
            if (baseParams && baseParams->length() >= 14 && baseParams->optInt(k) >= 2) continue;
            if ((!baseParams || baseParams->length() < 14) && params->optInt(k) >= 2) continue;
            candidates.push_back(k);
          }

          if (candidates.empty()) {
            for (int k = 1; k <= 12; k++) {
              if (k != other_pos_idx) candidates.push_back(k);
            }
          }

          int chosen_idx = -1;
          if (*ziruItemType == "POSITION_TRAINING_SELECT") {
            std::string requested = ziruSelectedPosition.value_or("");
            if (requested.empty()) requested = z27PositionNameFromPlayerId(ziruSelectedPositionId);
            chosen_idx = z27GetPositionIndexByName(requested);
            if (chosen_idx < 1 || std::find(candidates.begin(), candidates.end(), chosen_idx) == candidates.end()) {
              logRequest("ZIRU_POSITION_V27", "[ZIRU_POSITION_SELECT_INVALID] serial=" + str(j) + " position=" + requested);
              ObjP res = jo->optJSONObject("position_training_result");
              if (!res) { res = newObj(); jo->put("position_training_result", res); }
              res->put("additional_position", "")->put("is_success", "NO");
              return;
            }
          } else {
            chosen_idx = candidates[rand() % candidates.size()];
          }
          std::string chosen_pos = names[chosen_idx];

          params->put(chosen_idx, 2);

          if (slot == 0) {
            if (add->length() == 0) add->put(chosen_pos);
            else add->put(0, chosen_pos);
          } else {
            if (add->length() == 0) {
              add->put(chosen_pos);
            } else if (add->length() == 1) {
              add->put(chosen_pos);
            } else {
              add->put(1, chosen_pos);
            }
          }

          saveZiruPlayerToDb(p);

          ObjP res = jo->optJSONObject("position_training_result");
          if (!res) { res = newObj(); jo->put("position_training_result", res); }
          res->put("additional_position", chosen_pos);
          res->put("is_success", "YES");

          ArrP items = jo->optJSONArray("item_list");
          if (!items) { items = newArr(); jo->put("item_list", items); }
          bool hasSelect = false;
          for (int i8 = 0; i8 < items->length(); i8++) {
            ObjP it = items->optJSONObject(i8);
            ObjP item;
            if (it && (item = it->optJSONObject("item"))) {
              std::string t = item->optString("item_type", "");
              if (t == "POSITION_TRAINING_SELECT") hasSelect = true;
              if (t == "POSITION_TRAINING" || t == "POSITION_TRAINING_SELECT") it->put("inventory_num", 999999);
            }
          }
          if (!hasSelect) items->put(newObj()->put("item", newObj()->put("item_type", "POSITION_TRAINING_SELECT")
              ->put("grade", 0)->put("effect", 0))->put("inventory_num", 999999));

          logRequest("ZIRU_POSITION_V26", "[ZIRU_POSITION_V26] serial=" + str(j) + " slot=" + str(slot) + " position=" + chosen_pos);
          return;
        }

        ObjP res = jo->optJSONObject("position_training_result");
        if (!res) { res = newObj(); jo->put("position_training_result", res); }
        res->put("additional_position", "");
        res->put("is_success", "NO");
        return;
      }
    }
    logRequest("ZIRU_POSITION_V26", "[ZIRU_POSITION_V26_FAIL] missing/invalid player state");
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_POSITION_V26_ERR] ") + e.what());
  }
}

void applyZiruProcurableList(const ObjP& jo) {
  try {
    int32_t i = (int32_t)ziruLastAgentId;
    if (i <= 0) return;
    ArrP packs = Object::parse(readAsset("responses/ZIRU_PACKS_CONFIG.json"))->getJSONArray("packs");
    ObjP pack;
    for (int i2 = 0; i2 < packs->length(); i2++) {
      pack = packs->optJSONObject(i2);
      if (pack && pack->optInt("agent_id") == i) break;
      pack = nullptr;
    }
    if (!pack) return;
    ArrP ids = pack->getJSONArray("player_ids");
    std::string s = sp("almanac_state").getString("ziru_remaining_" + str(i), "");
    if (!s.empty()) ids = Array::parse(s);
    ArrP list = jo->optJSONArray("gameplayer_list");
    ObjP first;
    if (!list || list->length() <= 0 || !(first = list->optJSONObject(0))) return;
    ArrP out = newArr();
    for (int i3 = 0; i3 < ids->length(); i3++) {
      Value opt = ids->opt(i3);
      ObjP w = getZiruPlayerWrapper(opt, false);
      if (w) out->put(w);
      else {
        ObjP c = Object::parse(first->toString());
        c->getJSONObject("gameplayer")->getJSONObject("gameplayer_id")->put("id", opt);
        out->put(c);
      }
    }
    jo->put("gameplayer_list", out);
    jo->put("acquired_player_list", newArr());
    zs::logi("OFFLINE_SERVER", "[ZIRU_V11_PROC] agent_id=" + str(i) + " players=" + str(out->length()));
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V11_PROC_ERR] failed ") + e.what());
  }
}

void applyZiruProgressionToCareerList(const ArrP& ja, int64_t j) {
  try {
    JStr lp = loadProgression(j);
    if (!lp || lp->empty()) return;
    ObjP prog = Object::parse(*lp);
    ObjP first = nn(ja)->optJSONObject(0);
    ArrP dl;
    if (!first || !(dl = first->optJSONArray("data_list"))) return;
    for (int i = 0; i < dl->length(); i++) {
      ObjP d = dl->optJSONObject(i);
      if (d) {
        int c = ziruCountFromSpent(prog->optInt(d->optString("parameter_type", ""), 0));
        d->put("count", c);
        d->put("point", (c / 4) + 1);
      }
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V14_PROGRESSION_LIST_ERR] ") + e.what());
  }
}

void applyZiruRemainingCounts(const ObjP& jo) {
  try {
    ArrP list = jo->optJSONArray("agent_list");
    if (!list) return;
    zs::Prefs& prefs = sp("almanac_state");
    for (int i = 0; i < list->length(); i++) {
      ObjP a = list->optJSONObject(i);
      if (!a) continue;
      int64_t id = a->optLong("agent_id");
      std::string s = prefs.getString("ziru_remaining_" + str(id), "");
      if (!s.empty()) {
        int len = Array::parse(s)->length();
        ArrP c = newArr();
        c->put(0); c->put(0); c->put(len); c->put(0); c->put(0);
        a->put("remaining_player_count", c);
        logRequest("ZIRU_V12_2_REMAINING", "[ZIRU_V12_2_REMAINING] agent_id=" + str(id) + " remaining=" + str(len));
      }
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V12_2_REMAINING_ERR] failed ") + e.what());
  }
}

void applyZiruResetBox(const ObjP& jo) {
  try {
    int64_t j = ziruLastAgentId;
    if (j > 0) {
      sp("almanac_state").edit().remove("ziru_remaining_" + str(j)).commit();
      jo->put("agent_id", (long long)j);
      ziruAttachResetCounts(jo, j);
      logRequest("ZIRU_V14_RESET", "[ZIRU_V14_RESET] agent_id=" + str(j) + " restored");
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V14_RESET_ERR] failed ") + e.what());
  }
}

void applyZiruRonaldoMaxProgression(const ObjP& jo) {
  if (!jo) return;
  try {
    ObjP gid = jo->optJSONObject("gameplayer_id");
    if (!gid) return;
    int64_t pid = gid->optLong("id", 0);
    int32_t base_id = (int32_t)(pid & 0xFFFFFF);
    if (base_id != 4522 && pid != 89138556572074LL) return;
    ArrP p = jo->optJSONArray("parameters");
    if (p && p->length() >= 41) {
      // Prime Cristiano Ronaldo God-Tier Base Stats (Unlocked from Ziru nerf)
      p->put(14, 96); // Offensive Awareness
      p->put(17, 92); // Ball Control
      p->put(18, 92); // Dribbling
      p->put(19, 90); // Tight Possession
      p->put(20, 97); // Finishing
      p->put(23, 95); // Heading
      p->put(27, 88); // Place Kicking
      p->put(28, 86); // Curl
      p->put(34, 99); // Kicking Power
      p->put(35, 96); // Speed
      p->put(36, 93); // Acceleration
      p->put(37, 93); // Physical Contact
      p->put(38, 99); // Jump (Iconic CR7 Leap)
      p->put(40, 92); // Stamina
    }
    ObjP li = jo->optJSONObject("level_info");
    if (li) {
      int lim = std::max(36, li->optInt("level_limit", 36));
      li->put("level_limit", lim);
      li->put("level", lim);
      int cp = ziruProgressionBudget(lim);
      li->put("customize_point", cp);
      li->put("exp_current_level", 0);
      li->put("exp_to_next_level", 0);
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_RONALDO_UNLOCKED_ERR] ") + e.what());
  }
}

static bool isZiruMessiCard(int64_t id) { return (id & 0xFFFFFF) == 7511; }

static void applyZiruMessiMaxProgression(const ObjP& jo, bool preserveEditedStats = false) {
  if (!jo) return;
  try {
    ObjP gid = jo->optJSONObject("gameplayer_id");
    if (!gid || !isZiruMessiCard(gid->optLong("id", 0))) return;
    ArrP p = jo->optJSONArray("parameters");
    if (!preserveEditedStats && p && p->length() >= 41) {
      // Give every Messi variation a fixed, position-appropriate ceiling.
      // Keep a stronger existing value rather than downgrading a saved card.
      const std::pair<int, int> targets[] = {
        {14, 98}, // Offensive Awareness
        {17, 99}, // Ball Control
        {18, 99}, // Dribbling
        {19, 99}, // Tight Possession
        {20, 97}, // Finishing
        {21, 96}, // Low Pass
        {22, 93}, // Lofted Pass
        {27, 99}, // Place Kicking
        {28, 99}, // Curl
        {34, 92}, // Kicking Power
        {35, 94}, // Speed
        {36, 97}, // Acceleration
        {40, 92}, // Stamina
      };
      for (const auto& target : targets)
        p->put(target.first, std::max(p->optInt(target.first, 0), target.second));
    }
    ObjP li = jo->optJSONObject("level_info");
    if (li) {
      int lim = std::max(36, li->optInt("level_limit", 36));
      li->put("level_limit", lim);
      li->put("level", lim);
      // Keep the spare points after edits, while leaving the user's chosen
      // parameters alone whenever a progression allocation exists.
      li->put("customize_point", (lim - 1) * 2);
      li->put("exp_current_level", 0);
      li->put("exp_to_next_level", 0);
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_MESSI_MAX_ERR] ") + e.what());
  }
}

inline const std::unordered_map<int32_t, std::pair<int, int>>& getZiruCanonicalWeakFootMap() {
  static const std::unordered_map<int32_t, std::pair<int, int>> kMap = {
    {52, {3, 3}},
    {812, {3, 2}},    // Ruud Gullit legend base
    {852, {1, 0}},
    {866, {1, 0}},
    {867, {2, 3}},    // Kaka
    {3758, {2, 2}},
    {4073, {1, 1}},
    {4522, {3, 2}},   // Cristiano Ronaldo
    {7444, {1, 0}},
    {7511, {2, 0}},   // Lionel Messi
    {33185, {2, 1}},
    {33702, {2, 2}},
    {34881, {3, 2}},
    {40240, {3, 3}},  // Neymar Jr
    {40323, {2, 1}},
    {40352, {3, 2}},  // Luis Suárez
    {41126, {2, 2}},
    {44379, {3, 2}},  // Kevin De Bruyne
    {45097, {3, 3}},  // Son Heung-Min
    {45993, {1, 1}},
    {57123, {1, 1}},
    {104677, {2, 2}},
    {104717, {1, 1}},
    {108959, {2, 1}},
    {109941, {1, 1}},
    {110718, {2, 2}},  // Ousmane Dembélé
    {113960, {2, 2}},
    {114105, {2, 2}},
    {115881, {2, 0}},
    {116543, {2, 1}},
    {129369, {1, 0}},
    {132933, {3, 2}},
    {133543, {2, 0}},  // Erling Haaland
    {134577, {1, 0}},
    {134589, {2, 1}},
    {134594, {2, 2}},
    {134595, {2, 2}},
    {134604, {1, 0}},
    {134605, {2, 1}},
    {134608, {2, 1}},
    {134634, {3, 2}},  // Ruud Gullit (Epic / Big Time)
    {134635, {3, 3}},
    {134637, {3, 2}},
    {134639, {2, 1}},
    {137962, {3, 2}},
    {137990, {2, 2}},
    {138000, {2, 0}},
    {138392, {1, 1}},
    {139679, {2, 2}},
    {160577, {1, 1}},
    {166176, {3, 3}}
  };
  return kMap;
}

void applyZiruWeakFootFix(const ObjP& jo) {
  if (!jo) return;
  try {
    ObjP gid = jo->optJSONObject("gameplayer_id");
    if (!gid) return;
    int64_t pid = gid->optLong("id", 0);
    int32_t base_id = (int32_t)(pid & 0xFFFFFF);
    ArrP p = jo->optJSONArray("parameters");
    if (!p || p->length() < 46) return;

    if (pid == 88041460993514LL || base_id == 134634 || base_id == 812) {
      p->put(33, 3); // Weak Foot Accuracy: Very High
      p->put(45, 2); // Weak Foot Usage: Frequently
      return;
    }
    if (base_id == 40352) { // Luis Suárez
      p->put(33, 3); // Weak Foot Accuracy: Very High
      p->put(45, 2); // Weak Foot Usage: Frequently
      return;
    }

    const auto& map = getZiruCanonicalWeakFootMap();
    auto it = map.find(base_id);
    if (it != map.end()) {
      p->put(33, it->second.first);
      p->put(45, it->second.second);
      return;
    }

    if (p->optInt(33, 0) <= 0) {
      int finish = p->optInt(20, 50);
      int pass = p->optInt(21, 50);
      int dribble = p->optInt(18, 50);
      int maxAtt = std::max({finish, pass, dribble});
      if (maxAtt >= 85) {
        p->put(33, 3);
      } else if (maxAtt >= 75) {
        p->put(33, 2);
      } else {
        p->put(33, 1);
      }
      if (p->optInt(45, 0) <= 0 && maxAtt >= 75) {
        p->put(45, 1);
      }
    }
  } catch (...) {}
}

static bool vcontains(const std::vector<int32_t>& v, int32_t x) { return std::find(v.begin(), v.end(), x) != v.end(); }

void applyZiruSkillStateToLogin(const ObjP& jo) {
  try {
    ObjP ei = jo->optJSONObject("entry_info");
    ArrP list;
    if (!ei || !(list = ei->optJSONArray("gameplayer_list"))) return;
    zs::Prefs& prefs = sp("almanac_state");
    for (int i = 0; i < list->length(); i++) {
      ObjP p = list->optJSONObject(i);
      ObjP gid;
      if (!p || !(gid = p->optJSONObject("gameplayer_id"))) continue;
      int64_t serial = gid->optLong("serial", 0);
      if (serial == 0) continue;
      int64_t pid = gid->optLong("id", 0);
      std::vector<int32_t> al;
      std::string s = prefs.getString("ziru_skills_" + str(serial), "");
      if (!s.empty()) {
        for (auto& part : split(s, ',')) {
          try {
            int v = parseInt(part);
            if (isZiruSkillId(v) && !vcontains(al, v) && al.size() < 5) al.push_back(v);
          } catch (const NumberFormat&) {
          }
        }
      }
      if (al.empty()) {
        ArrP sk = p->optJSONArray("additional_skill_id");
        if (sk) {
          for (int i2 = 0; i2 < sk->length(); i2++) {
            int v = sk->optInt(i2);
            if (v == 25 || v == 68) v = iosSkillId(v, false);
            if (isZiruSkillId(v) && !vcontains(al, v) && al.size() < 5) al.push_back(v);
          }
        }
      }
      ArrP ja = newArr();
      for (int v : al) ja->put(v);
      p->put("additional_skill_id", ja);

      ArrP params = p->optJSONArray("parameters");
      if (params && params->length() >= 70) {
        ObjP w = getZiruPlayerWrapper(Value((long long)pid), false);
        if (!w) w = getZiruPlayerWrapperByBaseId((int32_t)(pid & 0xFFFFFF));
        ArrP baseParams = (w && w->optJSONObject("gameplayer")) ? w->getJSONObject("gameplayer")->optJSONArray("parameters") : nullptr;
        if (baseParams && baseParams->length() >= 70) {
          for (int idx = 70; idx <= 141; idx++) {
            if (baseParams->optInt(idx, 0) == 1) {
              params->put(idx, 1);
            }
          }
        }
        for (int v : al) {
          if (isZiruSkillId(v)) {
            int pIdx = kZiruSkillParameters[v];
            if (pIdx >= 0 && pIdx < params->length()) {
              params->put(pIdx, 1);
            }
          }
        }
      }
      if (!al.empty()) {
        prefs.edit().putString("ziru_skills_" + str(serial), joinInts(al)).commit();
      }
      JStr saved = loadPlayer(serial);
      if (!saved || *saved != p->toString()) saveZiruPlayerToDb(p);
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_SKILL_V2] login merge/reapply failed ") + e.what());
    throw;
  }
}

static std::string joinInts(const std::vector<int32_t>& v) {
  std::string s;
  for (size_t i = 0; i < v.size(); i++) { if (i) s += ","; s += std::to_string(v[i]); }
  return s;
}

void applyZiruSkillTrainingResponse(const ObjP& jo) {
  try {
    if (!ziruItemType || (*ziruItemType != "SKILL_TRAINING" && *ziruItemType != "SKILL_TRAINING_CATEGORY" && *ziruItemType != "SKILL_TRAINING_SELECT")) return;
    int64_t j = ziruTargetSerial;
    if (j == 0) return;
    ObjP owned = z27OwnedPlayer(j);
    if (!owned) return;

    zs::Prefs& prefs = sp("almanac_state");
    std::string key = "ziru_skills_" + str(j);
    std::string s = prefs.getString(key, "");
    std::vector<int32_t> al;
    ArrP sk = owned->optJSONArray("additional_skill_id");
    if (!sk) {
      JStr lp = loadPlayer(j);
      if (lp && !lp->empty()) sk = Object::parse(*lp)->optJSONArray("additional_skill_id");
    }
    if (sk) {
      for (int i = 0; i < sk->length(); i++) {
        int v = sk->optInt(i);
        if (v == 25 || v == 68) v = iosSkillId(v, false);
        if (isZiruSkillId(v)) {
          if (vcontains(al, v)) continue;
          if (al.size() >= 5) break;
          al.push_back(v);
        }
      }
    }
    if (!s.empty()) {
      for (auto& part : split(s, ',')) {
        try {
          int v = parseInt(part);
          if (isZiruSkillId(v) && !vcontains(al, v) && al.size() < 5) al.push_back(v);
        } catch (const NumberFormat&) {
        }
      }
    }
    if (al.size() >= 5) {
      int i2 = ziruTrainingIndex;
      if (i2 > 0) i2--;
      if (i2 < 0 || i2 >= (int)al.size()) i2 = 0;
      int old = al[i2];
      al.erase(al.begin() + i2);
      logRequest("ZIRU_SKILL_V18", "[ZIRU_SKILL_V18_REPLACE] slot=" + str(i2) + " old=" + str(old));
    }
    std::string pool = "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71";
    if (*ziruItemType == "SKILL_TRAINING_CATEGORY") {
      std::string c = resolveZiruSkillCategory();
      if (c == "UNKNOWN") c = "OTHER";
      if (contains(c, "SHOOT") || contains(c, "FINISH")) pool = "15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30";
      else if (contains(c, "PASS")) pool = "31,32,33,34,35,36,37,38,39,40,41,42,43,46";
      else if (contains(c, "DRIB")) pool = "0,1,2,3,4,5,6,7,8,9,10,11,12,13,14";
      else if (contains(c, "DEF")) pool = "55,56,57,58,59,60,61,62,63,64,65,66";
      else pool = "44,45,47,48,49,50,51,52,53,54,67,68,69,70,71";
    }
    std::vector<std::string> sp_ = split(pool, ',');
    std::vector<int> candidates;
    ArrP baseSkills = owned->optJSONArray("parameters");
    for (const auto& s_item : sp_) {
      int val = parseInt(s_item);
      bool builtIn = isZiruSkillId(val) && baseSkills && baseSkills->optInt(kZiruSkillParameters[val]) != 0;
      if (isZiruSkillId(val) && !builtIn && !vcontains(al, val)) candidates.push_back(val);
    }
    int v = 70;
    if (*ziruItemType == "SKILL_TRAINING_SELECT") {
      int chosen = ziruAbilityId;
      if (chosen == 0 && ziruSkillItemEffect > 0) chosen = ziruSkillItemEffect;
      if (chosen == 25 || chosen == 68) chosen = iosSkillId(chosen, false);
      if (!isZiruSkillId(chosen)) chosen = ziruAbilityId;
      if (isZiruSkillId(chosen)) {
        v = chosen;
      } else if (!candidates.empty()) {
        v = candidates[0];
      }
    } else {
      if (!candidates.empty()) {
        v = candidates[rand() % candidates.size()];
      } else throw IllegalState("No eligible additional skill remains");
    }
    auto itDup = std::find(al.begin(), al.end(), v);
    if (itDup != al.end()) al.erase(itDup);
    al.push_back(v);

    ArrP ja = newArr();
    for (int skillId : al) ja->put(skillId);
    owned->put("additional_skill_id", ja);

    int64_t pid = owned->optJSONObject("gameplayer_id") ? owned->getJSONObject("gameplayer_id")->optLong("id", 0) : 0;
    int32_t bid = (int32_t)(pid & 0xFFFFFF);
    ObjP w = getZiruPlayerWrapper(Value((long long)pid), false);
    if (!w) w = getZiruPlayerWrapperByBaseId(bid);
    ArrP baseParams = (w && w->optJSONObject("gameplayer")) ? w->getJSONObject("gameplayer")->optJSONArray("parameters") : nullptr;
    ArrP params = owned->optJSONArray("parameters");
    if (baseParams && params && baseParams->length() >= 70 && params->length() >= 70) {
      for (int idx = 70; idx <= 141; ++idx) {
        if (baseParams->optInt(idx, 0) == 1) params->put(idx, 1);
      }
    }
    if (params && params->length() >= 142) {
      for (int skillId : al) {
        if (isZiruSkillId(skillId)) {
          int pIdx = kZiruSkillParameters[skillId];
          if (pIdx >= 0 && pIdx < params->length()) {
            params->put(pIdx, 1);
          }
        }
      }
    }
    prefs.edit().putString(key, joinInts(al)).commit();
    saveZiruPlayerToDb(owned);

    logRequest("ZIRU_SKILL_V22", "[ZIRU_SKILL_V22] skill awarded: " + str(v));
    ObjP res = jo->optJSONObject("skill_training_result");
    if (!res) { res = newObj(); jo->put("skill_training_result", res); }
    res->put("additional_skill_id", v);
    res->put("is_success", "YES");
    ArrP items = jo->optJSONArray("item_list");
    if (items) {
      bool hasSel = false;
      for (int i8 = 0; i8 < items->length(); i8++) {
        ObjP it = items->optJSONObject(i8);
        ObjP item;
        if (it && (item = it->optJSONObject("item"))) {
          std::string t = item->optString("item_type", "");
          if (t == "SKILL_TRAINING_SELECT") hasSel = true;
          if (t == "SKILL_TRAINING" || t == "SKILL_TRAINING_CATEGORY" || t == "SKILL_TRAINING_SELECT") it->put("inventory_num", 999999);
        }
      }
      if (!hasSel) {
        ObjP it = newObj();
        it->put("item", newObj()->put("item_type", "SKILL_TRAINING_SELECT")->put("grade", 0)->put("effect", 0));
        it->put("inventory_num", 999999);
        items->put(it);
      }
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_SKILL_V2] response/persistence failed ") + e.what());
    throw;
  }
}

void applyZiruSquadStateToLogin(const ObjP& jo) { z27Restore(jo); }

void auditZiruCriticalAssetsV81() {
  for (const char* n : {"responses/CmdLogin.json", "responses/ZIRU_PLAYER_RECORDS.json", "responses/ZIRU_PACKS_CONFIG.json",
                        "responses/CMD_USE_MYCLUB_ITEM.json", "responses/ZIRU_ML_COMPETITION_REGISTRY.json",
                        "responses/ZIRU_ML_SCHEDULE_113.json", "responses/ZIRU_GET_GAMERESULT_LIST.json",
                        "responses/ZIRU_DIVISION_OPPONENT_POOL.json", "responses/CmdGetMyclubEntryInfo.json"}) {
    if (!assets.exists(n)) zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V81_ASSET_MISSING] ") + n);
  }
  zs::logi("OFFLINE_SERVER", "[ZIRU_V81_BOOT_AUDIT] critical asset scan complete");
}

void auditZiruWorldCupAssetsV86() {
  ObjP a = loadZiruJsonAssetObject("responses/ZIRU_WC_CONFIG.json");
  ObjP b = loadZiruJsonAssetObject("responses/ZIRU_WC_SPECIAL_POOL.json");
  zs::logi("ZIRU_WC", "[ZIRU_WC_V86_FOUNDATION] config=" + str(a ? a->length() : 0) + " pool=" + str(b ? b->length() : 0));
}
