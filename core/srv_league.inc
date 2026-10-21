// ---- part 2 (OfflineServer.java ~800-1440): history item, leaderboards, request decoders, careerplan ----

// Native development budget: each earned level contributes two points.
// Do not add a flat bonus; it overdevelops fixed POTW cards and makes Auto
// allocate substantially more points than the original game permits.
int ziruProgressionBudget(int limit) { return std::max(0, limit - 1) * 2; }

ObjP buildZiruLeagueHistoryItem(int64_t gameId, int32_t s0, int32_t s1) {
  try {
    ObjP tpl = loadZiruJsonAssetObject("responses/ZIRU_GET_GAMERESULT_LIST.json");
    ArrP l;
    ObjP f;
    if (!tpl || !(l = tpl->optJSONArray("game_result_list")) || !(f = l->optJSONObject(0))) return nullptr;
    ObjP jo = Object::parse(f->toString());
    jo->put("game_id", (long long)gameId);
    jo->put("date", (long long)(zc::currentTimeMillis() / 1000));
    jo->put("is_com_match", "YES");
    jo->put("match_category", "LEAGUE_COM_CUSTOM");
    std::string reason = "NORMAL";
    JStr m2 = loadMeta("ziru_league_last_gameend_reason");
    if (m2 && !m2->empty()) reason = *m2;
    jo->put("gameend_reason", reason);
    ObjP opp;
    JStr m = loadMeta("ziru_league_current_opponent_json");
    if (m && !m->empty()) opp = Object::parse(*m);
    ArrP pl = jo->optJSONArray("player");
    if (pl && pl->length() >= 2) {
      ObjP p0 = pl->optJSONObject(0);
      if (p0) p0->put("is_com", "NO");
      ObjP p1 = pl->optJSONObject(1);
      if (p1) {
        p1->put("is_com", "YES");
        if (opp) {
          p1->put("cpu_level", opp->optString("cpu_level", "EASY"));
          ObjP ui = p1->optJSONObject("user_info");
          if (ui) {
            ui->put("user_id", (long long)opp->optLong("user_id", 0));
            ui->put("user_name", opp->optString("user_name", "Division CPU"));
          }
        }
      }
    }
    if (opp) {
      int bt = opp->optInt("base_team_id", 121);
      ArrP team = jo->optJSONArray("team");
      if (team) {
        team->put(1, bt);
        ArrP em = jo->optJSONArray("emblem_team");
        if (em) em->put(1, bt);
      }
      std::string tn = opp->optString("team_name", "AC Milan");
      ArrP a5 = jo->optJSONArray("team_name");
      if (a5) a5->put(1, tn);
      std::string ts = opp->optString("team_name_short", "MIL");
      ArrP a6 = jo->optJSONArray("team_name_short");
      if (a6) a6->put(1, ts);
    }
    ArrP sc = jo->optJSONArray("score");
    if (sc && sc->length() >= 2) {
      ObjP a = sc->optJSONObject(0);
      if (a) { a->put("score_1st", s0); a->put("score_2nd", 0); a->put("score_ex1st", 0); a->put("score_ex2nd", 0); a->put("score_pk", 0); }
      ObjP b = sc->optJSONObject(1);
      if (b) { b->put("score_1st", s1); b->put("score_2nd", 0); b->put("score_ex1st", 0); b->put("score_ex2nd", 0); b->put("score_pk", 0); }
    }
    return jo;
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V75_HISTORY_ITEM_ERR] ") + e.what());
    return nullptr;
  }
}

ArrP buildZiruMlLeaderboardV85(const std::string& key) {
  ArrP out = newArr();
  try {
    JStr m = loadMeta(key);
    if (m) {
      ObjP jo = Object::parse(*m);
      if (jo->length() > 0) {  // names() returns null for empty objects
        ArrP list = newArr();
        for (auto& n : jo->names()) {
          ObjP o = jo->optJSONObject(n);
          if (o && o->optInt("value") > 0) list->put(o);
        }
        sortZiruMlLeaderboardV85(list);
        int len = list->length();
        if (len > 20) len = 20;
        for (int i = 0; i < len; i++) {
          ObjP o = list->optJSONObject(i);
          if (o) { o->put("rank", i + 1); out->put(o); }
        }
      }
    }
  } catch (const std::exception&) {
  }
  return out;
}

