// ---- part 3 (OfflineServer.java ~1571-1960): player data, league static, MyLeague entry/info/exchange ----

Response handleZiruGameRelayQualityMinimal() {
  logRequest("ZIRU_V81_PROTOCOL", "[ZIRU_V81_GAMERELAY_ACK] exact rqid");
  return handleZiruProtocolAckV81(JStr("CMD_SET_GAMERELAY_QUALITY"));
}

Response handleZiruGetMyclubPlayerData(const std::optional<Bytes>& b) {
  try {
    logRequest("ZIRU_V11_PLAYERDATA", "[ZIRU_V11_HANDLER_ENTER]");
    if (b) {
      decodeZiruPlayerDataRequest(*b);
      int64_t j = ziruPlayerDataId;
      ObjP w, gp;
      ArrP params;
      if (j > 0) {
        w = getZiruPlayerWrapper(Value((long long)j), false);
        if (!w) w = getZiruPlayerWrapperByBaseId((int32_t)(j & 0xFFFFFF));
      }
      if (w && (gp = w->optJSONObject("gameplayer"))) {
        params = gp->optJSONArray("parameters");
      }
      int64_t j2 = ziruPlayerDataSerial;
      ObjP catalog = gp ? z27Copy(gp) : nullptr;
      if (j2 > 0) {
        JStr lp = loadPlayer(j2);
        if (lp && !lp->empty()) {
          ObjP parsed = Object::parse(*lp);
          ObjP owned = parsed->optJSONObject("gameplayer") ? parsed->getJSONObject("gameplayer") : parsed;
          ObjP gid = owned ? owned->optJSONObject("gameplayer_id") : nullptr;
          int64_t ownedId = gid ? gid->optLong("id", 0) : 0;
          if (ownedId > 0 && (j <= 0 || ownedId == j || (ownedId & 0xFFFFFF) == (j & 0xFFFFFF))) gp = owned;
        }
      }
      if (gp) {
        gp = z27Copy(gp);
        std::string beforeBoosterSync = gp->toString();
        ziruSyncActiveBooster(gp);
        if (j2 > 0 && gp->toString() != beforeBoosterSync) {
          ObjP gid = gp->optJSONObject("gameplayer_id");
          if (gid && gid->optLong("serial", 0) == j2)
            ziruDb->savePlayer(j2, gid->optLong("id", 0), gp->toString());
        }
        params = gp->optJSONArray("parameters");
      }
      if (gp && params) {
        z27ApplyAppearanceAndClub(gp);
        // Restore missing intrinsic flags without replacing owned progression,
        // boosters or learned skills with a catalog copy.
        ArrP base = catalog ? catalog->optJSONArray("parameters") : nullptr;
        ArrP original = gp->optJSONArray("original_parameters");
        for (int index = 70; index < 142 && index < params->length(); ++index) {
          if ((base && base->optInt(index, 0) == 1) || (original && original->optInt(index, 0) == 1)) params->put(index, 1);
        }
        ObjP li = gp->optJSONObject("level_info");
        int lim = li ? li->optInt("level_limit", 1) : 1;
        ArrP career = Array::parse(
            "[{\"type\":\"PARAMETER\",\"effect\":1,\"data_list\":[{\"parameter_type\":\"SHOOT\",\"count\":0,\"parameter_list\":[\"ABILITY_SHOT\",\"ABILITY_PLACE_KICKING\",\"ABILITY_BALL_SPIN_CONTROL\"],\"point\":1},{\"parameter_type\":\"PASS\",\"count\":0,\"parameter_list\":[\"ABILITY_SHORT_PASS\",\"ABILITY_LONG_PASS\"],\"point\":1},{\"parameter_type\":\"DRIBLE\",\"count\":0,\"parameter_list\":[\"ABILITY_DRIBBLE\",\"ABILITY_BALL_CONTROL\",\"ABILITY_BALL_TOUCH\"],\"point\":1},{\"parameter_type\":\"QUICKNESS\",\"count\":0,\"parameter_list\":[\"ABILITY_OFFENSE_DECISION\",\"ABILITY_EXPLOSIVE_POWER\",\"ABILITY_BODY_CONTROL\"],\"point\":1},{\"parameter_type\":\"STRENGTH\",\"count\":0,\"parameter_list\":[\"ABILITY_KICKING_POWER\",\"ABILITY_SPEED\",\"ABILITY_STAMINA\"],\"point\":1},{\"parameter_type\":\"AERIAL\",\"count\":0,\"parameter_list\":[\"ABILITY_HEADING\",\"ABILITY_JUMP\",\"ABILITY_BODY_BALANCE\"],\"point\":1},{\"parameter_type\":\"DEFENCE\",\"count\":0,\"parameter_list\":[\"ABILITY_DEFENSE_DECISION\",\"ABILITY_INTERCEPT\",\"ABILITY_AGGRESSIVENESS\",\"ABILITY_CONSCIOUS_DEFENSE\"],\"point\":1},{\"parameter_type\":\"GK1\",\"count\":0,\"parameter_list\":[\"ABILITY_GK_DECISION\",\"ABILITY_JUMP\"],\"point\":1},{\"parameter_type\":\"GK2\",\"count\":0,\"parameter_list\":[\"ABILITY_CLEARANCES\",\"ABILITY_DEFLECTING\"],\"point\":1},{\"parameter_type\":\"GK3\",\"count\":0,\"parameter_list\":[\"ABILITY_CATCHING\",\"ABILITY_COLLAPSING\"],\"point\":1}]}]");
        applyZiruProgressionToCareerList(career, ziruPlayerDataSerial);
        ArrP expl = newArr();
        ObjP e = newObj();
        e->put("level", lim + 1);
        e->put("experience", 0);
        expl->put(e);
        // skillcard_id_list is the selector's available choices, not the
        // player's already-known skills. The latter live in parameters and
        // additional_skill_id on the owned player.
        ArrP cards = newArr();
        ArrP learned = gp->optJSONArray("additional_skill_id");
        for (int i = 0; i < 74; i++) {
          if (!isZiruSelectableSkillId(i)) continue;
          int pIdx = kZiruSkillParameters[i];
          bool innate = isZiruSkillParameter(pIdx) && pIdx < params->length()
              && params->optInt(pIdx, 0) == 1;
          bool alreadyLearned = false;
          if (learned) for (int slot = 0; slot < learned->length(); ++slot)
            if (learned->optInt(slot, -1) == i) { alreadyLearned = true; break; }
          if (!innate && !alreadyLearned) cards->put(i);
        }
        ObjP boost = Object::parse("{\"use_item_num_list_random\":[0,1],\"use_item_num_list_select\":[0,100]}");
        ObjP out = newObj();
        out->put("careerplan_list", career);
        out->put("experience_list", expl);
        out->put("skillcard_id_list", cards);
        out->put("booster_training_info", boost);
        ArrP initialBoosters = gp->optJSONArray("initial_booster_id_list");
        out->put("initial_booster_id_list", initialBoosters
            ? Array::parse(initialBoosters->toString()) : newArr()->put(0)->put(0)->put(0));
        ArrP additionalPositions = gp->optJSONArray("additional_position");
        out->put("additional_position", additionalPositions
            ? Array::parse(additionalPositions->toString()) : newArr());
        out->put("parameters", params);
        out->put("original_parameters", original ? Array::parse(original->toString()) : Array::parse(params->toString()));
        ArrP addSkills = learned;
        if (addSkills) out->put("additional_skill_id", addSkills);
        out->put("result", "NOERR");
        out->put("rqid", (long long)ziruPlayerDataRqid);
        out->put("msgid", "CMD_GET_MYCLUB_PLAYER_DATA");
        logRequest("ZIRU_V11_PLAYERDATA", "[ZIRU_V11_PLAYERDATA_OK] id=" + str(j) + " level_limit=" + str(lim) + " selectable_skills=" + str(cards->length()) + " extra_skills=" + str(addSkills ? addSkills->length() : 0));
        return serveObjectAsEncryptedMsgPack(out);
      }
    }
    zs::logw("OFFLINE_SERVER", "[ZIRU_V11_PLAYERDATA_FAIL] falling back to generic");
    return serveJsonAsEncryptedMsgPack("responses/generic.json");
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V11_PLAYERDATA_ERR] handler failed ") + e.what());
    return serveJsonAsEncryptedMsgPack("responses/generic.json");
  }
}

