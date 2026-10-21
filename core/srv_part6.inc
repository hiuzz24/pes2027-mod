// ---- part 6 (OfflineServer.java ~3323-3800): MyLeague table/stats, booster sync ----

void ziruMlAddCpuStatV85(int32_t pid, int32_t team, const std::string& key) {
  try {
    JStr m = loadMeta(key);
    ObjP jo = Object::parse(m ? *m : std::string("{}"));
    std::string k = str(pid);
    ObjP o = jo->optJSONObject(k);
    if (!o) o = newObj();
    int v = o->optInt("value", 0) + 1;
    o->put("player_id", pid);
    o->put("team_id", team);
    o->put("value", v);
    o->put("is_com", "YES");
    jo->put(k, o);
    ziruDb->saveMeta(key, jo->toString());
  } catch (const std::exception&) {
  }
}

void ziruMlAddUserStatV85(int64_t pid, int32_t team, const std::string& key) {
  try {
    JStr m = loadMeta(key);
    ObjP jo = Object::parse(m ? *m : std::string("{}"));
    std::string k = str(pid);
    ObjP o = jo->optJSONObject(k);
    if (!o) o = newObj();
    int v = o->optInt("value", 0) + 1;
    o->put("player_id", (long long)pid);
    o->put("team_id", team);
    o->put("value", v);
    o->put("is_com", "NO");
    jo->put(k, o);
    ziruDb->saveMeta(key, jo->toString());
  } catch (const std::exception&) {
  }
}

int32_t ziruMlCurrentOpponentTeamV85() {
  try {
    int f = ziruLeagueLoadInt("ziru_myleague_fixture", 1);
    int team = ziruLeagueLoadInt("ziru_myleague_team_id", 100);
    ArrP sl = nn(loadZiruMyLeagueScheduleV79())->optJSONArray("schedule_list");
    ObjP fx;
    ArrP ml;
    if (!sl || !(fx = sl->optJSONObject(f - 1)) || !(ml = fx->optJSONArray("match_list"))) return 0;
    for (int i = 0; i < ml->length(); i++) {
      ObjP m = ml->optJSONObject(i);
      if (!m) continue;
      int h = m->optInt("home_team_id"), a = m->optInt("away_team_id");
      if (h == team) return a;
      if (a == team) return h;
    }
    return 0;
  } catch (const std::exception&) {
    return 0;
  }
}

void ziruMlGenerateCpuEventsV85(int32_t team, int32_t goals, int32_t fixture) {
  if (goals <= 0) return;
  ArrP roster = ziruMlRosterForTeamV85(team);
  if (!roster) return;
  int len = roster->length();
  if (len <= 0) return;
  for (int i4 = 0; i4 < goals; i4++) {
    int i5 = ((i4 * 3) + team + fixture) % len;
    int scorer = roster->optInt(i5);
    if (scorer > 0) ziruMlAddCpuStatV85(scorer, team, "ziru_myleague_goal_stats_json");
    if (((team + fixture) + i4) % 4 != 0 && len > 1) {
      int i6 = (i5 + 5) % len;
      int ast = roster->optInt(i6);
      if (ast != scorer) ast = roster->optInt((i6 + 1) % len);
      if (ast > 0) ziruMlAddCpuStatV85(ast, team, "ziru_myleague_assist_stats_json");
    }
  }
}

int32_t ziruMlLoadTeamStatV83(int32_t team, const std::string& k) {
  try {
    JStr m = loadMeta(ziruMlTeamStatKeyV83(team, k));
    if (m) return parseInt(*m);
    return 0;
  } catch (const std::exception&) {
    return 0;
  }
}

void ziruMlRecordCpuListV85(const ArrP& a, int32_t team, const std::string& key) {
  if (!a) return;
  for (int i = 0; i < a->length(); i++) {
    ObjP o = a->optJSONObject(i);
    int id;
    if (o && (id = o->optInt("id")) > 0) ziruMlAddCpuStatV85(id, team, key);
  }
}

void ziruMlRecordRealMatchEventsV85(const ObjP& req) {
  if (!req) return;
  int team = ziruLeagueLoadInt("ziru_myleague_team_id", 100);
  int opp = ziruMlCurrentOpponentTeamV85();
  ziruMlRecordUserListV85(req->optJSONArray("goal_player_list"), team, "ziru_myleague_goal_stats_json");
  ziruMlRecordUserListV85(req->optJSONArray("assist_player_list"), team, "ziru_myleague_assist_stats_json");
  if (opp > 0) {
    ziruMlRecordCpuListV85(req->optJSONArray("opp_goal_player_list"), opp, "ziru_myleague_goal_stats_json");
    ziruMlRecordCpuListV85(req->optJSONArray("opp_assist_player_list"), opp, "ziru_myleague_assist_stats_json");
  }
}