ObjP buildZiruResetPlayer(int64_t playerId, int64_t serial) {
  try {
    ObjP w = getZiruPlayerWrapper(Value((long long)playerId), false);
    ObjP base;
    if (!w || !(base = w->optJSONObject("gameplayer"))) return nullptr;
    JStr lp = loadPlayer(serial);
    ObjP jo;
    if (lp && !lp->empty()) jo = Object::parse(*lp);
    if (!jo) jo = Object::parse(base->toString());
    ArrP bp = base->optJSONArray("parameters");
    if (bp) {
      ArrP np = Array::parse(bp->toString());
      ArrP cur = jo->optJSONArray("parameters");
      if (cur) {
        int l1 = np->length(), l2 = cur->length();
        if (l1 >= 7 && l2 >= 7) {
          int a = l1 - 7, b = l2 - 7;
          for (int k = 0; k < 3; k++) np->put(a + k, cur->optInt(b + k));
        }
      }
      jo->put("parameters", np);
    }
    ObjP bl = base->optJSONObject("level_info");
    if (bl) {
      ObjP li = Object::parse(bl->toString());
      int lim = li->optInt("level_limit", 1);
      li->put("level", lim);
      li->put("customize_point", ziruUnspentNativePoints(base));
      li->put("exp_current_level", 0);
      li->put("exp_to_next_level", 0);
      jo->put("level_info", li);
    }
    ObjP gid = jo->optJSONObject("gameplayer_id");
    if (gid) gid->put("serial", (long long)serial);
    return jo;
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V25_RESET_BUILD_ERR] ") + e.what());
    return nullptr;
  }
}

void clearZiruMyLeagueCapturedLeaderboardsV82(const ObjP& jo) {
  if (!jo) return;
  jo->put("goal_ranking", newArr());
  jo->put("assist_ranking", newArr());
}

void decodeAndPersistZiruSquadRequest(const Bytes&) {}

bool inflateRequest(const Bytes& b, Bytes& out) {
  // Arrays.copyOfRange + Cipher(AES/CBC/PKCS5) + InflaterInputStream; false = exception
  return zc::decodeRequestToMsgpack(b, out);
}

void decodeZiruAgentRequest(const Bytes& b) {
  ziruLastAgentId = 0;
  ziruLastCount = 0;
  try {
    if (b.size() > 16) {
      Bytes mp;
      if (!inflateRequest(b, mp)) throw std::runtime_error("decrypt/inflate failed");
      logRequest("ZIRU_V12_INFLATE", "[ZIRU_V12_INFLATE_OK] bytes=" + str((int64_t)mp.size()));
      zc::Unpacker u(mp.data(), mp.size());
      uint32_t n = u.unpackMapHeader();
      for (uint32_t i = 0; i < n; i++) {
        std::string k = u.unpackString();
        if (k == "agent_id") ziruLastAgentId = u.unpackLong();
        else if (k == "count") ziruLastCount = (int32_t)u.unpackLong();
        else u.skipValue();
      }
      std::string m = "[ZIRU_V10_REQ] agent_id=" + str(ziruLastAgentId) + " count=" + str(ziruLastCount);
      zs::logi("OFFLINE_SERVER", m);
      logRequest("ZIRU_V10_REQ", m);
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V10_REQ_ERR] decode failed ") + e.what());
  }
}

void decodeZiruCareerplanRequest(const Bytes& b) {
  ziruCareerPlayerId = 0;
  ziruCareerSerial = 0;
  ziruCareerRqid = 0;
  ziruCareerOverall = 0;
  ziruCareerSpent = 0;
  ziruCareerProgression = newObj();
  ziruCareerParameters = newArr();
  try {
    ObjP req = decodeZiruRequestObject(&b);
    if (!req) return;
    ziruCareerRqid = req->optLong("rqid");
    ArrP ul = req->optJSONArray("gameplayer_use_list");
    if (!ul) return;
    ObjP u0 = ul->optJSONObject(0);
    if (!u0) return;
    ObjP gid = u0->optJSONObject("gameplayer_id");
    if (!gid) return;
    ziruCareerPlayerId = gid->optLong("id");
    ziruCareerSerial = gid->optLong("serial");
    ArrP params = u0->optJSONArray("parameters");
    if (params) ziruCareerParameters = params;
    ziruCareerOverall = u0->optInt("overall");
    ArrP use = u0->optJSONArray("use_list");
    if (!use) return;
    for (int i = 0; i < use->length(); i++) {
      ObjP x = use->optJSONObject(i);
      if (!x) continue;
      std::string t = x->optString("parameter_type");
      if (t.empty()) continue;
      int p = x->optInt("point");
      if (p < 0) { ziruCareerPlayerId = 0; return; }
      ziruCareerProgression->put(t, p);
      int s = (int)((uint32_t)ziruCareerSpent + (uint32_t)p);
      if (s < 0) { ziruCareerPlayerId = 0; return; }
      ziruCareerSpent = s;
    }
    return;
  } catch (const std::exception&) {
  }
  ziruCareerPlayerId = 0;
}

