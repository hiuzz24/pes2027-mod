// ---- z27: division league / squads / kits (OfflineServer.java ~3801-4682) ----

ObjP z27Meta(const std::string& k) {
  JStr m = loadMeta(k);
  if (!m || m->empty()) return nullptr;
  return Object::parse(*m);
}

int32_t z27Int(const std::string& k, int32_t def) {
  try {
    JStr m = loadMeta(k);
    return !m ? def : parseInt(*m);
  } catch (const std::exception&) {
    return def;
  }
}

ObjP z27Request() {
  ObjP r = decodeZiruRequestObject(ziruLastRequestBody);
  if (!r) throw IllegalState("Request decode failed; no commit");
  return r;
}

ObjP z27Ack(const std::string& msg, const ObjP& req) {
  return newObj()->put("msgid", msg)->put("rqid", (long long)nn(req)->optLong("rqid", 1))->put("result", "NOERR")->put("errcode", "NOERR");
}

Response z27Error(const std::string& msg, const std::exception& e) {
  logRequest("ZIRU_MATCH_ERROR", msg + ": " + e.what());
  ObjP req = decodeZiruRequestObject(ziruLastRequestBody);
  return serveObjectAsEncryptedMsgPack(
      newObj()->put("msgid", msg)->put("rqid", (long long)(req ? req->optLong("rqid", 1) : 1))->put("result", "ERROR")->put("errcode", "ERROR"));
}

ObjP z27Teams() { return nn(loadZiruJsonAssetObject("responses/ZIRU_MATCH_TEAMS.json"))->getJSONObject("teams"); }

ObjP z27Team(int32_t id) {
  ObjP teams = z27Teams();
  int32_t lookup_id = (id <= 0 || id == 262143) ? 100 : id;
  ObjP t = teams ? teams->optJSONObject(str(lookup_id)) : nullptr;
  if (!t && teams) {
    for (const char* tid : {"100", "109", "102", "173", "135"}) {
      t = teams->optJSONObject(tid);
      if (t) break;
    }
    if (!t) {
      for (const auto& k : teams->keys()) {
        t = teams->optJSONObject(k);
        if (t) break;
      }
    }
  }
  if (!t) {
    ObjP syn = newObj()
        ->put("base_team_id", lookup_id)
        ->put("base_team_license", "LICENSED")
        ->put("team_name", "PES Team")
        ->put("team_name_short", "PES");
    ArrP kits = newArr();
    kits->put(newObj()->put("id", 1)->put("shirt_rgb", newArr()->put(255)->put(255)->put(255)));
    kits->put(newObj()->put("id", 2)->put("shirt_rgb", newArr()->put(0)->put(0)->put(0)));
    syn->put("kits", kits);
    return syn;
  }
  ObjP copy = z27Copy(t);
  if (id != 0 && id != 262143 && copy->optInt("base_team_id", 0) != id) {
    copy->put("base_team_id", id);
  }
  copy->put("base_team_license", "LICENSED");
  return copy;
}

ObjP z27Identity(const ObjP& t) {
  return newObj()
      ->put("base_team_id", nn(t)->getInt("base_team_id"))
      ->put("base_team_license", "LICENSED")
      ->put("team_name", t->getString("team_name"))
      ->put("team_name_short", t->getString("team_name_short"));
}

ObjP z27Entry() { return nn(loadZiruJsonAssetObject("responses/CmdLogin.json"))->getJSONObject("entry_info"); }

int32_t z27Main() { return z27Int("main_squad_id", z27Entry()->getInt("main_squad_id")); }

ObjP z27Squad(int32_t id) {
  JStr s = loadSquad(id);
  if (s) {
    ObjP jo = Object::parse(*s);
    return jo->optJSONObject("squad_data") ? jo->getJSONObject("squad_data") : jo;
  }
  ArrP list = z27Entry()->getJSONArray("squad_list");
  for (int i = 0; i < list->length(); i++) {
    ObjP sq = list->getJSONObject(i);
    if (sq->optInt("squad_id", -1) == id) return z27Copy(sq->getJSONObject("squad_data"));
  }
  if (list->length() > 0) return z27Copy(list->getJSONObject(0)->getJSONObject("squad_data"));
  throw IllegalState("Unknown squad " + str(id));
}

ObjP z27Base(int32_t id) {
  ObjP m = z27Meta("z27_base_" + str(id));
  if (!m) m = z27Meta("ziru_base_team_squad_" + str(id));
  if (m) {
    ObjP ident = z27Identity(z27Team(m->getInt("base_team_id")));
    std::string defName = z27Team(m->getInt("base_team_id"))->getString("team_name");
    ident->put("team_name", m->optString("team_name", defName));
    std::string defShort = z27Team(m->getInt("base_team_id"))->getString("team_name_short");
    ident->put("team_name_short", m->optString("team_name_short", defShort));
    return ident;
  }
  ArrP list = z27Entry()->getJSONArray("squad_list");
  for (int i = 0; i < list->length(); i++) {
    ObjP sq = list->getJSONObject(i);
    if (sq->optInt("squad_id", -1) == id) return z27Identity(z27Team(sq->getJSONArray("base_team")->getJSONObject(0)->getInt("base_team_id")));
  }
  if (list->length() > 0) return z27Identity(z27Team(list->getJSONObject(0)->getJSONArray("base_team")->getJSONObject(0)->getInt("base_team_id")));
  return z27Identity(z27Team(100));
}

ArrP z27Kits(const ObjP& team) {
  ArrP k = team->getJSONArray("kits");
  ArrP out = newArr();
  for (int i = 0; i < k->length(); i++) out->put(k->getJSONObject(i)->getInt("id"));
  return out;
}