Response handleZiruGiveupResultEarly() { return z27Result(); }
Response handleZiruLeagueHistoryDynamic() { return z27HistoryResponse(); }

Response handleZiruLeagueInfo(const std::string& msgid = "CMD_GET_LEAGUE_COM_INFO") {
  try {
    ObjP jo = loadZiruJsonAssetObject("responses/cmd_get_league_com_info.json");
    patchZiruLeagueInfo(jo);
    ObjP req = decodeZiruRequestJson(ziruLastRequestBody);
    if (req) nn(jo)->put("rqid", (long long)req->optLong("rqid", 1));
    jo->put("msgid", msgid);
    if (msgid == "CMD_GET_LEAGUE_INFO") {
      ObjP li = jo->optJSONObject("league_info");
      if (li) {
        li->put("is_com_match", "NO");
        li->put("type", "PVP");
      }
    }
    return serveObjectAsEncryptedMsgPack(jo);
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V46_INFO_ERR] ") + e.what());
    return serveJsonAsEncryptedMsgPack("responses/cmd_get_league_com_info.json");
  }
}

Response handleZiruLeagueOpponentDynamicV75() { return z27SelectOpponent(); }
Response handleZiruLeagueResult() { return z27Result(); }
Response handleZiruLeagueStartGameDynamic() { return z27Start(); }

