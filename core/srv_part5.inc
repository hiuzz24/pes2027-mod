// ---- part 5 (OfflineServer.java ~2805-3322) ----

inline void z27ApplyComPlayStyles(const ArrP& p, int pos, int st1) {
  if (!p || p->length() < 70) return;
  // CF (13) / SS (12)
  if (pos == 13 || pos == 12) {
    p->put(65, 1); // Speeding Bullet
    p->put(66, 1); // Incisive Run
    p->put(69, 1); // Long Ranger
    if (st1 == 1 || st1 == 6 || st1 == 10 || st1 == 5) p->put(64, 1); // Mazing Run
    if (st1 == 5 || st1 == 9) p->put(63, 1); // Trickster
  }
  // Wingers: LWG (10) / RWG (11)
  else if (pos == 10 || pos == 11) {
    p->put(63, 1); // Trickster
    p->put(64, 1); // Mazing Run
    p->put(65, 1); // Speeding Bullet
    p->put(66, 1); // Incisive Run
    p->put(68, 1); // Early Cross
    if (st1 == 6 || st1 == 7) p->put(69, 1); // Long Ranger
  }
  // AMF (9)
  else if (pos == 9) {
    p->put(63, 1); // Trickster
    p->put(64, 1); // Mazing Run
    p->put(67, 1); // Long Ball Expert
    p->put(69, 1); // Long Ranger
    if (st1 == 10) p->put(66, 1); // Hole Player -> Incisive Run
  }
  // LMF (7) / RMF (8)
  else if (pos == 7 || pos == 8) {
    p->put(64, 1); // Mazing Run
    p->put(65, 1); // Speeding Bullet
    p->put(67, 1); // Long Ball Expert
    p->put(68, 1); // Early Cross
    if (st1 == 8) p->put(68, 1); // Cross Specialist
  }
  // DMF (5) / CMF (6)
  else if (pos == 5 || pos == 6) {
    p->put(67, 1); // Long Ball Expert
    p->put(69, 1); // Long Ranger
    if (st1 == 11) { // Box-to-Box
      p->put(65, 1); // Speeding Bullet
      p->put(66, 1); // Incisive Run
    } else if (st1 == 14) { // Orchestrator
      p->put(63, 1); // Trickster
    }
  }
  // LB (3) / RB (4)
  else if (pos == 3 || pos == 4) {
    p->put(65, 1); // Speeding Bullet
    p->put(68, 1); // Early Cross
    if (st1 == 17 || st1 == 18) { // Offensive Fullback / Fullback Finisher
      p->put(66, 1); // Incisive Run
      p->put(64, 1); // Mazing Run
    }
  }
  // CB (2)
  else if (pos == 2) {
    if (st1 == 15 || st1 == 16) { // Build Up / Extra Frontman
      p->put(67, 1); // Long Ball Expert
    }
  }
}