ObjP z27Uniform(const ObjP& base, const ArrP& sel) {
  ArrP kits = z27Kits(z27Team(base->getInt("base_team_id")));
  ArrP out;
  if (sel) {
    out = newArr();
    for (int i = 0; i < sel->length(); i++) {
      out->put(sel->getInt(i));
    }
    for (int j = 0; j < kits->length(); j++) {
      bool have = false;
      for (int i = 0; i < out->length(); i++) if (out->getInt(i) == kits->getInt(j)) have = true;
      if (!have) out->put(kits->getInt(j));
    }
  } else {
    out = kits;
  }
  return newObj()->put("base_team_id", base->getInt("base_team_id"))->put("uniform_list", out);
}

ObjP z27FindBase(const ObjP& jo) {
  ObjP o = jo->optJSONObject("base_team");
  if (o) return o;
  ArrP a = jo->optJSONArray("base_team");
  if (a && a->optJSONObject(0)) return a->optJSONObject(0);
  if (jo->has("base_team_id")) return jo;
  for (const char* k : {"entry_info", "squad_info", "team_data"}) {
    ObjP c = jo->optJSONObject(k);
    ObjP f;
    if (c && (f = z27FindBase(c))) return f;
  }
  return nullptr;
}

Response z27TeamChange(const std::string& cmd) {
  try {
    ObjP req = z27Request();
    int sq = req->optInt("squad_id", z27Main());
    if (cmd == "CMD_SET_MYCLUB_MAIN_SQUAD") {
      sq = req->optInt("main_squad_id", sq);
      z27Squad(sq);
      z27Base(sq);
      z27PutMeta("main_squad_id", str(sq));
    } else if (cmd == "CMD_SET_MYCLUB_BRING_UNIFORM") {
      ArrP sel = req->optJSONArray("set_uniform_list");
      if (!sel) sel = req->optJSONArray("uniform_list");
      ObjP ul;
      if (!sel && (ul = req->optJSONObject("uniform_list"))) sel = ul->optJSONArray("uniform_list");
      if (!sel) throw IllegalArg("Missing uniform selection");
      z27Uniform(z27Base(sq), sel);
      z27PutMeta("z27_kits_" + str(sq), sel->toString());
    } else {
      ObjP fb = z27FindBase(req);
      if (!fb && cmd == "CMD_SET_MYCLUB_ENTRY_INFO") {
        fb = z27Copy(req);
        int defTid = 100;
        try { defTid = z27Base(sq)->getInt("base_team_id"); } catch (...) {}
        fb->put("base_team_id", defTid);
      }
      if (!fb) {
        fb = newObj()->put("base_team_id", 100);
      }
      int tid = fb->getInt("base_team_id");
      if (tid == 262143 || tid <= 0) {
        tid = 100;
        fb->put("base_team_id", tid);
      }
      ObjP ident = z27Identity(z27Team(tid));
      if (cmd == "CMD_SET_MYCLUB_ENTRY_INFO") {
        if (fb->has("team_name")) ident->put("team_name", fb->getString("team_name"));
        if (fb->has("team_name_short")) ident->put("team_name_short", fb->getString("team_name_short"));
      }
      z27PutMeta("z27_base_" + str(sq), ident->toString());
      z27PutMeta("z27_kits_" + str(sq), z27Kits(z27Team(ident->getInt("base_team_id")))->toString());
    }
    ObjP ack = z27Ack(cmd, req);
    ack->put("base_team", newArr()->put(z27Base(sq)));
    ack->put("entry_info", z27Entry());
    z27Restore(ack);
    ack->put("main_squad_id", z27Main());
    logRequest("ZIRU_TEAM_COMMIT", "squad=" + str(sq) + " base=" + str(z27Base(sq)->getInt("base_team_id")));
    return serveObjectAsEncryptedMsgPack(ack);
  } catch (const std::exception& e) {
    return z27Error(cmd, e);
  }
}

void z27SaveSquad(const std::optional<Bytes>& body) {
  ObjP req = decodeZiruRequestObject(body);
  if (!req) throw IllegalArg("No squad request");
  int sq = req->optInt("squad_id", z27Main());
  ObjP data = req->optJSONObject("squad_data");
  if (!data) throw IllegalArg("No squad_data");
  ObjP cur = z27Squad(sq);
  for (auto& k : data->keys()) cur->put(k, data->get(k));
  ObjP fb = z27FindBase(req);
  if (fb) z27Team(fb->getInt("base_team_id"));
  std::string s = cur->toString();
  ziruDb->saveSquad(sq, s);
  JStr back = loadSquad(sq);
  if (!back || *back != s) throw IllegalState("SQLite squad save failed");
  if (fb) {
    ObjP ident = z27Identity(z27Team(fb->getInt("base_team_id")));
    if (fb->has("team_name")) ident->put("team_name", fb->getString("team_name"));
    if (fb->has("team_name_short")) ident->put("team_name_short", fb->getString("team_name_short"));
    bool changed = z27Base(sq)->getInt("base_team_id") != fb->getInt("base_team_id");
    z27PutMeta("z27_base_" + str(sq), ident->toString());
    if (changed) z27PutMeta("z27_kits_" + str(sq), z27Kits(z27Team(fb->getInt("base_team_id")))->toString());
  }
  ArrP sel = req->optJSONArray("set_uniform_list");
  if (sel) {
    z27Uniform(z27Base(sq), sel);
    z27PutMeta("z27_kits_" + str(sq), sel->toString());
  }
  logRequest("ZIRU_SQUAD_COMMIT", "squad=" + str(sq));
}