void decodeZiruMyclubItemRequest(const Bytes& b) {
  ziruItemType = std::string("");
  ziruSkillCategory = std::string("");
  ziruTrainingIndex = -1;
  ziruSelectedPositionId = 0;
  ziruSelectedPosition.reset();
  ziruAbilityId = 0;
  ziruSkillCategoryCode = -1;
  ziruSkillItemEffect = 0;
  ziruSkillItemGrade = 0;
  ziruTargetSerial = 0;
  ziruTargetPlayerId = 0;
  ziruLevelEffect = 0;
  ziruIsAuto = false;
  try {
    ObjP req = decodeZiruRequestObject(&b);
    if (!req) return;

    if (req->has("is_auto_use_item")) {
      std::string s = req->optString("is_auto_use_item", "");
      ziruIsAuto = (s == "YES" || s == "true" || req->optBoolean("is_auto_use_item", false));
    }
    ArrP gids = req->optJSONArray("gameplayer_id_list");
    if (gids && gids->length() > 0) {
      ObjP gid = gids->optJSONObject(0);
      if (gid) {
        ziruTargetPlayerId = gid->optLong("id", 0);
        ziruTargetSerial = gid->optLong("serial", 0);
      }
    }
    ziruTrainingIndex = req->optInt("training_index", -1);
    if (req->has("position_id")) {
      std::string requested = req->optString("position_id", "");
      if (z27GetPositionIndexByName(requested) >= 1) ziruSelectedPosition = requested;
      ziruSelectedPositionId = req->optInt("position_id", 0);
    }
    else if (req->has("position_training_id")) ziruSelectedPositionId = req->optInt("position_training_id", 0);
    if ((!ziruSelectedPosition || ziruSelectedPosition->empty()) && req->has("additional_position"))
      ziruSelectedPosition = req->optString("additional_position", "");
    else if ((!ziruSelectedPosition || ziruSelectedPosition->empty()) && req->has("position"))
      ziruSelectedPosition = req->optString("position", "");
    if (req->has("ability_id")) ziruAbilityId = req->optInt("ability_id", 0);
    else if (req->has("ability_Id")) ziruAbilityId = req->optInt("ability_Id", 0);
    else if (req->has("skill_id")) ziruAbilityId = req->optInt("skill_id", 0);
    else if (req->has("additional_skill_id")) ziruAbilityId = req->optInt("additional_skill_id", 0);

    if (req->has("skill_training_category_type")) {
      ziruSkillCategory = req->optString("skill_training_category_type", "");
      ziruSkillCategoryCode = req->optInt("skill_training_category_type", -1);
    }

    ArrP itemList = req->optJSONArray("item_list");
    if (itemList) {
      for (int i = 0; i < itemList->length(); i++) {
        ObjP it = itemList->optJSONObject(i);
        if (!it) continue;
        int i5 = 0;
        ObjP item = it->optJSONObject("item");
        if (item) {
          if (item->has("item_type")) ziruItemType = item->optString("item_type", "");
          if (item->has("effect")) { i5 = item->optInt("effect", 0); ziruSkillItemEffect = i5; }
          if (item->has("grade")) ziruSkillItemGrade = item->optInt("grade", 0);
          if (item->has("ability_id")) ziruAbilityId = item->optInt("ability_id", 0);
          else if (item->has("ability_Id")) ziruAbilityId = item->optInt("ability_Id", 0);
          else if (item->has("skill_id")) ziruAbilityId = item->optInt("skill_id", 0);
          else if (item->has("additional_skill_id")) ziruAbilityId = item->optInt("additional_skill_id", 0);
          // The selected position is sent at the request root on iOS. Item
          // metadata can also contain a generic position_id (usually GK=1),
          // so it must never overwrite an explicit user selection.
          if (ziruSelectedPositionId == 0 && (!ziruSelectedPosition || ziruSelectedPosition->empty())) {
            if (item->has("position_id")) ziruSelectedPositionId = item->optInt("position_id", 0);
            else if (item->has("position_training_id")) ziruSelectedPositionId = item->optInt("position_training_id", 0);
            if (item->has("additional_position")) ziruSelectedPosition = item->optString("additional_position", "");
            else if (item->has("position")) ziruSelectedPosition = item->optString("position", "");
          }
        }
        int i6 = it->optInt("present_num", 0);
        if (it->has("ability_id")) ziruAbilityId = it->optInt("ability_id", 0);
        else if (it->has("ability_Id")) ziruAbilityId = it->optInt("ability_Id", 0);
        else if (it->has("skill_id")) ziruAbilityId = it->optInt("skill_id", 0);
        else if (it->has("additional_skill_id")) ziruAbilityId = it->optInt("additional_skill_id", 0);
        if (ziruSelectedPositionId == 0 && (!ziruSelectedPosition || ziruSelectedPosition->empty())) {
          if (it->has("position_id")) ziruSelectedPositionId = it->optInt("position_id", 0);
          else if (it->has("position_training_id")) ziruSelectedPositionId = it->optInt("position_training_id", 0);
          if (it->has("additional_position")) ziruSelectedPosition = it->optString("additional_position", "");
          else if (it->has("position")) ziruSelectedPosition = it->optString("position", "");
        }

        if (ziruItemType && *ziruItemType == "LEVEL_TRAINING") {
          ziruLevelEffect += (int32_t)((uint32_t)i5 * (uint32_t)i6);
        }
      }
      if (ziruAbilityId == 0 && ziruSkillItemEffect > 0) ziruAbilityId = ziruSkillItemEffect;
    }
    zs::logi("OFFLINE_SERVER", "[ZIRU V15.7.2A ITEM] type=" + (ziruItemType ? *ziruItemType : "none") +
             " serial=" + str(ziruTargetSerial) + " player_id=" + str(ziruTargetPlayerId) +
             " training_index=" + str(ziruTrainingIndex) + " ability=" + str(ziruAbilityId) +
             " position_id=" + str(ziruSelectedPositionId) +
             " position=" + ziruSelectedPosition.value_or(""));
    if (ziruItemType && *ziruItemType == "POSITION_TRAINING_SELECT")
      logRequest("ZIRU_POSITION_REQUEST", "[ZIRU_POSITION_REQUEST] " + req->toString());
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU V15.7.2A ITEM] decode failed ") + e.what());
    Bytes mp;
    if (inflateRequest(b, mp)) {
      const std::string raw((const char*)mp.data(), mp.size());
      if (raw.find("POSITION_TRAINING_SELECT") != std::string::npos)
        ziruItemType = std::string("POSITION_TRAINING_SELECT");
      // Needed to identify the actual iOS select-position payload. The old
      // decoder silently discarded it and the UI displayed GK by default.
      logRequest("ZIRU_POSITION_DECODE", "[ZIRU_POSITION_DECODE] bytes=" +
          str((int64_t)mp.size()) + " hex=" + zc::hex(mp.data(), std::min<size_t>(mp.size(), 2048)));
    }
  }
}