void z27ApplyAppearanceAndClub(const ObjP& cur) {
  if (!cur) return;
  ObjP gid = cur->optJSONObject("gameplayer_id");
  if (!gid) return;
  int64_t pid = gid->optLong("id", 0);
  int32_t base_id = (int32_t)(pid & 0xFFFFFF);
  ArrP params = cur->optJSONArray("parameters");

  // 1. Fix club kit if initial_base_team_id == 0
  if (cur->optInt("initial_base_team_id", 0) == 0) {
    auto it = kKnownClubTeams.find(base_id);
    if (it != kKnownClubTeams.end()) {
      cur->put("initial_base_team_id", it->second);
    }
  }

  // Fix position proficiencies and minimum rating for Calhanoglu
  if (base_id == 44379) {
    if (params && params->length() >= 70) {
      params->put(5, 2); // DMF full
      params->put(6, 2); // CMF full
      params->put(7, 2); // LMF full
      params->put(8, 2); // RMF full
      params->put(9, 2); // AMF full
      if (params->optInt(14, 0) < 80) params->put(14, 84);
    }
    ArrP origParams = cur->optJSONArray("original_parameters");
    if (origParams && origParams->length() >= 70) {
      origParams->put(5, 2);
      origParams->put(6, 2);
      origParams->put(7, 2);
      origParams->put(8, 2);
      origParams->put(9, 2);
      if (origParams->optInt(14, 0) < 80) origParams->put(14, 84);
    }
  }

  // 2. Fix skin tone parameter 49 if 0 (chalk-white albino bug)
  auto restoreAppearance = [&](const ArrP& values) {
    if (!values || values->length() < 61) return;
    if (base_id == 133543 || base_id == 133580 || base_id == 40352) {
      // Licensed models (Neymar, Haaland): parameters 49..60 must remain 0 so native 3D face/braids load properly
      for (int i = 0; i < 12; ++i) values->put(49 + i, 0);
      return;
    }
    bool missing = true;
    bool corrupt = false;
    for (int i = 49; i <= 60; ++i) if (values->optInt(i, 0) == 99) corrupt = true;
    for (int i = 50; i <= 60; ++i) if (values->optInt(i, 0) != 0) missing = false;
    if (corrupt) missing = true;
    auto preset = kAppearancePresets.find(base_id);
    bool presetValid = false;
    if (preset != kAppearancePresets.end()) {
      for (int i = 0; i < 12; ++i) if (preset->second.v[i] != 0 && preset->second.v[i] != 99) presetValid = true;
    }
    if (missing && presetValid) {
      for (int i = 0; i < 12; ++i) values->put(49 + i, preset->second.v[i]);
    } else if (missing) {
      static const int defFace[12] = {3, 1, 1, 2, 3, 0, 2, 0, 2, 2, 48, 0};
      for (int i = 0; i < 12; ++i) values->put(49 + i, defFace[i]);
    }
  };
  restoreAppearance(params);
  if (params && params->length() >= 65) {
    if (base_id != 133543 && base_id != 133580 && base_id != 40352 && params->optInt(49, 0) <= 0) params->put(49, 3);
  }

  // 3. Synchronize parameters[61] (Offensive PlayStyle), parameters[62] (Defensive PlayStyle), and COM styles 63..69
  ArrP ps = cur->optJSONArray("play_style");
  if (ps && ps->length() > 0) {
    int st1 = ps->optInt(0, 0);
    int pos = params ? params->optInt(0, 0) : 0;
    int st2 = ps->length() > 1 ? ps->optInt(1, 0) : 0;
    if (st2 <= 0) {
      st2 = z27GetDefensiveStyle(pos, st1);
      if (ps->length() > 1) ps->put(1, st2);
      else ps->put(st2);
    }
    if (params && params->length() >= 70) {
      if (st1 > 0) params->put(61, st1);
      params->put(62, st2);
      z27ApplyComPlayStyles(params, pos, st1);
    }
    ArrP origParams = cur->optJSONArray("original_parameters");
    restoreAppearance(origParams);
    if (origParams && origParams->length() >= 70) {
      if (base_id != 133543 && origParams->optInt(49, 0) <= 0) origParams->put(49, 3);
      if (st1 > 0) origParams->put(61, st1);
      origParams->put(62, st2);
      z27ApplyComPlayStyles(origParams, origParams->optInt(0, 0), st1);
    }
  }
}