Response z27SquadAck() {
  try {
    ObjP req = z27Request();
    z27SaveSquad(ziruLastRequestBody);
    return serveObjectAsEncryptedMsgPack(z27Ack("CMD_SET_MYCLUB_SQUAD_INFO", req)->put("achievement_notification", newArr())->put("user_stepup_list", newArr()));
  } catch (const std::exception& e) {
    return z27Error("CMD_SET_MYCLUB_SQUAD_INFO", e);
  }
}

void z27Restore(const ObjP& jo) {
  try {
    ObjP ei = jo->optJSONObject("entry_info");
    if (!ei) return;
    ArrP squads = ei->optJSONArray("squad_list");
    ObjP target;
    if (!squads) {
      ObjP e = z27Entry();
      jo->put("entry_info", e);
      squads = e->getJSONArray("squad_list");
      target = e;
    } else {
      target = ei;
    }
    ObjP base = z27Base(z27Main());
    for (auto& k : base->keys()) target->put(k, base->get(k));
    int mainId = z27Main();
    bool found = false;
    for (int i = 0; i < squads->length(); i++) {
      ObjP s = squads->getJSONObject(i);
      int id = s->getInt("squad_id");
      if (id == mainId) found = true;
      if (loadSquad(id)) s->put("squad_data", z27Squad(id));
      s->put("base_team", newArr()->put(z27Base(id)));
    }
    if (!found)
      squads->put(newObj()->put("squad_id", mainId)->put("name", "Dream Team")->put("squad_data", z27Squad(mainId))->put("base_team", newArr()->put(z27Base(mainId))));
    std::set<int> present;
    for (int i = 0; i < squads->length(); i++) present.insert(squads->getJSONObject(i)->getInt("squad_id"));
    for (int id = 1; id <= 20; id++) {
      if (present.find(id) == present.end() && loadSquad(id)) {
        ObjP sq = z27Squad(id);
        std::string sname = sq->optString("squad_name", "Tactics " + str(id));
        squads->put(newObj()->put("squad_id", id)->put("name", sname)->put("squad_data", sq)->put("base_team", newArr()->put(z27Base(id))));
      }
    }
    target->put("main_squad_id", mainId);
    target->put("current_gameplayer_warehouse_list_max", 900);
    target->put("warehouse_player_max", 900);
    target->put("current_gameplayer_list_max", 900);
    target->put("gameplayer_list_max", 900);
    target->put("unlock_gameplayer_frame", 900);
    target->put("unlock_gameplayer_warehouse_frame", 900);
    JStr kits = loadMeta("z27_kits_" + str(mainId));
    target->put("set_uniform_list", z27Uniform(z27Base(mainId), kits ? Array::parse(*kits) : nullptr)->getJSONArray("uniform_list"));
    ArrP gl = target->optJSONArray("gameplayer_list");
    if (gl) {
      ArrP kept = newArr();
      for (int i = 0; i < gl->length(); i++) {
        ObjP p = gl->getJSONObject(i);
        if (!isZiruPlayerSold(p->getJSONObject("gameplayer_id")->getLong("serial"))) kept->put(p);
      }
      target->put("gameplayer_list", kept);
      ArrP all = Array::parse(ziruDb->loadAllPlayersJson());
      std::set<int64_t> have;
      for (int i = 0; i < kept->length(); i++) have.insert(kept->getJSONObject(i)->getJSONObject("gameplayer_id")->optLong("serial", 0));
      for (int i = 0; i < all->length(); i++) {
        ObjP p = all->getJSONObject(i);
        int64_t s = p->getJSONObject("gameplayer_id")->getLong("serial");
        if (!isZiruPlayerSold(s) && !have.count(s)) kept->put(p);
      }
      gl = kept;
    }
    if (gl) {
      for (int i = 0; i < gl->length(); i++) {
        ObjP p = gl->getJSONObject(i);
        JStr lp = loadPlayer(p->getJSONObject("gameplayer_id")->optLong("serial", 0));
        if (lp) {
          ObjP sv = Object::parse(*lp);
          if (sv->optJSONObject("gameplayer")) sv = sv->getJSONObject("gameplayer");
          if (sv->getJSONObject("gameplayer_id")->getLong("id") == p->getJSONObject("gameplayer_id")->getLong("id")) gl->put(i, sv);
        }
      }
    }
  } catch (const std::exception& e) {
    logRequest("ZIRU_RESTORE_ERROR", e.what());
  }
}

ObjP z27State() {
  ObjP m = z27Meta("z27_division_state");
  if (m) {
    if (!loadMeta("z27_div1_set")) {
      m->put("division", 1);
      m->put("best", 1);
      m->put("point", 0);
      m->put("matches", 0);
      m->put("win", 0);
      m->put("draw", 0);
      m->put("lose", 0);
      z27PutMeta("z27_division_state", m->toString());
      z27PutMeta("z27_div1_set", "1");
    }
    return m;
  }
  z27PutMeta("z27_div1_set", "1");
  int div = std::max(1, std::min(10, z27Int("ziru_league_division", 1)));
  return newObj()
      ->put("division", div)
      ->put("best", std::min(div, z27Int("ziru_league_best_division", div)))
      ->put("point", z27Int("ziru_league_point", 0))
      ->put("matches", z27Int("ziru_league_matches", 0))
      ->put("win", z27Int("ziru_league_win", 0))
      ->put("draw", z27Int("ziru_league_draw", 0))
      ->put("lose", z27Int("ziru_league_lose", 0))
      ->put("sequence", z27Int("ziru_league_opponent_seq", 0))
      ->put("completed", newObj());
}