void decodeZiruPlayerDataRequest(const Bytes& b) {
  ziruPlayerDataId = 0;
  ziruPlayerDataSerial = 0;
  ziruPlayerDataRqid = 0;
  try {
    if (b.size() > 16) {
      Bytes mp;
      if (!inflateRequest(b, mp)) throw std::runtime_error("decrypt/inflate failed");
      logRequest("ZIRU_V11_PLAYERDATA", "[ZIRU_V11_PD_INFLATE] bytes=" + str((int64_t)mp.size()));
      zc::Unpacker u(mp.data(), mp.size());
      uint32_t n = u.unpackMapHeader();
      for (uint32_t i = 0; i < n; i++) {
        std::string k = u.unpackString();
        if (k == "rqid") ziruPlayerDataRqid = u.unpackLong();
        else if (k == "gameplayer_id") {
          uint32_t m = u.unpackMapHeader();
          for (uint32_t i2 = 0; i2 < m; i2++) {
            std::string k2 = u.unpackString();
            if (k2 == "id") ziruPlayerDataId = u.unpackLong();
            else if (k2 == "serial") ziruPlayerDataSerial = u.unpackLong();
            else u.skipValue();
          }
        } else u.skipValue();
      }
      logRequest("ZIRU_V11_PLAYERDATA", "[ZIRU_V11_PD_PARSED] id=" + str(ziruPlayerDataId) + " serial=" + str(ziruPlayerDataSerial));
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V11_PD_ERR] streaming decode failed ") + e.what());
    logRequest("ZIRU_V11_PLAYERDATA", "[ZIRU_V11_PD_ERR] streaming decode failed");
  }
}

ObjP decodeZiruRequestJson(const Bytes* b) {
  if (!b || b->size() <= 16) return nullptr;
  try {
    Bytes mp;
    if (!inflateRequest(*b, mp)) throw std::runtime_error("decrypt/inflate failed");
    size_t pos = 0;
    Value v = zc::unpackZiruValue(mp.data(), mp.size(), pos);
    if (v.t == Value::OBJ) return v.o;
    return nullptr;
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V46_DECODE_ERR] ") + e.what());
    return nullptr;
  }
}
ObjP decodeZiruRequestJson(const std::optional<Bytes>& b) { return decodeZiruRequestJson(b ? &*b : nullptr); }