Response handleZiruLeagueStatic(const std::string& name) {
  try {
    ObjP jo = loadZiruJsonAssetObject(name);
    ObjP req = decodeZiruRequestJson(ziruLastRequestBody);
    if (req) nn(jo)->put("rqid", (long long)req->optLong("rqid", 1));
    patchZiruMyLeagueCommonState(jo);
    return serveObjectAsEncryptedMsgPack(jo);
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V46_STATIC_ERR] ") + e.what());
    return handleZiruProtocolAckV81(std::nullopt);
  }
}

Response handleZiruMyLeagueEnter() {
  ziruMyLeagueSetActive(1);
  if (ziruLeagueLoadInt("ziru_myleague_fixture", 1) <= 0) ziruLeagueSaveInt("ziru_myleague_fixture", 1);
  if (ziruLeagueLoadInt("ziru_myleague_point", 9760) <= 0) ziruLeagueSaveInt("ziru_myleague_point", 9760);
  return handleZiruLeagueStatic("responses/ZIRU_ML_SET_SEASON_ENTER.json");
}

Response handleZiruMyLeagueEnterV71() {
  ziruMyLeagueSetActive(1);
  try {
    ObjP req = decodeZiruRequestJson(ziruLastRequestBody);
    if (req) {
      int team = req->optInt("select_team_id", 0);
      if (team <= 0) team = req->optInt("team_id", 0);
      int compe = req->optInt("select_compe_unit_id", 0);
      if (compe <= 0) compe = req->optInt("league_compe_unit_id", 0);
      if (team > 0) {
        ziruLeagueSaveInt("ziru_myleague_season_counter", ziruLeagueLoadInt("ziru_myleague_season_counter", 0) + 1);
        ziruLeagueSaveInt("ziru_myleague_apk_score_start_fixture", 1);
        if (compe <= 0) compe = ziruMyLeagueInferCompetitionV80(team);
        if (compe <= 0) compe = 113;
        ziruLeagueSaveInt("ziru_myleague_team_id", team);
        ziruLeagueSaveInt("ziru_myleague_compe_unit_id", compe);
        ziruLeagueSaveInt("ziru_myleague_initialized", 1);
        ziruLeagueSaveInt("ziru_myleague_fixture", 1);
        ziruLeagueSaveInt("ziru_myleague_table_points", 0);
        ziruLeagueSaveInt("ziru_myleague_table_wins", 0);
        ziruLeagueSaveInt("ziru_myleague_table_draws", 0);
        ziruLeagueSaveInt("ziru_myleague_table_losses", 0);
        ziruLeagueSaveInt("ziru_myleague_table_gf", 0);
        ziruLeagueSaveInt("ziru_myleague_table_ga", 0);
        ziruDb->saveMeta("ziru_myleague_history_json", "[]");
        ziruDb->saveMeta("ziru_myleague_last_result_rqid", "");
        ziruDb->saveMeta("ziru_myleague_last_result", "");
        ziruDb->saveMeta("ziru_myleague_last_gameend_reason", "");
        ziruLeagueSaveInt("ziru_myleague_last_added_point", 0);
        ziruMlResetTableV83();
        ziruMlResetPlayerStatsV85();
        zs::logi("OFFLINE_SERVER", "[ZIRU_V81_1_TEAM_SELECTED] team=" + str(team) + " compe=" + str(compe));
      }
    }
    if (!loadMeta("ziru_myleague_fixture")) ziruLeagueSaveInt("ziru_myleague_fixture", 1);
    if (!loadMeta("ziru_myleague_point")) ziruLeagueSaveInt("ziru_myleague_point", 9760);
    if (!loadMeta("ziru_myleague_team_id")) ziruLeagueSaveInt("ziru_myleague_team_id", 100);
    if (!loadMeta("ziru_myleague_compe_unit_id")) ziruLeagueSaveInt("ziru_myleague_compe_unit_id", 113);
    ObjP jo = loadZiruJsonAssetObject("responses/ZIRU_ML_SET_SEASON_ENTER.json");
    patchZiruMyLeagueCommonState(jo);
    if (req) jo->put("rqid", (long long)req->optLong("rqid", 1));
    return serveObjectAsEncryptedMsgPack(jo);
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V81_1_ENTER_ERR] ") + e.what());
    return handleZiruLeagueStatic("responses/ZIRU_ML_SET_SEASON_ENTER.json");
  }
}