std::string z27Difficulty(int d) {
  if (d >= 9) return "EASY";
  if (d >= 7) return "NORMAL";
  if (d == 6) return "HARD";
  if (d >= 4) return "VERYHARD";
  if (d >= 2) return "SUPERSTAR";
  return "LEGEND";
}

int32_t z27Promotion(int d) { return ((10 - d) * 2) + 6; }

void z27Info(const ObjP& jo) { z27InfoState(jo, z27State()); }

void z27InfoState(ObjP jo, const ObjP& st) {
  ObjP li = nn(jo)->optJSONObject("league_info");
  if (li) jo = li;
  int d = st->getInt("division");
  int rating = st->optInt("rating", d == 1 ? 1800 : 0);
  jo->put("current_division", d)
      ->put("personal_best_division", st->getInt("best"))
      ->put("current_point", st->getInt("point"))
      ->put("round_match_count", std::max(0, std::min(st->optInt("matches"), 10)))
      ->put("point_promoted", d == 1 ? 30 : z27Promotion(d))
      ->put("point_staying", std::max(0, z27Promotion(d) - 5))
      ->put("point_relegated", std::max(0, z27Promotion(d) - 6))
      ->put("can_skip", "NO")
      ->put("is_com_match", "YES")
      ->put("rating", rating)
      ->put("pre_rating", st->optInt("pre_rating", rating))
      ->put("is_first_enter", "NO")
      ->put("is_first_best_division", "NO")
      ->put("simple_ranking", newArr())
      ->put("personal_best_division_all_phase", st->getInt("best"));
  ObjP cl = jo->optJSONObject("com_level_info");
  if (cl) cl->put("offense_com_level", z27Difficulty(d))->put("defense_com_level", z27Difficulty(d));
  ObjP rm = jo->optJSONObject("round_match_result");
  if (rm) {
    int m = st->getInt("matches");
    rm->put("win", st->getInt("win"))->put("draw", st->getInt("draw"))->put("lose", st->getInt("lose"))->put("ratio", m == 0 ? 0 : (st->getInt("win") * 100) / st->getInt("matches"));
  }
}

ObjP z27Active() {
  ObjP m = z27Meta("z27_active_match");
  if (!m) throw IllegalState("No selected opponent; request GET_VSCOM_OPPONENT first");
  return m;
}

// Opponent cards in the pack have a level limit, but their ability array still
// contains level-one values. Allocate the full budget on a private match copy.
void z27MaxOpponentPlayer(const ObjP& player) {
  if (!player) return;
  ArrP params = player->optJSONArray("parameters");
  ObjP level = player->optJSONObject("level_info");
  if (!params || params->length() < 145 || !level) return;
  int limit = std::max(1, std::min(100, level->optInt("level_limit", 1)));
  int budget = ziruProgressionBudget(limit);
  const char* const groups[] = {"SHOOT", "PASS", "DRIBLE", "QUICKNESS", "STRENGTH",
                                "AERIAL", "DEFENCE", "GK1", "GK2", "GK3"};
  int weight[10] = {};
  int pos = params->optInt(0, 0);
  if (pos == 1) { weight[7] = 4; weight[8] = 4; weight[9] = 4; weight[5] = 1; }
  else if (pos == 2) { weight[6] = 5; weight[5] = 3; weight[4] = 2; weight[1] = 1; }
  else if (pos == 3 || pos == 4) { weight[6] = 4; weight[4] = 3; weight[1] = 2; weight[3] = 2; }
  else if (pos == 5 || pos == 6) { weight[1] = 4; weight[6] = 3; weight[4] = 2; weight[2] = 2; }
  else if (pos == 7 || pos == 8 || pos == 9) { weight[1] = 3; weight[2] = 3; weight[3] = 3; weight[4] = 2; }
  else { weight[0] = 4; weight[2] = 3; weight[3] = 3; weight[4] = 2; }
  int count[10] = {}, spent[10] = {};
  while (budget > 0) {
    int best = -1;
    for (int i = 0; i < 10; i++) {
      int cost = 1 + count[i] / 4;
      if (!weight[i] || count[i] >= 12 || cost > budget) continue;
      if (best < 0 || count[i] * weight[best] < count[best] * weight[i] ||
          (count[i] * weight[best] == count[best] * weight[i] && weight[i] > weight[best])) best = i;
    }
    if (best < 0) break;
    int cost = 1 + count[best] / 4;
    budget -= cost;
    spent[best] += cost;
    count[best]++;
  }
  ObjP progression = newObj();
  for (int i = 0; i < 10; i++) progression->put(groups[i], spent[i]);
  z27ApplyProgressionPoints(params, progression, Array::parse(params->toString()));
  level->put("level", limit)->put("exp_current_level", 0)->put("exp_to_next_level", 0);
}