ObjP decodeZiruRequestObject(const Bytes* b) {
  try {
    if (!b) throw NPE();
    if (b->size() <= 16) return nullptr;
    Bytes mp;
    if (!inflateRequest(*b, mp)) throw std::runtime_error("decrypt/inflate failed");
    size_t pos = 0;
    return Object::parse(zc::unpackValueToJson(mp.data(), mp.size(), pos));
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V9_PLAYERDATA_ERR] request decode failed ") + e.what());
    return nullptr;
  }
}
ObjP decodeZiruRequestObject(const std::optional<Bytes>& b) { return decodeZiruRequestObject(b ? &*b : nullptr); }

void filterZiruSoldPlayersFromLogin(const ObjP& jo) {
  try {
    ObjP ei = jo->optJSONObject("entry_info");
    ArrP list;
    if (!ei || !(list = ei->optJSONArray("gameplayer_list"))) return;
    for (int k = list->length() - 1; k >= 0; k--) {
      ObjP o = list->optJSONObject(k);
      if (!o) continue;
      ObjP gp = o->optJSONObject("gameplayer");
      if (gp) o = gp;
      ObjP gid = o->optJSONObject("gameplayer_id");
      if (gid) {
        int64_t s = gid->optLong("serial", 0);
        if (s > 0 && isZiruPlayerSold(s)) {
          list->remove(k);
          logRequest("ZIRU_RELEASE_V35", "[ZIRU_RELEASE_V35_FILTERED] serial=" + str(s));
        }
      }
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_RELEASE_V35_FILTER_ERR] ") + e.what());
  }
}

ObjP getZiruPlayerWrapper(const Value& obj, bool warehouse = false) {
  try {
    std::string key = str(obj);
    ObjP recs = ziruPlayerRecordsCache;
    if (!recs) {
      recs = Object::parse(readAsset("responses/ZIRU_PLAYER_RECORDS.json"));
      ziruPlayerRecordsCache = recs;
      zs::logi("OFFLINE_SERVER", "[ZIRU_V41_CACHE_READY] ZIRU_PLAYER_RECORDS loaded once");
    }
    ObjP rec = recs->optJSONObject(key);
    if (!rec) return nullptr;
    ObjP jo = Object::parse(rec->toString());
    if (warehouse) { jo->remove("is_dummy"); jo->put("is_warehouse", "NO"); }
    else { jo->remove("is_warehouse"); jo->put("is_dummy", "NO"); }
    ObjP gp, li;
    int lim;
    if (warehouse && (gp = jo->optJSONObject("gameplayer")) && (li = gp->optJSONObject("level_info")) && (lim = li->optInt("level_limit", 1)) > 0) {
      li->put("level", lim);
      li->put("exp_current_level", 0);
      li->put("exp_to_next_level", 0);
      li->put("customize_point", ziruUnspentNativePoints(gp));
    }
    z27ApplyAppearanceAndClub(jo->optJSONObject("gameplayer"));
    return jo;
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V12_RECORD_ERR] failed ") + e.what());
    return nullptr;
  }
}

ObjP getZiruPlayerWrapperByBaseId(int32_t baseId, bool warehouse = false) {
  try {
    static std::unordered_map<int32_t, std::string> ziruBaseIdToRecordKey;
    ObjP recs = ziruPlayerRecordsCache;
    if (!recs) {
      recs = Object::parse(readAsset("responses/ZIRU_PLAYER_RECORDS.json"));
      ziruPlayerRecordsCache = recs;
      zs::logi("OFFLINE_SERVER", "[ZIRU_V41_CACHE_READY] ZIRU_PLAYER_RECORDS loaded once");
    }
    if (ziruBaseIdToRecordKey.empty() && recs) {
      for (const auto& k : recs->keys()) {
        try {
          int64_t fullId = parseLong(k);
          int32_t bid = (int32_t)(fullId & 0xFFFFFF);
          if (bid > 0 && ziruBaseIdToRecordKey.find(bid) == ziruBaseIdToRecordKey.end()) {
            ziruBaseIdToRecordKey[bid] = k;
          }
        } catch (...) {}
      }
    }
    auto it = ziruBaseIdToRecordKey.find(baseId);
    if (it != ziruBaseIdToRecordKey.end()) {
      return getZiruPlayerWrapper(Value(it->second), warehouse);
    }
    return nullptr;
  } catch (...) {
    return nullptr;
  }
}