void z27ApplyProgressionPoints(const ArrP& params, const ObjP& prog, const ArrP& baseParams) {
  if (!params || !prog || params->length() < 145) return;
  const ArrP& src = (baseParams && baseParams->length() >= 145) ? baseParams : params;

  int strengthCount = ziruCountFromSpent(prog->optInt("STRENGTH", 0));
  int shootCount    = ziruCountFromSpent(prog->optInt("SHOOT", 0));
  int passCount     = ziruCountFromSpent(prog->optInt("PASS", 0));
  int dribbleCount  = ziruCountFromSpent(prog->optInt("DRIBLE", 0) + prog->optInt("DRIBBLE", 0));
  int quickCount    = ziruCountFromSpent(prog->optInt("QUICKNESS", 0));
  int aerialCount   = ziruCountFromSpent(prog->optInt("AERIAL", 0));
  int defCount      = ziruCountFromSpent(prog->optInt("DEFENCE", 0));
  int gk1Count      = ziruCountFromSpent(prog->optInt("GK1", 0));
  int gk2Count      = ziruCountFromSpent(prog->optInt("GK2", 0));
  int gk3Count      = ziruCountFromSpent(prog->optInt("GK3", 0));

  int deltas[45] = {0};

  // SHOOT -> Finishing (20), Place Kicking (27), Curl (28)
  deltas[20] += shootCount;
  deltas[27] += shootCount;
  deltas[28] += shootCount;

  // PASS -> Low Pass (21), Lofted Pass (22)
  deltas[21] += passCount;
  deltas[22] += passCount;

  // DRIBBLE -> Ball Control (17), Dribble (18), Tight Possession (19)
  deltas[17] += dribbleCount;
  deltas[18] += dribbleCount;
  deltas[19] += dribbleCount;

  // QUICKNESS -> Offensive Awareness (14), Acceleration (35), Balance (36)
  deltas[14] += quickCount;
  deltas[35] += quickCount;
  deltas[36] += quickCount;

  // STRENGTH -> Speed (34), Kicking Power (37), Stamina (40)
  deltas[34] += strengthCount;
  deltas[37] += strengthCount;
  deltas[40] += strengthCount;

  // AERIAL -> Heading (23), Jump (38), Physical Contact (39)
  deltas[23] += aerialCount;
  deltas[38] += aerialCount;
  deltas[39] += aerialCount;

  // DEFENCE -> Defensive Awareness (15), Tackling (24), Aggression (25), Defensive Engagement (26)
  deltas[15] += defCount;
  deltas[24] += defCount;
  deltas[25] += defCount;
  deltas[26] += defCount;

  // GK1 -> GK Awareness (29), Jump (38)
  deltas[29] += gk1Count;
  deltas[38] += gk1Count;

  // GK2 -> GK Clearing (31), GK Reach (39)
  deltas[31] += gk2Count;
  deltas[39] += gk2Count;

  // GK3 -> GK Catching (30), GK Reflexes (32)
  deltas[30] += gk3Count;
  deltas[32] += gk3Count;

  for (int idx = 14; idx <= 40; idx++) {
    int baseVal = src->optInt(idx, 0);
    int newVal = std::min(105, baseVal + deltas[idx]);
    params->put(idx, newVal);
  }
}

// Catalog cards that already arrive at their level limit contain finished
// ratings. Auto may redistribute their earned points, but must not increase
// the card's total attributes again.
void z27BalanceMaxLevelProgression(const ArrP& params, const ArrP& baseParams) {
  if (!params || !baseParams || params->length() < 41 || baseParams->length() < 41) return;
  int gained = 0;
  bool trained[41] = {};
  for (int idx = 14; idx <= 40; ++idx) {
    if (idx == 33) continue;
    int base = baseParams->optInt(idx, 0);
    int value = std::min(params->optInt(idx, base), std::min(105, base + 5));
    value = std::max(base, value);
    trained[idx] = value > base;
    params->put(idx, value);
    gained += value - base;
  }
  while (gained > 0) {
    bool changed = false;
    for (int idx = 14; idx <= 40 && gained > 0; ++idx) {
      if (idx == 33) continue;
      int base = baseParams->optInt(idx, 0);
      int value = params->optInt(idx, base);
      if (!trained[idx] && value > std::max(40, base - 3)) {
        params->put(idx, value - 1);
        --gained;
        changed = true;
      }
    }
    if (!changed) break;
  }
  for (int idx = 14; idx <= 40 && gained > 0; ++idx) {
    if (idx == 33) continue;
    int base = baseParams->optInt(idx, 0);
    int value = params->optInt(idx, base);
    int drop = std::min(gained, std::max(0, value - base));
    params->put(idx, value - drop);
    gained -= drop;
  }
}