Response handleZiruMyLeagueExchangeItemV71() {
  try {
    ObjP req = decodeZiruRequestJson(ziruLastRequestBody);
    if (req) {
      int itemId = req->optInt("item_id", -1);
      ArrP items = nn(loadZiruJsonAssetObject("responses/ZIRU_ML_POINT_ITEM_LIST.json"))->optJSONArray("item_list");
      int cost = 0;
      for (int i = 0; i < nn(items)->length(); i++) {
        ObjP it = nn(items->optJSONObject(i));
        if (it->optInt("item_id", -1) == itemId) { cost = it->optInt("point"); break; }
      }
      int pts = ziruLeagueLoadInt("ziru_myleague_point", 9760);
      if (cost > 0 && pts >= cost) {
        ziruLeagueSaveInt("ziru_myleague_point", pts - cost);
        ziruDb->saveMeta("ziru_myleague_item_" + str(itemId), "1");
      }
    }
    return handleZiruLeagueStatic("responses/ZIRU_ML_EXCHANGE_ITEM.json");
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V71_EXCHANGE_ITEM_ERR] ") + e.what());
    return handleZiruLeagueStatic("responses/ZIRU_ML_EXCHANGE_ITEM.json");
  }
}

Response handleZiruMyLeagueExchangeRentalV71() {
  try {
    ObjP req = decodeZiruRequestJson(ziruLastRequestBody);
    ObjP gid;
    if (req && (gid = req->optJSONObject("gameplayer_id"))) {
      int64_t id = gid->optLong("id", 0);
      ArrP list = nn(loadZiruJsonAssetObject("responses/ZIRU_ML_RENTAL_LIST.json"))->optJSONArray("rental_player_list");
      int cost = 0;
      ObjP found;
      for (int i = 0; i < nn(list)->length(); i++) {
        ObjP e = nn(list->optJSONObject(i));
        ObjP pi = e->optJSONObject("player_info");
        ObjP g2;
        if (pi && (g2 = pi->optJSONObject("gameplayer_id")) && g2->optLong("id") == id) {
          cost = e->optInt("point");
          found = pi;
          break;
        }
      }
      int pts;
      if (found && (pts = ziruLeagueLoadInt("ziru_myleague_point", 9760)) >= cost) {
        ziruLeagueSaveInt("ziru_myleague_point", pts - cost);
        ziruDb->saveMeta("ziru_myleague_rental_player", found->toString());
      }
    }
    return handleZiruLeagueStatic("responses/ZIRU_ML_EXCHANGE_RENTAL.json");
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V71_EXCHANGE_RENTAL_ERR] ") + e.what());
    return handleZiruLeagueStatic("responses/ZIRU_ML_EXCHANGE_RENTAL.json");
  }
}