inline int z27GetDefensiveStyle(int pos, int st1) {
  if (pos == 1 || (st1 >= 16 && st1 <= 17)) return 33; // Sweeper GK (33)
  if (pos == 2) return 30; // CB -> Line Controller (30)
  if (pos == 3 || pos == 4) return 28; // LB/RB -> Lane Blocker (28)
  if (pos == 5 || pos == 6) return 25; // DMF/CMF -> Cover Shadow (25)
  if (pos == 7 || pos == 8) return 27; // LMF/RMF -> Hard Worker (27)
  if (pos == 9) return 23; // AMF -> Press Back (23)
  if (pos == 10 || pos == 11) return 23; // LWG/RWG -> Press Back (23)
  if (pos == 12 || pos == 13) return 26; // SS/CF -> Outlet Forward (26)
  if (st1 == 1 || st1 == 2 || st1 == 3 || st1 == 13 || st1 == 22) return 26; // Forwards -> Outlet Forward
  if (st1 == 4 || st1 == 6 || st1 == 10 || st1 == 14 || st1 == 18 || st1 == 19) return 23; // Attackers/Wingers -> Press Back
  if (st1 == 7 || st1 == 8 || st1 == 9 || st1 == 20) return 25; // Midfielders -> Cover Shadow
  if (st1 == 11 || st1 == 12 || st1 == 15 || st1 == 21) return 28; // Defenders -> Lane Blocker
  return 27; // Hard Worker
}

ArrP z27ResolvePlayStyle(int64_t id, const ObjP& existing = nullptr) {
  int pos = 0;
  if (existing) {
    ArrP par = existing->optJSONArray("parameters");
    if (par && par->length() > 0) pos = par->optInt(0, 0);
  }
  auto ensureTwoStyles = [&](ArrP ps) -> ArrP {
    if (!ps || ps->length() == 0 || ps->optInt(0, 0) <= 0) return nullptr;
    int st1 = ps->optInt(0, 0);
    int st2 = ps->length() > 1 ? ps->optInt(1, 0) : 0;
    if (st2 <= 0) st2 = z27GetDefensiveStyle(pos, st1);
    ArrP res = newArr();
    res->put(st1);
    res->put(st2);
    return res;
  };
  if (existing) {
    ArrP ps = ensureTwoStyles(existing->optJSONArray("play_style"));
    if (ps) return ps;
  }
  // 1. Try getZiruPlayerWrapper by exact ID
  ObjP w = getZiruPlayerWrapper(Value((long long)id), false);
  if (w && w->optJSONObject("gameplayer")) {
    ArrP ps = ensureTwoStyles(w->getJSONObject("gameplayer")->optJSONArray("play_style"));
    if (ps) return ps;
  }
  // 2. Try getZiruPlayerWrapper by base ID (lower 24 bits)
  int32_t bid = (int32_t)(id & 0xFFFFFF);
  ObjP wBase = getZiruPlayerWrapperByBaseId(bid);
  if (wBase && wBase->optJSONObject("gameplayer")) {
    ArrP ps = ensureTwoStyles(wBase->getJSONObject("gameplayer")->optJSONArray("play_style"));
    if (ps) return ps;
  }
  // 3. Try CmdLogin.json
  try {
    std::string s = readAsset("responses/CmdLogin.json");
    if (!s.empty()) {
      ObjP root = Object::parse(s);
      ObjP entry = root ? root->optJSONObject("entry_info") : nullptr;
      ArrP list = entry ? entry->optJSONArray("gameplayer_list") : nullptr;
      if (list) {
        for (int i = 0; i < list->length(); i++) {
          ObjP o = list->optJSONObject(i);
          if (!o) continue;
          ObjP gid = o->optJSONObject("gameplayer_id");
          if (!gid) continue;
          int64_t oid = gid->optLong("id", 0);
          if (oid == id || ((oid & 0xFFFFFF) == bid)) {
            ArrP ps = ensureTwoStyles(o->optJSONArray("play_style"));
            if (ps) return ps;
          }
        }
      }
    }
  } catch (...) {}
  // 4. Semantic default from position parameters so NO player is ever "Basic"
  ArrP fallback = newArr();
  if (pos == 1) { fallback->put(20); fallback->put(33); } // GK -> Offensive Goalkeeper + Sweeper GK
  else if (pos == 2 || pos == 3 || pos == 4) { fallback->put(15); fallback->put(30); } // CB/LB/RB -> Build Up + Line Controller
  else if (pos == 5 || pos == 6 || pos == 7 || pos == 8) { fallback->put(11); fallback->put(27); } // DMF/CMF/LMF/RMF -> Box-to-Box + Hard Worker
  else if (pos == 9) { fallback->put(10); fallback->put(23); } // AMF -> Hole Player + Press Back
  else if (pos == 10 || pos == 11) { fallback->put(6); fallback->put(23); } // LWG/RWG -> Prolific Winger + Press Back
  else { fallback->put(1); fallback->put(26); } // CF/SS -> Goal Poacher + Outlet Forward
  return fallback;
}