ObjP refreshZiruGameplayerFromCanonical(const ObjP& jo) {
  if (!jo) return jo;
  try {
    ObjP gid = jo->optJSONObject("gameplayer_id");
    if (gid) {
      int64_t id = gid->optLong("id", 0);
      int64_t serial = gid->optLong("serial", 0);
      if (id > 0) {
        ObjP cur = jo;
        JStr lp;
        if (serial > 0 && (lp = loadPlayer(serial)) && !lp->empty()) {
          cur = Object::parse(*lp);
          ObjP gp = cur->optJSONObject("gameplayer");
          if (gp) cur = gp;
          ObjP g2 = cur->optJSONObject("gameplayer_id");
          if (!g2 || g2->optLong("id") != id) cur = jo;
        }
        ObjP w = getZiruPlayerWrapper(Value((long long)id), false);
        if (!w) {
          w = getZiruPlayerWrapperByBaseId((int32_t)(id & 0xFFFFFF));
        }
        ObjP base;
        if (w && (base = w->optJSONObject("gameplayer"))) {
          ArrP ib = base->optJSONArray("initial_booster_id_list");
          ArrP curIb = cur->optJSONArray("initial_booster_id_list");
          if (!curIb) {
            curIb = newArr()->put(0)->put(0)->put(0);
            cur->put("initial_booster_id_list", curIb);
          }
          while (curIb->length() < 3) curIb->put(0);
          if (ib) {
            for (int b = 0; b < ib->length() && b < 3; ++b) {
              int bidVal = ib->optInt(b, 0);
              if (bidVal > 0 && curIb->optInt(b, 0) == 0) curIb->put(b, bidVal);
            }
          }
          ArrP ps = base->optJSONArray("play_style");
          if (ps && ps->length() > 0 && ps->optInt(0, 0) > 0) cur->put("play_style", ps);
          ArrP baseParams = base->optJSONArray("parameters");
          ArrP curParams = cur->optJSONArray("parameters");
          if (baseParams && curParams) {
            while (curParams->length() < 145) curParams->put(0);
            for (int idx = 70; idx <= 141; ++idx) {
              int baseSkill = baseParams->optInt(idx, 0);
              if (baseSkill != 0) {
                curParams->put(idx, baseSkill);
              }
            }
            for (int b = 0; b < 3; ++b) {
              int bVal = curIb->optInt(b, 0);
              if (bVal > 0 && curParams->optInt(142 + b, 0) == 0) curParams->put(142 + b, bVal);
              else if (baseParams->length() >= 145 && curParams->optInt(142 + b, 0) == 0) {
                int bpVal = baseParams->optInt(142 + b, 0);
                if (bpVal > 0) {
                  curParams->put(142 + b, bpVal);
                  if (curIb->optInt(b, 0) == 0) curIb->put(b, bpVal);
                }
              }
            }
          }
          ArrP origParams = cur->optJSONArray("original_parameters");
          if (baseParams && origParams) {
            while (origParams->length() < 145) origParams->put(0);
            for (int idx = 70; idx <= 141; ++idx) {
              int baseSkill = baseParams->optInt(idx, 0);
              if (baseSkill != 0) {
                origParams->put(idx, baseSkill);
              }
            }
            for (int b = 0; b < 3; ++b) {
              int bVal = curIb->optInt(b, 0);
              if (bVal > 0 && origParams->optInt(142 + b, 0) == 0) origParams->put(142 + b, bVal);
            }
          }
          ObjP bl = base->optJSONObject("level_info");
          int lim = bl ? bl->optInt("level_limit", 1) : 1;
          if (lim == 1 && (((int32_t)id) & 16777215) == 134665) {
            lim = 33;
            logRequest("ZIRU_V16_LEVEL", "[ZIRU_V16_ADRIANO_FALLBACK] base_id=134665 level_limit=33");
          }
          if ((((int32_t)id) & 16777215) == 4522 || id == 89138556572074LL) {
            lim = 36;
            logRequest("ZIRU_CR7_UNLOCK", "[ZIRU_CR7_UNNERFED] Unlocked Prime CR7 level limit to 36");
          }
          ObjP li = cur->optJSONObject("level_info");
          if (!li) { li = newObj(); cur->put("level_info", li); }
          li->put("level_limit", lim);
          li->put("level", lim);
          li->put("exp_total", 0);
          li->put("exp_current_level", 0);
          li->put("exp_to_next_level", 0);
          int cp = ziruUnspentNativePoints(base);
          if (serial > 0) {
            JStr pr = loadProgression(serial);
            if (pr && !pr->empty()) {
              ObjP progression = Object::parse(*pr);
              int spent = std::max(0, progression->optInt("_spent_total", 0));
              if (!isZiruMessiCard(id) && spent > cp && lp && !lp->empty() && cur != jo &&
                  baseParams && curParams && baseParams->length() >= 41 && curParams->length() >= 41) {
                std::string key = "ziru_overbudget_backup_" + str(serial);
                if (!loadMeta(key)) ziruDb->saveMeta(key, *lp);
                if (!loadMeta(key + "_progression")) ziruDb->saveMeta(key + "_progression", *pr);
                for (int idx = 14; idx <= 40; ++idx)
                  curParams->put(idx, baseParams->optInt(idx, 0));
                ziruDb->saveProgression(serial, newObj()->put("_spent_total", 0)->toString());
                spent = 0;
                logRequest("ZIRU_AUTO_BUDGET", "Restored overbudget player serial=" + str(serial));
              }
              cp = std::max(0, cp - spent);
              // A saved plan contains the exact attributes accepted by the
              // development screen. Rebuilding it from the catalog on every
              // read made Auto results jump between views and erased edits.
            }
          }
          li->put("customize_point", cp);
        }
        ArrP curPs = cur->optJSONArray("play_style");
        if (!curPs || curPs->length() == 0 || curPs->optInt(0, 0) <= 0) {
          cur->put("play_style", z27ResolvePlayStyle(id, cur));
        }
        std::vector<int32_t> al;
        std::string s = sp("almanac_state").getString("ziru_skills_" + str(serial), "");
        if (!s.empty()) {
          for (auto& part : split(s, ',')) {
            try {
              int v = parseInt(part);
              if (isZiruSkillId(v) && !vcontains(al, v) && al.size() < 5) al.push_back(v);
            } catch (...) {}
          }
        }
        if (al.empty()) {
          ArrP addSkills = cur->optJSONArray("additional_skill_id");
          if (addSkills) {
            for (int i = 0; i < addSkills->length(); i++) {
              int v = addSkills->optInt(i, -1);

              if (isZiruSkillId(v) && !vcontains(al, v) && al.size() < 5) al.push_back(v);
            }
          }
        }
        ArrP ja = newArr();
        for (int v : al) ja->put(v);
        cur->put("additional_skill_id", ja);
        ArrP params = cur->optJSONArray("parameters");
        if (params && params->length() >= 70) {
          for (int v : al) {
            if (isZiruSkillId(v)) {
              int pIdx = kZiruSkillParameters[v];
              if (isZiruSkillParameter(pIdx) && pIdx < params->length()) params->put(pIdx, 1);
            }
          }
        }
        z27ApplyAppearanceAndClub(cur);
        applyZiruRonaldoMaxProgression(cur);
        if (isZiruMessiCard(id)) {
          int spent = 0;
          JStr progression = serial > 0 ? loadProgression(serial) : JStr();
          if (progression && !progression->empty())
            spent = std::max(0, Object::parse(*progression)->optInt("_spent_total", 0));
          std::string marker = "messi_elite_restored_" + str(serial);
          bool restore = serial <= 0 || !loadMeta(marker);
          applyZiruMessiElite(cur, restore, ziruMessiBudget(serial) - spent);
          if (serial > 0 && restore) ziruDb->saveMeta(marker, "1");
        }
        applyZiruWeakFootFix(cur);
        if (serial > 0) {
          ziruDb->savePlayer(serial, id, cur->toString());
        }
        return ziruSyncActiveBooster(cur);
      }
    }
    return jo;
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V14_PLAYER_NORMALIZE_ERR] ") + e.what());
    return jo;
  }
}