Response handleZiruMyLeagueInfo() {
  ziruMyLeagueSetActive(1);
  int f = ziruLeagueLoadInt("ziru_myleague_fixture", 1);
  std::string s = f != 2 ? (f < 3 ? "responses/ZIRU_ML_SEASON_INFO_F1.json" : "responses/ZIRU_ML_SEASON_INFO_F3.json") : "responses/ZIRU_ML_SEASON_INFO_F2.json";
  return handleZiruLeagueStatic(s);
}

Response handleZiruMyLeagueInfoV71() {
  ziruMyLeagueSetActive(1);
  int f = ziruLeagueLoadInt("ziru_myleague_fixture", 1);
  JStr m;
  if (f == 2 && (m = loadMeta("ziru_myleague_last_result_rqid")) && *m == "1" && ziruLeagueLoadInt("ziru_myleague_last_home_score", 0) == 0 &&
      ziruLeagueLoadInt("ziru_myleague_last_away_score", 0) == 0) {
    f = 1;
    ziruLeagueSaveInt("ziru_myleague_fixture", 1);
    ziruDb->saveMeta("ziru_myleague_last_result_rqid", "");
  }
  std::string s = "responses/ZIRU_ML_SEASON_INFO_F3.json";
  if (f == 1) s = "responses/ZIRU_ML_SEASON_INFO_F1.json";
  else if (f == 2) s = "responses/ZIRU_ML_SEASON_INFO_F2.json";
  ObjP jo = loadZiruJsonAssetObject(s);
  patchZiruMyLeagueCommonState(jo);
  patchZiruMyLeagueSeasonStateV74(jo);
  patchZiruMyLeagueRankingV79(jo);
  patchZiruMyLeagueLeaderboardsV85(jo);
  try {
    nn(jo)->put("team_id", ziruLeagueLoadInt("ziru_myleague_team_id", 100));
    patchZiruMyLeagueAddedPointV83(jo);
    jo->put("expired_item_list", newArr());
    ObjP sch = nn(nn(loadZiruMyLeagueScheduleV79())->optJSONArray("schedule_list"))->optJSONObject(f - 1);
    if (sch) {
      ArrP ml = sch->optJSONArray("match_list");
      int team = ziruLeagueLoadInt("ziru_myleague_team_id", 100);
      for (int i = 0; i < nn(ml)->length(); i++) {
        ObjP mt = nn(ml->optJSONObject(i));
        if (mt->optInt("home_team_id") == team) { jo->put("user_side", "HOME"); jo->put("opponent_team_id", mt->optInt("away_team_id")); break; }
        if (mt->optInt("away_team_id") == team) { jo->put("user_side", "AWAY"); jo->put("opponent_team_id", mt->optInt("home_team_id")); break; }
      }
    }
    ObjP req = decodeZiruRequestJson(ziruLastRequestBody);
    if (req) jo->put("rqid", (long long)req->optLong("rqid", 1));
    return serveObjectAsEncryptedMsgPack(jo);
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V71_INFO_ERR] ") + e.what());
    return serveObjectAsEncryptedMsgPack(jo);
  }
}