Response handleZiruAdjustParamMinimal() {
  logRequest("ZIRU_V81_PROTOCOL", "[ZIRU_V81_ADJUST_ACK] exact rqid");
  return handleZiruProtocolAckV81(JStr("CMD_SEND_ADJUST_PARAM"));
}

static Bytes readBody(Request& r, int n, int& got) {
  Bytes b(n);
  got = 0;
  while (got < n) {
    size_t k = r.read(b.data() + got, n - got);
    if (k == 0) break;
    got += (int)k;
  }
  return b;
}

Response handleZiruCareerplan(Request& r) {
  try {
    auto it = r.headers.find("content-length");
    if (it != r.headers.end()) {
      int n = parseInt(it->second);
      if (n > 0) {
        int got;
        Bytes body = readBody(r, n, got);
        if (got == n) {
          decodeZiruCareerplanRequest(body);
          if (ziruCareerPlayerId > 0) {
            zs::Prefs& prefs = sp("almanac_state");
            std::string s = prefs.getString("recruited_players", "");
            ArrP arr = !s.empty() ? Array::parse(s) : newArr();
            int i2 = 0;
            ObjP found;
            while (true) {
              found = nullptr;
              if (i2 >= arr->length()) break;
              ObjP o = arr->optJSONObject(i2);
              if (o) {
                found = o->optJSONObject("gameplayer");
                if (found) {
                  ObjP gid = found->optJSONObject("gameplayer_id");
                  if (gid && gid->optLong("id", 0) == ziruCareerPlayerId && gid->optLong("serial", 0) == ziruCareerSerial) break;
                }
              }
              i2++;
            }
            ObjP jo = found;
            bool fail = false;
            if (!jo) {
              ObjP w = getZiruPlayerWrapper(Value((long long)ziruCareerPlayerId), true);
              ObjP gp;
              if (!w || !(gp = w->optJSONObject("gameplayer"))) fail = true;
              else {
                ObjP gid = gp->optJSONObject("gameplayer_id");
                if (gid) gid->put("serial", (long long)ziruCareerSerial);
                jo = gp;
              }
            }
            ObjP res;
            if (!fail) {
              JStr lp = loadPlayer(ziruCareerSerial);
              if (lp && !lp->empty()) jo = Object::parse(*lp);
              if (ziruCareerSpent == 0) {
                res = buildZiruResetPlayer(ziruCareerPlayerId, ziruCareerSerial);
                if (!res) fail = true;
                if (!fail && isZiruMessiCard(ziruCareerPlayerId))
                  ziruDb->saveMeta("messi_budget_" + str(ziruCareerSerial),
                      str(ziruProgressionBudget(36)));
              } else {
                ObjP li = jo->optJSONObject("level_info");
                ObjP nativeWrapper = getZiruPlayerWrapper(Value((long long)ziruCareerPlayerId), false);
                ObjP nativePlayer = nativeWrapper ? nativeWrapper->optJSONObject("gameplayer") : nullptr;
                int budget = nativePlayer ? ziruUnspentNativePoints(nativePlayer) :
                    (li ? ziruProgressionBudget(li->optInt("level_limit", 1)) : 0);
                if (isZiruMessiCard(ziruCareerPlayerId)) budget = ziruMessiBudget(ziruCareerSerial);
                if (!li || ziruCareerSpent > budget) {
                  logRequest("ZIRU_CAREER_REJECT", "id=" + str(ziruCareerPlayerId) +
                      " spent=" + str(ziruCareerSpent) + " budget=" + str(budget));
                  fail = true;
                }
                else {
                  ArrP params = ziruCareerParameters;
                  res = jo;
                  ObjP nativeLevel = nativePlayer ? nativePlayer->optJSONObject("level_info") : nullptr;
                  bool finishedCatalogCard = nativeLevel &&
                      nativeLevel->optInt("level", 1) == nativeLevel->optInt("level_limit", 1) &&
                      nativeLevel->optInt("customize_point", 0) == 0;
                  if (!params || params->length() < 145) {
                    ObjP baseWrapper = getZiruPlayerWrapper(Value((long long)ziruCareerPlayerId), false);
                    ObjP basePlayer = baseWrapper ? baseWrapper->optJSONObject("gameplayer") : nullptr;
                    ArrP bp = basePlayer ? basePlayer->optJSONArray("parameters") : nullptr;
                    if (!bp) bp = jo->optJSONArray("parameters");
                    if (bp) {
                      ArrP np = Array::parse(bp->toString());
                      if (isZiruMessiCard(ziruCareerPlayerId)) {
                        ArrP previous = jo->optJSONArray("parameters");
                        if (previous && previous->length() >= 41)
                          for (int idx = 14; idx <= 40; ++idx) np->put(idx, previous->optInt(idx, 0));
                      } else {
                        z27ApplyProgressionPoints(np, ziruCareerProgression, bp);
                        if (finishedCatalogCard) z27BalanceMaxLevelProgression(np, bp);
                      }
                      z28PreserveCareerTraining(jo, np);
                      jo->put("parameters", np);
                    }
                  } else {
                    ArrP bp = nativePlayer ? nativePlayer->optJSONArray("parameters") : nullptr;
                    if (!isZiruMessiCard(ziruCareerPlayerId) && bp && bp->length() >= 145) {
                      ArrP calculated = Array::parse(bp->toString());
                      z27ApplyProgressionPoints(calculated, ziruCareerProgression, bp);
                      if (finishedCatalogCard) z27BalanceMaxLevelProgression(calculated, bp);
                      for (int idx = 14; idx <= 40; ++idx)
                        params->put(idx, calculated->optInt(idx, bp->optInt(idx, 0)));
                    }
                    z28PreserveCareerTraining(jo, params);
                    jo->put("parameters", params);
                  }
                }
              }
            }
            if (!fail) {
              z27ApplyAppearanceAndClub(res);
              ObjP li = res->optJSONObject("level_info");
              if (li) {
                ObjP nativeWrapper = getZiruPlayerWrapper(Value((long long)ziruCareerPlayerId), false);
                ObjP nativePlayer = nativeWrapper ? nativeWrapper->optJSONObject("gameplayer") : nullptr;
                int budget = nativePlayer ? ziruUnspentNativePoints(nativePlayer) :
                    ziruProgressionBudget(li->optInt("level_limit", 1));
                if (isZiruMessiCard(ziruCareerPlayerId)) budget = ziruMessiBudget(ziruCareerSerial);
                int cp = budget - ziruCareerSpent;
                if (cp < 0) cp = 0;
                li->put("customize_point", cp);
              }
              if (isZiruMessiCard(ziruCareerPlayerId)) {
                applyZiruMessiElite(res, ziruCareerSpent == 0,
                    ziruMessiBudget(ziruCareerSerial) - ziruCareerSpent);
                ziruDb->saveMeta("messi_elite_restored_" + str(ziruCareerSerial), "1");
              }
              ObjP prog = ziruCareerProgression;
              prog->put("_spent_total", ziruCareerSpent);
              ziruDb->saveProgression(ziruCareerSerial, prog->toString());
              ziruDb->savePlayer(ziruCareerSerial, ziruCareerPlayerId, res->toString());
              logRequest("ZIRU_SQLITE", "[ZIRU_SQLITE_PLAYER_SAVED] careerplan");
              if (i2 >= 0 && i2 < arr->length()) {
                nn(arr->optJSONObject(i2))->put("gameplayer", res);
                prefs.edit().putString("recruited_players", arr->toString()).commit();
              }
              ObjP e = newObj();
              e->put("gameplayer", res);
              e->put("careerplan_list", newArr());
              ArrP rl = newArr();
              rl->put(e);
              ObjP out = newObj();
              out->put("careerplan_result_list", rl);
              out->put("achievement_notification", newArr());
              out->put("result", "NOERR");
              out->put("rqid", (long long)ziruCareerRqid);
              out->put("msgid", "CMD_USE_MYCLUB_CAREERPLAN");
              zs::logi("OFFLINE_SERVER", "[ZIRU_CAREER_V25_OK] progression persisted with base/reset visual fix");
              return serveObjectAsEncryptedMsgPack(out);
            }
          }
        }
      }
    }
    zs::logw("OFFLINE_SERVER", "[ZIRU_CAREER_FAIL] falling back to generic");
    return serveJsonAsEncryptedMsgPack("responses/generic.json");
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_CAREER_ERR] handler failed ") + e.what());
    return serveJsonAsEncryptedMsgPack("responses/generic.json");
  }
}