ObjP resolveZiruCoachFromLoginV73(int64_t id) {
  try {
    ObjP login = loadZiruJsonAssetObject("responses/CmdLogin.json");
    ObjP ei;
    ArrP cl;
    if (!login || !(ei = login->optJSONObject("entry_info")) || !(cl = ei->optJSONArray("coach_list"))) return nullptr;
    auto boostCoach = [](const ObjP& c) -> ObjP {
      ObjP res = Object::parse(c->toString());
      res->put("possession", 99);
      res->put("short_counter", 99);
      res->put("long_ball", 99);
      res->put("long_counter", 99);
      res->put("side_attack", 99);
      res->put("overload", 99);
      return res;
    };
    for (int i = 0; i < cl->length(); i++) {
      ObjP c = cl->optJSONObject(i);
      ObjP cid;
      if (c && (cid = c->optJSONObject("coach_id")) && cid->optLong("id", 0) == id) return boostCoach(c);
    }
    if (cl->length() > 0) return boostCoach(cl->getJSONObject(0));
    return nullptr;
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V73_ML_COACH_RESOLVE_ERR] ") + e.what());
    return nullptr;
  }
}

std::string resolveZiruSkillCategory() {
  if (ziruSkillCategory) {
    std::string s = *ziruSkillCategory;
    size_t b = 0, e = s.size();
    while (b < e && (unsigned char)s[b] <= ' ') b++;
    while (e > b && (unsigned char)s[e - 1] <= ' ') e--;
    std::string t = s.substr(b, e - b);
    if (!t.empty()) return toUpperUS(t);
  }
  int i = ziruSkillCategoryCode;
  if (i >= 0) {
    if (i == 0) return "SHOOT";
    if (i == 1) return "PASS";
    if (i == 2) return "DRIBBLE";
    if (i == 3) return "DEFENCE";
    if (i == 4 || i == 5) return "OTHER";
  }
  int i2 = ziruSkillItemEffect;
  if (i2 <= 0) i2 = ziruSkillItemGrade;
  switch (i2) {
    case 1: return "SHOOT";
    case 2: return "PASS";
    case 3: return "DRIBBLE";
    case 4: return "DEFENCE";
    case 5: return "OTHER";
    default: return "UNKNOWN";
  }
}