Response handleZiruMyLeagueMainmenuCreativeV78() {
  try {
    ObjP jo = loadZiruJsonAssetObject("responses/get_mainmenu_match_creative_info.json");
    ObjP req = decodeZiruRequestJson(ziruLastRequestBody);
    if (req) nn(jo)->put("rqid", (long long)req->optLong("rqid", 1));
    int init = ziruLeagueLoadInt("ziru_myleague_initialized", 0);
    int team = ziruLeagueLoadInt("ziru_myleague_team_id", 0);
    if (team <= 0) {
      try { team = z27Base(z27Main())->getInt("base_team_id"); } catch (...) {}
      if (team <= 0) team = 100;
      ziruLeagueSaveInt("ziru_myleague_team_id", team);
    }
    int compe = ziruLeagueLoadInt("ziru_myleague_compe_unit_id", 0);
    if (compe <= 0) {
      compe = ziruMyLeagueInferCompetitionV80(team);
      if (compe <= 0) compe = 113;
      ziruLeagueSaveInt("ziru_myleague_compe_unit_id", compe);
    }
    if (init <= 0) ziruLeagueSaveInt("ziru_myleague_initialized", 1);
    ObjP root = nn(jo)->optJSONObject("season_root_info");
    if (root) {
      root->put("team_id", team);
      root->put("league_compe_unit_id", compe);
      ArrP teams = ziruMyLeagueCompetitionTeamsV80(compe);
      if (!teams) { teams = newArr(); teams->put(team); }
      root->put("league_team_id_list", teams);
    }
    return serveObjectAsEncryptedMsgPack(jo);
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V78_SEASON_ROOT_ERR] ") + e.what());
    return handleZiruLeagueStatic("responses/get_mainmenu_match_creative_info.json");
  }
}

Response handleZiruMyLeagueOpponentV72() {
  try {
    ObjP jo = loadZiruJsonAssetObject("responses/ZIRU_ML_GET_VSCOM_OPPONENT.json");
    patchZiruMyLeagueCommonState(jo);
    patchZiruMyLeagueOpponentV82(jo);
    JStr lv = loadMeta("ziru_myleague_cpu_level");
    std::string level = lv ? *lv : std::string("HARDEST");
    ObjP req = decodeZiruRequestJson(ziruLastRequestBody);
    if (req) {
      std::string s = req->optString("cpu_level", "");
      if (!s.empty()) { level = s; ziruDb->saveMeta("ziru_myleague_cpu_level", level); }
      nn(jo)->put("rqid", (long long)req->optLong("rqid", 1));
    }
    nn(jo)->put("cpu_level", level);
    return serveObjectAsEncryptedMsgPack(jo);
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V72_ML_CPU_ERR] ") + e.what());
    return handleZiruLeagueStatic("responses/ZIRU_ML_GET_VSCOM_OPPONENT.json");
  }
}

Response handleZiruMyLeagueResult() {
  int f = ziruLeagueLoadInt("ziru_myleague_fixture", 1) + 1;
  if (f > 3) f = 3;
  ziruLeagueSaveInt("ziru_myleague_fixture", f);
  int p = ziruLeagueLoadInt("ziru_myleague_point", 9760) + 1680;
  ziruLeagueSaveInt("ziru_myleague_point", p);
  ziruMyLeagueSetActive(1);
  return handleZiruLeagueStatic("responses/ZIRU_ML_GAME_RESULT.json");
}