void ziruMlRecordUserListV85(const ArrP& a, int32_t team, const std::string& key) {
  if (!a) return;
  for (int i = 0; i < a->length(); i++) {
    ObjP o = a->optJSONObject(i);
    if (!o) continue;
    int64_t id = o->optLong("id", 0);
    if (id > 0) ziruMlAddUserStatV85(id, team, key);
  }
}

void ziruMlResetPlayerStatsV85() {
  ziruDb->saveMeta("ziru_myleague_goal_stats_json", "{}");
  ziruDb->saveMeta("ziru_myleague_assist_stats_json", "{}");
}

void ziruMlResetTableV83() {
  ArrP teams = ziruMyLeagueCompetitionTeamsV80(ziruLeagueLoadInt("ziru_myleague_compe_unit_id", 113));
  if (!teams) return;
  for (int i = 0; i < teams->length(); i++) {
    int t = teams->optInt(i);
    for (const char* k : {"p", "w", "d", "l", "gf", "ga", "pts"}) ziruMlSaveTeamStatV83(t, k, 0);
  }
}

ArrP ziruMlRosterForTeamV85(int32_t team) {
  ObjP r = loadZiruJsonAssetObject("responses/ZIRU_ML_ROSTERS.json");
  if (!r) return nullptr;
  return r->optJSONArray(str(team));
}

void ziruMlSaveTeamStatV83(int32_t team, const std::string& k, int32_t v) { ziruDb->saveMeta(ziruMlTeamStatKeyV83(team, k), str(v)); }

void ziruMlCpuFixtureScoresV86(int32_t homeTeam, int32_t awayTeam, int32_t fixture, int32_t& home, int32_t& away) {
  JStr boundary = loadMeta("ziru_myleague_apk_score_start_fixture");
  int first = boundary ? parseInt(*boundary) : ziruLeagueLoadInt("ziru_myleague_fixture", 1);
  if (!boundary) ziruLeagueSaveInt("ziru_myleague_apk_score_start_fixture", first);
  if (fixture < first) {
    // Completed fixtures from an older IPA must still agree with the saved table.
    home = ((homeTeam + awayTeam) + fixture) % 4;
    away = (((homeTeam * 3) + (awayTeam * 2)) + fixture) % 4;
    return;
  }
  // The supplied APK's CPU-fixture seed is stable across schedule reads and
  // result settlement; use the same two scores in both paths.
  int64_t seed = (int64_t)ziruLeagueLoadInt("ziru_myleague_season_counter", 1) * 31 +
                 (int64_t)fixture * 17 + (int64_t)homeTeam * 7 + awayTeam;
  home = (int32_t)(seed % 4);
  away = (int32_t)((seed / 7) % 4);
}

void ziruMlSimulateCurrentFixtureV83(int32_t a, int32_t b) {
  try {
    int f = ziruLeagueLoadInt("ziru_myleague_fixture", 1);
    int team = ziruLeagueLoadInt("ziru_myleague_team_id", 100);
    ArrP sl = nn(loadZiruMyLeagueScheduleV79())->optJSONArray("schedule_list");
    ObjP fx;
    ArrP ml;
    if (!sl || !(fx = sl->optJSONObject(f - 1)) || !(ml = fx->optJSONArray("match_list"))) return;
    for (int i = 0; i < ml->length(); i++) {
      ObjP m = ml->optJSONObject(i);
      if (!m) continue;
      int h = m->optInt("home_team_id"), w = m->optInt("away_team_id");
      int hs, as;
      if (h == team || w == team) { hs = a; as = b; }
      else {
        ziruMlCpuFixtureScoresV86(h, w, f, hs, as);
        ziruMlGenerateCpuEventsV85(h, hs, f);
        ziruMlGenerateCpuEventsV85(w, as, f);
      }
      ziruMlUpdateTeamV83(h, hs, as);
      ziruMlUpdateTeamV83(w, as, hs);
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V83_SIM_ERR] ") + e.what());
  }
}

std::string ziruMlTeamStatKeyV83(int32_t team, const std::string& k) { return "ziru_ml_team_" + str(team) + "_" + k; }

void ziruMlUpdateTeamV83(int32_t t, int32_t gf, int32_t ga) {
  ziruMlSaveTeamStatV83(t, "p", ziruMlLoadTeamStatV83(t, "p") + 1);
  ziruMlSaveTeamStatV83(t, "gf", ziruMlLoadTeamStatV83(t, "gf") + gf);
  ziruMlSaveTeamStatV83(t, "ga", ziruMlLoadTeamStatV83(t, "ga") + ga);
  if (gf > ga) {
    ziruMlSaveTeamStatV83(t, "w", ziruMlLoadTeamStatV83(t, "w") + 1);
    ziruMlSaveTeamStatV83(t, "pts", ziruMlLoadTeamStatV83(t, "pts") + 3);
  } else if (gf != ga) {
    ziruMlSaveTeamStatV83(t, "l", ziruMlLoadTeamStatV83(t, "l") + 1);
  } else {
    ziruMlSaveTeamStatV83(t, "d", ziruMlLoadTeamStatV83(t, "d") + 1);
    ziruMlSaveTeamStatV83(t, "pts", ziruMlLoadTeamStatV83(t, "pts") + 1);
  }
}