void sanitizeZiruAdditionalSkills(const ObjP& jo) {
  try {
    ObjP ei = jo->optJSONObject("entry_info");
    ArrP list;
    if (!ei || !(list = ei->optJSONArray("gameplayer_list"))) return;
    for (int i = 0; i < list->length(); i++) {
      ObjP p = list->optJSONObject(i);
      ArrP sk;
      if (p && (sk = p->optJSONArray("additional_skill_id"))) {
        ArrP n = newArr();
        for (int k = 0; k < sk->length(); k++) {
          int v = sk->optInt(k);
          if (isZiruSkillId(v) && n->length() < 5) n->put(v);
        }
        p->put("additional_skill_id", n);
      }
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_SKILL_V21_SANITIZE_ERR] ") + e.what());
  }
}

void saveZiruLeagueHistory(int64_t gameId, int32_t a, int32_t b) {
  try {
    JStr m = loadMeta("ziru_league_history_json");
    ArrP hist = newArr();
    if (m && !m->empty()) hist = Array::parse(*m);
    JStr m2 = loadMeta("ziru_league_last_game_id");
    int64_t j2 = gameId;
    if (m2 && !m2->empty()) j2 = parseLong(*m2);
    ObjP item = buildZiruLeagueHistoryItem(j2, a, b);
    if (item) {
      JStr m3 = loadMeta("ziru_league_last_gameend_reason");
      if (m3 && !m3->empty()) item->put("gameend_reason", *m3);
      ziruDb->saveLeagueHistory(gameId, item->toString());
      ArrP out = newArr();
      out->put(item);
      int len = hist->length();
      for (int i = 0; i < len && i < 49; i++) {
        ObjP o = hist->optJSONObject(i);
        if (o) out->put(o);
      }
      ziruDb->saveMeta("ziru_league_history_json", out->toString());
    }
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("[ZIRU_V64_HISTORY_SAVE_ERR] ") + e.what());
  }
}