ObjP z27Opponent(const ObjP& active) {
  ObjP tpl = loadZiruJsonAssetObject("responses/ZIRU_GET_VSCOM_OPPONENT.json");
  ObjP prof = active->getJSONObject("profile");
  ArrP ids = prof->getJSONArray("player_ids");
  ArrP players = newArr(), refs = newArr();
  for (int i = 0; i < ids->length(); i++) {
    ObjP w = getZiruPlayerWrapper(Value((long long)ids->getLong(i)), false);
    if (!w) throw IllegalState("Missing pack ID " + str(ids->getLong(i)));
    ObjP gp = z27Copy(w->getJSONObject("gameplayer"));
    ObjP gid = newObj()->put("id", (long long)ids->getLong(i))->put("serial", (long long)(800000000000000LL + (int64_t)(prof->getInt("profile_id") * 100) + i));
    gp->put("gameplayer_id", gid)->put("uniform_number", i + 1);
    z27MaxOpponentPlayer(gp);
    players->put(gp);
    refs->put(newObj()->put("gameplayer_id", z27Copy(gid))->put("uniform_number", i + 1));
  }
  ObjP sd = nn(tpl)->getJSONObject("squad_data");
  ObjP plan = sd->getJSONObject("squad_gameplan");
  ArrP form = plan->getJSONArray("formation");
  for (int i = 0; i < form->length(); i++) {
    ObjP f = form->getJSONObject(i);
    f->put("formation_offensive", Array::parse(prof->getJSONArray("coordinates")->toString()))
        ->put("formation_defensive", Array::parse(prof->getJSONArray("coordinates")->toString()))
        ->put("team_style", prof->getInt("team_style"))
        ->put("use_multi_formation", "NO");
    ArrP cl = f->optJSONArray("coach_link_data");
    if (cl) for (int k = 0; k < cl->length(); k++) cl->getJSONObject(k)->put("is_link_on", "NO")->put("key_member_Id", 255)->put("link_member_Id", 255);
  }
  ArrP order = newArr();
  for (int i = 0; i < 40; i++) order->put(i < 23 ? i : 255);
  plan->getJSONObject("match_plan_settings")->put("order", order)->put("captain", 1)->put("set_piece_taker", Array::parse("[7,7,10,7,7,7]"));
  sd->put("player_list", refs);
  tpl->put("player_list", players)
      ->put("team_data", z27Copy(active->getJSONObject("away")))
      ->put("game_id", (long long)active->getLong("game_id"))
      ->put("cpu_level", active->getString("cpu_level"))
      ->put("side", "AWAY");
  tpl->getJSONArray("user_info")->getJSONObject(0)->put("user_id", 1700000000 + prof->getInt("profile_id"))->put("user_name", active->getJSONObject("away")->getString("team_name"));
  return tpl;
}

Response z27SelectOpponent() {
  try {
    ObjP req = z27Request();
    ObjP st = z27State();
    ObjP m = z27Meta("z27_active_match");
    if (!m || st->getJSONObject("completed")->has(str(m->getLong("game_id")))) {
      ArrP profs = nn(loadZiruJsonAssetObject("responses/ZIRU_MATCH_PROFILES.json"))->getJSONArray("profiles");
      ObjP prof = profs->getJSONObject(st->getInt("sequence") % profs->length());
      int64_t now = zc::currentTimeMillis();
      while (st->getJSONObject("completed")->has(str(now))) now++;
      m = newObj()
              ->put("game_id", (long long)now)
              ->put("division", st->getInt("division"))
              ->put("profile", z27Copy(prof))
              ->put("home", z27Base(z27Main()))
              ->put("away", z27Identity(z27Team(prof->getInt("base_team_id"))))
              ->put("cpu_level", z27Difficulty(st->getInt("division")))
              ->put("squad_id", z27Main())
              ->put("created_at", (long long)(now / 1000));
      z27PutMeta("z27_active_match", m->toString());
    }
    ObjP opp = z27Opponent(m);
    opp->put("rqid", (long long)req->optLong("rqid", 1));
    logRequest("ZIRU_MATCH_SELECTED", "game=" + str(m->getLong("game_id")));
    return serveObjectAsEncryptedMsgPack(opp);
  } catch (const std::exception& e) {
    return z27Error("CMD_GET_VSCOM_OPPONENT", e);
  }
}

void z27SaveSkills(int64_t serial, const std::vector<int32_t>& list) {
  ObjP p = z27OwnedPlayer(serial);
  if (!p) throw IllegalState("Missing owned player for skill save");
  ArrP a = newArr();
  for (int v : list) a->put(v);
  p->put("additional_skill_id", a);
  ziruDb->savePlayer(serial, p->getJSONObject("gameplayer_id")->getLong("id"), p->toString());
}

ObjP z27Player(const ObjP& gid) {
  int64_t serial = gid->optLong("serial", 0);
  JStr lp = loadPlayer(serial);
  ObjP p = lp ? Object::parse(*lp) : nullptr;
  if (p && p->optJSONObject("gameplayer")) p = p->getJSONObject("gameplayer");
  if (!p || p->getJSONObject("gameplayer_id")->getLong("id") != gid->getLong("id")) {
    ArrP list = z27Entry()->getJSONArray("gameplayer_list");
    int i = 0;
    while (true) {
      if (i >= list->length()) {
        ObjP w = getZiruPlayerWrapper(Value((long long)gid->getLong("id")), false);
        if (!w) throw IllegalState("Missing HOME player " + gid->toString());
        p = z27Copy(w->getJSONObject("gameplayer"));
        p->put("gameplayer_id", z27Copy(gid));
        break;
      }
      p = list->getJSONObject(i);
      if (p->getJSONObject("gameplayer_id")->optLong("serial", -1) == serial && p->getJSONObject("gameplayer_id")->getLong("id") == gid->getLong("id")) break;
      i++;
    }
  }
  z27ApplyAppearanceAndClub(p);
  if (serial > 0) {
    JStr pr = loadProgression(serial);
    if (pr && !pr->empty()) {
      ObjP prog = Object::parse(*pr);
      ObjP w = getZiruPlayerWrapper(Value((long long)gid->getLong("id")), false);
      ArrP bp = (w && w->optJSONObject("gameplayer")) ? w->getJSONObject("gameplayer")->optJSONArray("parameters") : nullptr;
      ArrP params = p->optJSONArray("parameters");
      if (params && bp) {
        z27ApplyProgressionPoints(params, prog, bp);
      }
    }
  }
  return ziruSyncActiveBooster(p);
}