void ziruMyLeagueCommitStateV74(const std::string& res, int32_t h, int32_t a, bool userHome) {
  try {
    int f = ziruLeagueLoadInt("ziru_myleague_fixture", 1);
    ziruDb->saveMeta(ziruMyLeagueFixtureKeyV74(f, "result"), res);
    ziruDb->saveMeta(ziruMyLeagueFixtureKeyV74(f, "home"), str(h));
    ziruDb->saveMeta(ziruMyLeagueFixtureKeyV74(f, "away"), str(a));
    JStr m = loadMeta("ziru_myleague_history_json");
    ArrP hist = m ? Array::parse(*m) : newArr();
    hist->put(res);
    ziruDb->saveMeta("ziru_myleague_history_json", hist->toString());
    int gf = userHome ? h : a, ga = userHome ? a : h;
    ziruLeagueSaveInt("ziru_myleague_table_gf", ziruLeagueLoadInt("ziru_myleague_table_gf", 0) + gf);
    ziruLeagueSaveInt("ziru_myleague_table_ga", ziruLeagueLoadInt("ziru_myleague_table_ga", 0) + ga);
    if (res == "WIN") {
      ziruLeagueSaveInt("ziru_myleague_table_wins", ziruLeagueLoadInt("ziru_myleague_table_wins", 0) + 1);
      ziruLeagueSaveInt("ziru_myleague_table_points", ziruLeagueLoadInt("ziru_myleague_table_points", 0) + 3);
    } else if (res == "DRAW") {
      ziruLeagueSaveInt("ziru_myleague_table_draws", ziruLeagueLoadInt("ziru_myleague_table_draws", 0) + 1);
      ziruLeagueSaveInt("ziru_myleague_table_points", ziruLeagueLoadInt("ziru_myleague_table_points", 0) + 1);
    } else {
      ziruLeagueSaveInt("ziru_myleague_table_losses", ziruLeagueLoadInt("ziru_myleague_table_losses", 0) + 1);
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V74_COMMIT_ERR] ") + e.what());
  }
}

ArrP ziruMyLeagueCompetitionTeamsV80(int32_t compe) {
  try {
    ObjP c = nn(loadZiruJsonAssetObject("responses/ZIRU_ML_COMPETITION_REGISTRY.json"))->optJSONObject("competitions");
    ObjP x;
    if (!c || !(x = c->optJSONObject(str(compe)))) return nullptr;
    return x->optJSONArray("team_ids");
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V80_COMP_TEAMS_ERR] ") + e.what());
    return nullptr;
  }
}

std::string ziruMyLeagueFixtureKeyV74(int32_t f, const std::string& k) { return "ziru_myleague_f" + str(f) + "_" + k; }

int32_t ziruMyLeagueInferCompetitionV80(int32_t team) {
  if (team <= 0) return 0;
  try {
    ObjP m = nn(loadZiruJsonAssetObject("responses/ZIRU_ML_COMPETITION_REGISTRY.json"))->optJSONObject("team_to_competition");
    if (m) return m->optInt(str(team), 0);
    return 0;
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V80_INFER_COMP_ERR] ") + e.what());
    return 0;
  }
}

bool ziruMyLeagueIsActive() { return ziruLeagueLoadInt("ziru_myleague_active", 0) > 0; }

int32_t ziruMyLeagueMaxFixtureV80() {
  try {
    ArrP sl = nn(loadZiruMyLeagueScheduleV79())->optJSONArray("schedule_list");
    if (!sl) return 38;
    int n = sl->length();
    return n > 0 ? n : 38;
  } catch (const std::exception&) {
    return 38;
  }
}

int32_t ziruMyLeagueRewardForCpuLevelV72() {
  JStr m = loadMeta("ziru_myleague_cpu_level");
  std::string l = m ? *m : std::string("HARDEST");
  if (l == "LEGEND") return 1820;
  if (l == "HARDEST") return 1680;
  if (l == "VERYHARD") return 1610;
  if (l == "HARD") return 1540;
  if (l == "NORMAL") return 1400;
  return l == "EASY" ? 1260 : 1120;
}

void ziruMyLeagueSetActive(int32_t v) { ziruLeagueSaveInt("ziru_myleague_active", v); }