void saveZiruPlayerToDb(const ObjP& jo) {
  if (!jo) return;
  ObjP gid = jo->optJSONObject("gameplayer_id");
  if (gid) {
    int64_t s = gid->optLong("serial", 0);
    int64_t id = gid->optLong("id", 0);
    if (s > 0) ziruDb->savePlayer(s, id, jo->toString());
  }
}

void saveZiruSkillsToDb(int64_t serial, const std::vector<int32_t>& list) { z27SaveSkills(serial, list); }

Response serveLivedataAsset(const std::string& name) {
  std::string data;
  if (!assets.read(name, data)) return newFixedText(404, "LiveData asset not found");
  zs::logi("OFFLINE_SERVER", "[ALMANAC_V8] Served LiveData asset: " + name);
  return newFixed(200, "application/octet-stream", Bytes(data.begin(), data.end()));
}

Response serveObjectAsEncryptedMsgPack(const ObjP& jo) {
  // packValue(null) packs nil – Java does not throw here
  Response r = newFixed(200, "application/x-msgpack", encodeClientResponse(jo ? Value(jo) : Value()));
  r.headers["Connection"] = "close";
  return r;
}

void sortZiruMlLeaderboardV85(const ArrP& a) {
  if (!a) return;
  int len = a->length();
  for (int i = 0; i < len - 1; i++) {
    int lim = (len - 1) - i;
    for (int k = 0; k < lim; k++) {
      ObjP x = a->optJSONObject(k), y = a->optJSONObject(k + 1);
      if (x && y && x->optInt("value") < y->optInt("value")) { a->put(k, y); a->put(k + 1, x); }
    }
  }
}

void sortZiruMyLeagueRankingV83(const ArrP& a) {
  if (!a) return;
  int len = a->length();
  for (int i = 0; i < len - 1; i++) {
    int lim = (len - 1) - i;
    for (int k = 0; k < lim; k++) {
      ObjP x = a->optJSONObject(k), y = a->optJSONObject(k + 1);
      if (!x || !y) continue;
      int p1 = x->optInt("winning_points"), p2 = y->optInt("winning_points");
      bool sw = false;
      if (p1 < p2) sw = true;
      else if (p1 == p2) {
        int d1 = x->optInt("diff"), d2 = y->optInt("diff");
        if (d1 < d2) sw = true;
        else if (d1 == d2 && x->optInt("goals") < y->optInt("goals")) sw = true;
      }
      if (sw) { a->put(k, y); a->put(k + 1, x); }
    }
  }
  for (int i = 0; i < len; i++) {
    ObjP o = a->optJSONObject(i);
    if (o) o->put("rank", i + 1);
  }
}

void traceZiruUserCompeV87(const std::string& uri) {
  if (!contains(uri, "USER_COMPE")) return;
  zs::logi("ZIRU_WC", "[ZIRU_V87_USER_COMPE_URI] " + uri);
  ObjP req;
  if (ziruLastRequestBody && (req = decodeZiruRequestJson(ziruLastRequestBody)))
    zs::logi("ZIRU_WC", "[ZIRU_V87_USER_COMPE_REQ] msgid=" + req->optString("msgid", "") + " body=" + req->toString());
}