ObjP z27Home(const ObjP& active) {
  ObjP home = nn(loadZiruJsonAssetObject("responses/ZIRU_START_GAME.json"))->getJSONObject("squad_data_home");
  ObjP sq = z27Squad(active->getInt("squad_id"));
  ArrP pl = sq->getJSONArray("player_list");
  ArrP out = newArr();
  for (int i = 0; i < pl->length(); i++) {
    ObjP gid = pl->getJSONObject(i)->getJSONObject("gameplayer_id");
    out->put(newObj()->put("gameplayer_id", z27Copy(gid))->put("parameters", Array::parse(z27Player(gid)->getJSONArray("parameters")->toString()))->put("averaged_overall", 0));
  }
  home->put("player_list", out)->put("squad_gameplan", z27Copy(sq->getJSONObject("squad_gameplan")))->put("team_id", active->getJSONObject("home")->getInt("base_team_id"));
  ArrP co = sq->optJSONArray("coach");
  if (!co || co->length() == 0) throw IllegalState("Missing selected HOME coach");
  ObjP ref = co->getJSONObject(0);
  int64_t cid = ref->getJSONObject("coach_id")->getLong("id");
  ObjP coach = resolveZiruCoachFromLoginV73(cid);
  if (!coach) throw IllegalState("Unknown HOME coach " + str(cid));
  // The serial identifies the selected owned instance, including duplicate coaches.
  coach->put("coach_id", z27Copy(ref->getJSONObject("coach_id")));
  home->put("coach", coach);
  return home;
}

ArrP z27SavedKits(int32_t sq) {
  JStr m = loadMeta("z27_kits_" + str(sq));
  if (!m) return nullptr;
  return Array::parse(*m);
}

ArrP z27AwayKits(const ObjP& active, const ObjP& homeUniform) {
  ObjP ht = z27Team(active->getJSONObject("home")->getInt("base_team_id"));
  ObjP at = z27Team(active->getJSONObject("away")->getInt("base_team_id"));
  int first = homeUniform->getJSONArray("uniform_list")->getInt(0);
  ArrP rgb;
  ArrP hk = ht->getJSONArray("kits"), ak = at->getJSONArray("kits");
  for (int i = 0; i < hk->length(); i++)
    if (hk->getJSONObject(i)->getInt("id") == first) rgb = hk->getJSONObject(i)->getJSONArray("shirt_rgb");
  int best = -1, bi = 0;
  for (int i = 0; i < ak->length(); i++) {
    ArrP c = ak->getJSONObject(i)->getJSONArray("shirt_rgb");
    int d = 0;
    for (int k = 0; k < 3; k++) {
      int x = c->getInt(k) - nn(rgb)->getInt(k);
      d = x * x + d;
    }
    if (d > best) { best = d; bi = i; }
  }
  ArrP out = newArr()->put(ak->getJSONObject(bi)->getInt("id"));
  for (int i = 0; i < ak->length(); i++) if (i != bi) out->put(ak->getJSONObject(i)->getInt("id"));
  return out;
}

Response z27Start() {
  try {
    ObjP req = z27Request();
    ObjP act = z27Active();
    int64_t gid = req->optLong("game_id", act->getLong("game_id"));
    if (gid != act->getLong("game_id")) throw IllegalArg("Stale game_id");
    if (z27State()->getJSONObject("completed")->has(str(gid))) throw IllegalState("Match already completed");
    int mainId = z27Main();
    ObjP current = z27Copy(act);
    current->put("squad_id", mainId)->put("home", z27Base(mainId));
    ObjP home = z27Home(current);
    ObjP hu = z27Uniform(current->getJSONObject("home"), z27SavedKits(mainId));
    bool reuse = act->optBoolean("started", false) && act->getInt("squad_id") == mainId &&
        act->optJSONObject("home_squad") && act->optJSONObject("home_uniform") &&
        act->getJSONObject("home_squad")->toString() == home->toString() &&
        act->getJSONObject("home_uniform")->toString() == hu->toString() &&
        act->getJSONObject("home")->toString() == current->getJSONObject("home")->toString();
    if (!reuse) {
      act->put("squad_id", mainId)->put("home", current->getJSONObject("home"));
      act->put("home_squad", home);
      act->put("home_uniform", hu)->put("away_uniform", z27Uniform(act->getJSONObject("away"), z27AwayKits(act, hu)))->put("started", true);
      z27PutMeta("z27_active_match", act->toString());
    }
    ObjP out = nn(loadZiruJsonAssetObject("responses/ZIRU_START_GAME.json"));
    out->put("rqid", (long long)req->optLong("rqid", 1))
        ->put("game_id", (long long)gid)
        ->put("squad_data_home", z27Copy(act->getJSONObject("home_squad")))
        ->put("uniform_list_home", z27Copy(act->getJSONObject("home_uniform")))
        ->put("uniform_list_away", z27Copy(act->getJSONObject("away_uniform")));
    out->put("squad_data_away", Value::null());
    ObjP ci = out->optJSONObject("commentary_info");
    if (ci) ci->put("pre_division", act->getInt("division"));
    logRequest("ZIRU_MATCH_START", "game=" + str(gid) + " squad=" + str(mainId) + " coach=" +
        home->getJSONObject("coach")->getJSONObject("coach_id")->toString() + " reused=" + str(reuse));
    return serveObjectAsEncryptedMsgPack(out);
  } catch (const std::exception& e) {
    return z27Error("CMD_START_GAME", e);
  }
}