bool ziruMyLeagueUserIsHomeV74() {
  try {
    int f = ziruLeagueLoadInt("ziru_myleague_fixture", 1);
    int team = ziruLeagueLoadInt("ziru_myleague_team_id", 100);
    ArrP sl = nn(loadZiruMyLeagueScheduleV79())->optJSONArray("schedule_list");
    ObjP fx;
    ArrP ml;
    if (sl && (fx = sl->optJSONObject(f - 1)) && (ml = fx->optJSONArray("match_list"))) {
      for (int i = 0; i < ml->length(); i++) {
        ObjP m = ml->optJSONObject(i);
        if (!m) continue;
        if (m->optInt("home_team_id") == team) return true;
        if (m->optInt("away_team_id") == team) return false;
      }
    }
    return true;
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V74_SIDE_ERR] ") + e.what());
    return true;
  }
}

void ziruPatchAgentBoxInfo(const ObjP& jo) {
  try {
    int64_t j = ziruLastAgentId;
    if (j > 0) {
      ziruAttachResetCounts(jo, j);
      std::string s = sp("almanac_state").getString("ziru_remaining_" + str(j), "");
      int len = 23;
      if (!s.empty()) {
        try { len = Array::parse(s)->length(); } catch (...) {}
      }
      ArrP a = newArr();
      a->put(0); a->put(0); a->put(0); a->put(0); a->put(len);
      jo->put("remaining_player_count", a);
    }
  } catch (const std::exception&) {
  }
}

ObjP ziruSyncActiveBooster(const ObjP& jo) {
  if (!jo) return jo;
  try {
    ArrP params = jo->optJSONArray("parameters");
    ObjP gid;
    if (params && params->length() >= 145 && (gid = jo->optJSONObject("gameplayer_id"))) {
      int64_t id = gid->optLong("id");
      ObjP w = getZiruPlayerWrapper(Value((long long)id), false);
      ObjP base = w ? w->optJSONObject("gameplayer") : nullptr;
      if (!base) {
        ArrP list = z27Entry()->optJSONArray("gameplayer_list");
        if (!list) return jo;
        int i2 = 0;
        for (;; i2++) {
          if (i2 >= list->length()) return jo;
          ObjP o = list->optJSONObject(i2);
          ObjP g2;
          if (o && (g2 = o->optJSONObject("gameplayer_id")) && g2->optLong("id") == id) { base = o; break; }
        }
      }
      ArrP ib = base->optJSONArray("initial_booster_id_list");
      if (ib) {
        ArrP cur = jo->optJSONArray("initial_booster_id_list");
        if (!cur) {
          cur = Array::parse(ib->toString());
          jo->put("initial_booster_id_list", cur);
        }
        while (cur->length() < 3) cur->put(0);
        for (int i3 = 0; i3 < 3; i3++) {
          int native = ib->optInt(i3, 0);
          int savedInitial = cur->optInt(i3, 0);
          int active = params->optInt(i3 + 142, 0);
          if ((native == 0 || native == 136) && savedInitial > 0 && savedInitial != native) {
            // Older builds wrote a crafted booster into immutable innate
            // metadata. Move it into the active slot before restoring the
            // card's native eligibility to train that slot.
            if (active == 0 || active == 136) params->put(i3 + 142, savedInitial);
            cur->put(i3, native);
          } else if (savedInitial <= 0 && native > 0) {
            cur->put(i3, native);
          }
          if (params->optInt(i3 + 142, 0) <= 0 && native > 0 && native != 136)
            params->put(i3 + 142, native);
        }
      }
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("Booster factory metadata restore failed ") + e.what());
    return jo;
  }
  return jo;
}

ObjP z27Copy(const ObjP& jo) { return Object::parse(nn(jo)->toString()); }

void z27PutMeta(const std::string& k, const std::string& v) {
  ziruDb->saveMeta(k, v);
  JStr r = loadMeta(k);
  if (!r || *r != v) throw IllegalState("SQLite write failed: " + k);
}

void appendZiruMyLeagueRankRowV79(const ArrP& rows, int32_t team, int32_t /*userTeam*/) {
  if (team <= 0) return;
  ObjP o = newObj();
  o->put("rank", 0);
  o->put("pre_rank", 0);
  o->put("team_id", team);
  int pts = ziruMlLoadTeamStatV83(team, "pts"), w = ziruMlLoadTeamStatV83(team, "w"), l = ziruMlLoadTeamStatV83(team, "l");
  int d = ziruMlLoadTeamStatV83(team, "d"), gf = ziruMlLoadTeamStatV83(team, "gf"), ga = ziruMlLoadTeamStatV83(team, "ga");
  o->put("winning_points", pts);
  o->put("wins", w);
  o->put("losses", l);
  o->put("draws", d);
  o->put("goals", gf);
  o->put("goals_against", ga);
  o->put("diff", gf - ga);
  rows->put(o);
}