ObjP z27OwnedPlayer(int64_t serial) {
  JStr lp = loadPlayer(serial);
  if (lp) {
    ObjP jo = Object::parse(*lp);
    ObjP gp = jo->optJSONObject("gameplayer");
    return gp ? gp : jo;
  }
  std::string s = sp("almanac_state").getString("recruited_players", "[]");
  if (s.empty()) s = "[]";
  ArrP arr = Array::parse(s);
  bool second = false;
  while (true) {
    for (int i = 0; i < arr->length(); i++) {
      ObjP o = arr->optJSONObject(i);
      if (!o) continue;
      ObjP gp = o->optJSONObject("gameplayer");
      if (gp) o = gp;
      ObjP gid = o->optJSONObject("gameplayer_id");
      if (gid && gid->optLong("serial") == serial) return o;
    }
    if (second) return nullptr;
    arr = z27Entry()->getJSONArray("gameplayer_list");
    second = true;
  }
}

void z27RepairLoginRoster(const ObjP& jo) {
  try {
    ObjP ei = jo->optJSONObject("entry_info");
    ArrP list;
    if (!ei || !(list = ei->optJSONArray("gameplayer_list"))) return;
    ArrP out = newArr();
    std::set<int64_t> seen;
    for (int i = 0; i < list->length(); i++) {
      ObjP p = list->optJSONObject(i);
      ObjP gid;
      if (p && (gid = p->optJSONObject("gameplayer_id"))) {
        int64_t s = gid->optLong("serial");
        if (!isZiruPlayerSold(s) && seen.insert(s).second) out->put(refreshZiruGameplayerFromCanonical(p));
      }
    }
    ei->put("gameplayer_list", out);
    applyZiruSkillStateToLogin(jo);
  } catch (const std::exception& e) {
    zs::loge("OFFLINE_SERVER", std::string("Login roster repair failed ") + e.what());
    throw;
  }
}

void z28PreserveCareerTraining(const ObjP& jo, const ArrP& params) {
  ziruSyncActiveBooster(jo);
  ArrP cur = jo->optJSONArray("parameters");
  if (cur) {
    if (cur->length() < 145 || params->length() < 145) return;
    for (int i = 0; i < 3; i++) params->put(142 + i, cur->optInt(142 + i));
  }
}

void ziruAttachResetCounts(const ObjP& jo, int64_t agent) {
  try {
    ArrP list = nn(loadZiruJsonAssetObject("responses/CmdGetMyclubAgentlist.json"))->optJSONArray("agent_list");
    if (!list) return;
    for (int i = 0; i < list->length(); i++) {
      ObjP a = list->optJSONObject(i);
      if (a && a->optLong("agent_id") == agent) {
        ArrP b = a->optJSONArray("base_player_count");
        if (b) {
          ArrP c = Array::parse(b->toString());
          jo->put("remaining_player_count", c);
          jo->put("base_player_count", c);
          jo->put("can_reset", "YES");
        }
        return;
      }
    }
  } catch (const std::exception&) {
  }
}

int32_t ziruCountFromSpent(int32_t spent) {
  int c = 0, step = 1, rem = spent;
  while (rem >= step) {
    rem -= step;
    c++;
    if (c % 4 == 0) step++;
  }
  return c;
}

std::string ziruLeagueCpuLevel(int i) {
  if (i == 8) return "EASY";
  if (i == 7) return "NORMAL";
  if (i == 6) return "HARD";
  if (i == 5) return "VERYHARD";
  return i < 3 ? "LEGEND" : "SUPERSTAR";
}

int32_t ziruLeagueLoadInt(const std::string& k, int32_t def) {
  JStr m = loadMeta(k);
  if (m) {
    try { return parseInt(*m); } catch (const std::exception&) {}
  }
  return def;
}

int32_t ziruLeaguePromotionThreshold(int i) {
  if (i == 8) return 15;
  if (i == 7) return 16;
  if (i != 6) return i != 5 ? 19 : 18;
  return 17;
}

int32_t ziruLeagueRoundLimit(int i) { return (i == 10 || i == 9) ? 8 : 10; }

void ziruLeagueSaveInt(const std::string& k, int32_t v) { ziruDb->saveMeta(k, str(v)); }