int32_t z27Score(const ObjP& row) {
  if (!row) throw IllegalArg("Missing score row");
  int sum = 0;
  bool any = false;
  for (const char* k : {"score_1st", "score_2nd", "score_ex1st", "score_ex2nd"}) {
    if (row->has(k)) {
      int v = row->getInt(k);
      if (v < 0) throw IllegalArg("Negative score");
      sum = v + sum;
      any = true;
    }
  }
  if (any) return sum;
  throw IllegalArg("No score fields");
}

ObjP z27History(const ObjP& act, const ObjP& req, int32_t a, int32_t b, const std::string& res) {
  ObjP h = z27Copy(nn(loadZiruJsonAssetObject("responses/ZIRU_GET_GAMERESULT_LIST.json"))->getJSONArray("game_result_list")->getJSONObject(0));
  ObjP home = act->getJSONObject("home"), away = act->getJSONObject("away");
  std::string ge = !req->optJSONObject("match_result") ? std::string("NORMAL") : req->getJSONObject("match_result")->optString("abnormalend_reason", "NORMAL");
  h->put("game_id", (long long)act->getLong("game_id"))
      ->put("date", (long long)(zc::currentTimeMillis() / 1000))
      ->put("is_com_match", "YES")
      ->put("gameend_reason", ge)
      ->put("team", newArr()->put(home->getInt("base_team_id"))->put(away->getInt("base_team_id")))
      ->put("emblem_team", newArr()->put(home->getInt("base_team_id"))->put(away->getInt("base_team_id")))
      ->put("team_name", newArr()->put(home->getString("team_name"))->put(away->getString("team_name")))
      ->put("team_name_short", newArr()->put(home->getString("team_name_short"))->put(away->getString("team_name_short")))
      ->put("score", Array::parse(req->getJSONArray("score")->toString()))
      ->put("division", act->getInt("division"))
      ->put("opponent_profile_id", act->getJSONObject("profile")->getInt("profile_id"))
      ->put("game_res", res)
      ->put("home_score", a)
      ->put("away_score", b);
  ArrP pl = h->getJSONArray("player");
  pl->getJSONObject(0)->put("is_com", "NO");
  pl->getJSONObject(1)->put("is_com", "YES")->put("cpu_level", act->getString("cpu_level"))->put("user_info", z27Opponent(act)->getJSONArray("user_info")->getJSONObject(0));
  return h;
}

Response z27Result() {
  try {
    ObjP req = z27Request();
    ObjP act = z27Active();
    ObjP st = z27State();
    ObjP mr = req->optJSONObject("match_result");
    int64_t gid = !mr ? req->optLong("game_id", 0) : mr->optLong("game_id", req->optLong("game_id", 0));
    ObjP cached = z27Meta("z27_result_" + str(gid));
    if (cached) return serveObjectAsEncryptedMsgPack(cached->getJSONObject("response")->put("rqid", (long long)req->optLong("rqid", 1)));
    if (gid != act->getLong("game_id")) throw IllegalArg("Result game_id does not match active snapshot");
    if (!act->optBoolean("started", false)) throw IllegalState("Result before START_GAME");
    ObjP completed = st->getJSONObject("completed");
    std::string key = str(gid);
    ObjP resp;
    ObjP m2 = z27Meta("z27_result_" + key);
    if (m2) {
      resp = z27Copy(m2->getJSONObject("response"));
    } else {
      ArrP sc = req->optJSONArray("score");
      if (!sc || sc->length() != 2) throw IllegalArg("Two actual score rows required; no inferred result");
      int a = z27Score(sc->getJSONObject(0));
      int b = z27Score(sc->getJSONObject(1));
      std::string res = a > b ? "WIN" : (a < b ? "LOSE" : "DRAW");
      bool giveup = req->optString("intentional_give_up") == "YES" && mr && mr->optString("abnormalend_reason") == "NORMAL_GIVEUP";
      if (giveup && a <= b) res = "LOSE";
      int div = st->getInt("division");
      int pt = st->getInt("point");
      if (div == 1) {
        // Approximate the game's Division 1 rating locally; the official
        // server's exact formula is not available in this offline mode.
        int previous = std::max(0, st->optInt("rating", 1800));
        double expected = 1.0 / (1.0 + std::pow(10.0, (1800.0 - previous) / 400.0));
        double actual = res == "WIN" ? 1.0 : (res == "DRAW" ? 0.5 : 0.0);
        int change = (int)std::lround(24.0 * (actual - expected));
        if (res == "WIN") change = std::max(1, change);
        if (res == "LOSE") change = std::min(-1, change);
        st->put("pre_rating", previous)->put("rating", std::max(0, previous + change));
      }
      int gain;
      if (res == "WIN") gain = 3;
      else if (res == "DRAW") gain = 1;
      else gain = 0;
      int np = pt + gain;
      int nm = st->getInt("matches") + 1;
      st->put("point", np)->put("matches", nm);
      std::string k = res == "WIN" ? "win" : res == "DRAW" ? "draw" : "lose";
      st->put(k, st->getInt(k) + 1);
      ObjP hist = z27History(act, req, a, b, res);
      st->put("sequence", st->getInt("sequence") + 1);
      // The menu advertises this point target. Do not hold an already qualified
      // player in the same division until all ten fixtures have been played.
      bool roundEnded = nm >= 10 || (div > 1 && np >= z27Promotion(div));
      if (roundEnded) {
        int nd;
        if (np < z27Promotion(div) || div <= 1) nd = (np >= std::max(0, z27Promotion(div) + (-5)) || div >= 10) ? div : div + 1;
        else nd = div - 1;
        st->put("division", nd)->put("best", std::min(st->getInt("best"), nd))->put("point", 0)->put("matches", 0)->put("win", 0)->put("draw", 0)->put("lose", 0);
      }
      resp = nn(loadZiruJsonAssetObject("responses/ZIRU_GET_VSCOM_GAME_RESULT_TEMPLATE.json"));
      ObjP ge = resp->getJSONObject("game_end_result");
      ObjP mres = ge->getJSONObject("match_result");
      mres->getJSONObject("player_data")->put("team_id", act->getJSONObject("home")->getInt("base_team_id"));
      ObjP an = ge->optJSONObject("analyst_get_game_result_info");
      if (an) an->put("advice_info_match_list", newArr());
      mres->put("game_res", res)
          ->put("gameend_reason", giveup ? "NORMAL_GIVEUP" : "NORMAL")
          ->put("abnormalend_reason", giveup ? "NORMAL_GIVEUP" : "NORMAL")
          ->put("score_data", newObj()->put("home_score", a)->put("away_score", b));
      ObjP ex = ge->optJSONObject("extra_match_result");
      ObjP lm;
      if (ex && (lm = ex->optJSONObject("league_match_result"))) {
        z27InfoState(lm, st);
        ObjP li = lm->optJSONObject("league_info");
        if (!li) li = lm;
        li->put("is_promoted", st->getInt("division") < div ? "YES" : "NO")
            ->put("is_relegated", st->getInt("division") > div ? "YES" : "NO")
            ->put("is_staying", (!roundEnded || st->getInt("division") != div) ? "NO" : "YES");
      }
      completed->put(key, true);
      ziruDb->saveMatchState(st->toString(), "z27_result_" + key, newObj()->put("history", hist)->put("response", z27Copy(resp))->toString());
      if (!z27Meta("z27_result_" + key)) throw IllegalState("Result transaction missing");
      logRequest("ZIRU_RESULT_COMMIT", "game=" + str(gid) + " " + str(a) + "-" + str(b) + " " + res +
          " division=" + str(div) + "->" + str(st->getInt("division")) + " points=" + str(np) + " round=" + str(nm));
    }
    resp->put("rqid", (long long)req->optLong("rqid", 1));
    return serveObjectAsEncryptedMsgPack(resp);
  } catch (const std::exception& e) {
    return z27Error("CMD_GET_VSCOM_GAME_RESULT", e);
  }
}

Response z27HistoryResponse() {
  try {
    ObjP req = z27Request();
    ObjP tpl = loadZiruJsonAssetObject("responses/ZIRU_GET_GAMERESULT_LIST.json");
    ObjP completed = z27State()->getJSONObject("completed");
    std::vector<int64_t> ids;
    for (auto& k : completed->keys()) ids.push_back(parseLong(k));
    std::sort(ids.begin(), ids.end(), std::greater<int64_t>());
    ArrP list = newArr();
    for (int64_t id : ids) list->put(nn(z27Meta("z27_result_" + str(id)))->getJSONObject("history"));
    ArrP lh = Array::parse(ziruDb->loadLeagueHistory());
    for (int i = 0; i < lh->length(); i++)
      if (!completed->has(str(lh->getJSONObject(i)->optLong("game_id")))) list->put(lh->getJSONObject(i));
    nn(tpl)->put("game_result_list", list)->put("rqid", (long long)req->optLong("rqid", 1));
    return serveObjectAsEncryptedMsgPack(tpl);
  } catch (const std::exception& e) {
    return z27Error("CMD_GET_GAMERESULT_LIST", e);
  }
}

std::optional<Response> z27Route(const std::string& uri) {
  std::string up = toUpperUS(replaceAll(uri, "_", ""));
  if (contains(up, "CMDSETMYCLUBSQUADINFO")) return z27SquadAck();
  if (contains(up, "CMDGETUSEABLETEAMLIST")) {
    ObjP jo = loadZiruJsonAssetObject("responses/get_useable_team_list.json");
    ObjP req = z27Request();
    ArrP ids = newArr();
    for (auto& k : z27Teams()->keys()) ids->put(parseInt(k));
    nn(jo)->put("useable_team_list", ids)->put("rqid", (long long)req->optLong("rqid", 1));
    return serveObjectAsEncryptedMsgPack(jo);
  }
  if (contains(up, "CMDGETMYCLUBENTRYINFO") || contains(up, "CMDGETENTRYINFO")) {
    ObjP entry = z27Entry();
    entry->put("current_gameplayer_warehouse_list_max", 900);
    entry->put("warehouse_player_max", 900);
    entry->put("current_gameplayer_list_max", 900);
    entry->put("gameplayer_list_max", 900);
    entry->put("unlock_gameplayer_frame", 900);
    entry->put("unlock_gameplayer_warehouse_frame", 900);
    ObjP put = z27Ack("CMD_GET_MYCLUB_ENTRY_INFO", z27Request())->put("entry_info", entry);
    z27Restore(put);
    return serveObjectAsEncryptedMsgPack(put);
  }
  for (const char* c : {"CMD_UPDATE_MYCLUB_BASE_TEAM", "CMD_SET_MYCLUB_MAIN_SQUAD", "CMD_SET_MYCLUB_BRING_UNIFORM", "CMD_SET_MYCLUB_ENTRY_INFO"}) {
    if (contains(up, replaceAll(c, "_", ""))) return z27TeamChange(c);
  }
  return std::nullopt;
}
