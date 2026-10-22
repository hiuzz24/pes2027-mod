// End-to-end encrypted commands, synthetic fixtures (no game assets required).
#include "../core/server.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <sqlite3.h>
#include <sys/wait.h>
using namespace zsv;
static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static void asset(const std::string& root, const char* name, const ObjP& obj) {
  std::ofstream(root + "/responses/" + name) << obj->toString();
}
static ObjP obj(const char* text) { return Object::parse(text); }
static ObjP call(OfflineServer& srv, const char* cmd, ObjP req = newObj()) {
  req->put("msgid", cmd)->put("rqid", 42);
  Request r; r.uri = std::string("/pes22/gate/gate_") + cmd + ".php";
  r.body = zc::encodeRequest(Value(req)); r.headers["content-length"] = std::to_string(r.body.size());
  Response resp = srv.serve(r); Value result;
  check(resp.status == 200 && zc::decodeResponse(resp.body, result) && result.o, "encrypted response");
  check(result.o->optString("result") != "ERROR", result.o->toString().c_str());
  return result.o;
}
static ObjP coachRef(int id, int serial) {
  return newObj()->put("coach_id", newObj()->put("id", id)->put("serial", serial));
}
static int coachId(const ObjP& start) {
  return start->getJSONObject("squad_data_home")->getJSONObject("coach")->getJSONObject("coach_id")->getInt("id");
}
static ObjP result(OfflineServer& s, int64_t id, int a, int b, bool giveup = false) {
  auto req = newObj()->put("game_id", (long long)id)->put("match_result", newObj()->put("game_id", (long long)id)
      ->put("abnormalend_reason", giveup ? "NORMAL_GIVEUP" : "NORMAL"))
      ->put("score", newArr()->put(newObj()->put("score_1st", a))->put(newObj()->put("score_1st", b)));
  if (giveup) req->put("intentional_give_up", "YES");
  return call(s, "CMD_GET_VSCOM_GAME_RESULT", req);
}
int main() {
  try {
    zs::setVerbose(false);
    {
      Value coaches(newArr()->put(newObj()->put("coach_id", newObj()->put("id", 17609097478250LL))
          ->put("ability_id", newArr()->put(11)->put(14)->put(7)->put(24)))
          ->put(newObj()->put("coach_id", newObj()->put("id", 17608560707469LL))
          ->put("ability_id", newArr()->put(7)->put(13)->put(17)->put(28)))
          ->put(newObj()->put("coach_id", newObj()->put("id", 123))
          ->put("ability_id", newArr()->put(7)->put(13)->put(17)->put(28))));
      adaptIosPlayerData(coaches, true);
      check(coaches.a->getJSONObject(0)->getJSONArray("ability_id")->getInt(3) == 12, "Conte second link matches native Coach.bin");
      check(coaches.a->getJSONObject(1)->getJSONArray("ability_id")->getInt(3) == 14, "Mourinho second link matches native Coach.bin");
      check(coaches.a->getJSONObject(2)->getJSONArray("ability_id")->getInt(3) == 28, "other coaches retain their link ID");
      adaptIosPlayerData(coaches, true);
      check(coaches.a->getJSONObject(0)->getJSONArray("ability_id")->getInt(3) == 12, "coach repair is idempotent");
    }
    // Independent table extracted from iOS Player::GetDataLabel(SkillCard),
    // VA 0x10578bc00. The native enum and APK enum have identical ordinals.
    const int iosSkillLabels[74] = {
      70,107,71,72,73,108,74,75,135,136,102,116,124,130,140,76,121,77,
      120,101,78,103,104,109,127,143,134,79,80,81,114,128,82,110,83,84,
      119,85,86,98,117,118,122,141,87,88,105,89,90,99,100,138,139,126,
      129,91,92,93,106,111,112,113,125,137,115,94,123,132,142,95,131,96,97,133
    };
    auto skillIds = newArr(); for (int i = 0; i < 74; ++i) skillIds->put(i);
    Value skillWire(newObj()->put("additional_skill_id", skillIds)->put("skillcard_id_list", skillIds)
        ->put("skill_training_result", newObj()->put("additional_skill_id", 67)));
    adaptIosPlayerData(skillWire, true);
    for (int i = 0; i < 74; ++i) {
      int target = skillWire.o->getJSONArray("additional_skill_id")->getInt(i);
      check(target == i, "skill ID is not shifted between APK and iOS");
      check(iosSkillLabels[target] == kZiruSkillParameters[i], "skill enum must refer to the same actual skill parameter");
      check(skillWire.o->getJSONArray("skillcard_id_list")->getInt(i) == target, "training list uses the same skill mapping");
    }
    check(skillWire.o->getJSONObject("skill_training_result")->getInt("additional_skill_id") == 67, "training result keeps native skill ID");
    adaptIosPlayerData(skillWire, false);
    for (int i = 0; i < 74; ++i) check(skillWire.o->getJSONArray("additional_skill_id")->getInt(i) == i, "all skill IDs roundtrip to stable save format");
    {
      auto params = newArr(); for (int i = 0; i < 149; ++i) params->put(0);
      params->put(71, 1); params->put(125, 1); params->put(142, 1); params->put(143, 1);
      Value owned(newObj()->put("gameplayer_id", newObj()->put("id", 4073))
          ->put("parameters", params)->put("additional_skill_id", newArr()));
      adaptIosPlayerData(owned, true);
      auto skills = owned.o->getJSONArray("skillcard_id_list");
      check(skills->length() == 2 && skills->getInt(0) == 2 && skills->getInt(1) == 62,
          "owned-player menu receives Double Touch and Long Reach Tackle by native ID");
      auto wire = owned.o->getJSONArray("parameters");
      check(wire->getInt(142) == 0 && wire->getInt(143) == 0,
          "booster ID one must not invent two extra skill flags");
      check(wire->getInt(144) == 1 && wire->getInt(145) == 1,
          "booster IDs survive skill-list reconstruction");
      check(kZiruSkillParameters[72] == 97 && kZiruSkillParameters[73] == 133,
          "the last two skill IDs retain their real match-parameter flags");
      check(!isZiruSkillParameter(kZiruSkillParameters[25]) && !isZiruSkillParameter(kZiruSkillParameters[68]),
          "reserved skill IDs never read saved booster values");
    }
    char temp[] = "/tmp/pes-gameplay-XXXXXX";
    check(mkdtemp(temp), "temporary directory");
    std::string root(temp);
    std::filesystem::create_directories(root + "/responses");
    Config cfg; cfg.assetsDir = root; cfg.dataDir = root + "/save";
    {
      Config stadiumCfg = cfg; stadiumCfg.downloadDir = root + "/stadiums";
      std::filesystem::create_directories(stadiumCfg.downloadDir);
      for (const char* ext : {"pak", "ucas", "utoc"})
        std::ofstream(stadiumCfg.downloadDir + "/pc3099_mobile_ios." + ext) << "custom";
      std::ofstream(stadiumCfg.downloadDir + "/pc3002_mobile_ios.pak") << "incomplete";
      OfflineServer server(stadiumCfg);
      auto list = call(server, "CMD_GET_STADIUM_DATA")->getJSONArray("data_list");
      check(list->length() == 3, "advertise complete installed stadiums only, including new IDs");
      check(list->getJSONObject(0)->getString("name") == "pc3099_mobile_ios.pak", "custom stadium ID discovered");
      check(list->getJSONObject(1)->getInt("size") == 6, "stadium metadata uses installed size");
      auto hash = list->getJSONObject(1)->getString("hash");
      std::ofstream(stadiumCfg.downloadDir + "/pc3099_mobile_ios.ucas") << "edited";
      list = call(server, "CMD_GET_STADIUM_DATA")->getJSONArray("data_list");
      check(list->getJSONObject(1)->getString("hash") != hash, "same-size replacement invalidates stadium hash");
    }
    {
      std::filesystem::create_directories(root + "/livedata");
      std::ofstream(root + "/livedata/pc9999_mobile_ios.ucas") << "native";
      std::ofstream(root + "/livedata/pc9999_mobile_and.ucas") << "old";
      std::ofstream(root + "/livedata/dt870_mobile_and.cpk") << "fixed";
      Config ios = cfg; ios.iosPlayerLayout = true; ios.dropAndroidLivedata = false; ios.livedataDir = root + "/livedata";
      OfflineServer server(ios);
      auto data = obj(R"({"livedata_info":{"livedata_list":[{"name":"pc9999_mobile_and.ucas","size":3},{"name":"dt870_mobile_and.cpk","size":99}],"version_file":"{\"dt880\":[{\"name\":\"pc9999\"}]}"}})");
      server.patchLocalLivedataUrls(data);
      auto info = data->getJSONObject("livedata_info");
      auto list = info->getJSONArray("livedata_list");
      auto version = Object::parse(info->getString("version_file"));
      check(list->getJSONObject(0)->getInt("size") == 6 && list->getJSONObject(0)->getString("hash").size() == 64,
          "livedata list uses actual native bytes, not Android metadata");
      check(version->getInt("dt870FileSize") == 5 && version->getString("dt870Hash") == list->getJSONObject(1)->getString("hash"),
          "repaired database hash is also updated in the version file");
      check(version->getJSONArray("dt880")->getJSONObject(0)->getInt("ucasFileSize") == 6,
          "native texture container size is also updated in the version file");
      auto agueroParams = newArr();
      for (int i = 0; i < 149; ++i) agueroParams->put(0);
      auto aguero = newObj()->put("gameplayer_id", newObj()->put("id", 88044682118054LL))
          ->put("parameters", agueroParams)->put("play_style", newArr()->put(1)->put(0));
      server.z27ApplyAppearanceAndClub(aguero);
      check(agueroParams->getInt(49) == 7 && agueroParams->getInt(50) == 1 && agueroParams->getInt(56) == 14,
          "Aguero's missing appearance comes from his known base preset");
      agueroParams->put(50, 9);
      server.z27ApplyAppearanceAndClub(aguero);
      check(agueroParams->getInt(50) == 9, "existing appearance customization is preserved");
    }
    auto p = obj(R"({"gameplayer_id":{"id":1,"serial":101},"level_info":{"level":5,"level_limit":5,"customize_point":0}})");
    auto parameters = newArr(); for (int i = 0; i < 145; ++i) parameters->put(i == 14 ? 70 : 0);
    p->put("parameters", parameters);
    p->put("initial_booster_id_list", newArr()->put(0)->put(0)->put(0));
    auto squad = newObj()->put("player_list", newArr()->put(newObj()->put("gameplayer_id", p->getJSONObject("gameplayer_id"))))
        ->put("coach", newArr()->put(coachRef(100, 1000)))->put("squad_gameplan", newObj());
    auto entry = newObj()->put("main_squad_id", 9)->put("gameplayer_list", newArr()->put(p))
        ->put("coach_list", newArr()->put(coachRef(100, 1000)->put("possession", 80))->put(coachRef(200, 2000)->put("possession", 90)))
        ->put("squad_list", newArr()->put(newObj()->put("squad_id", 9)->put("squad_data", squad)
        ->put("base_team", newArr()->put(newObj()->put("base_team_id", 108)))));
    asset(root, "CmdLogin.json", newObj()->put("entry_info", entry));
    asset(root, "CmdCreateUser.json", obj(R"({"user_code":"offline","auth_code":"local","session_id":"create"})"));
    asset(root, "ZIRU_PLAYER_RECORDS.json", newObj()->put("1", newObj()->put("gameplayer", p)));
    asset(root, "ZIRU_PACKS_CONFIG.json", obj(R"({"packs":[{"agent_id":91004,"player_ids":[1],"base_count":1}]})"));
    asset(root, "ZIRU_MATCH_TEAMS.json", obj(R"({"teams":{"108":{"base_team_id":108,"team_name":"Test","team_name_short":"TST","kits":[{"id":1,"shirt_rgb":[0,0,0]},{"id":2,"shirt_rgb":[255,255,255]}]}}})"));
    asset(root, "ZIRU_MATCH_PROFILES.json", obj(R"({"profiles":[{"profile_id":1,"player_ids":[1],"coordinates":[],"team_style":0,"base_team_id":108}]})"));
    {
      Config packCfg = cfg; packCfg.dataDir = root + "/pack-filter";
      std::filesystem::create_directories(packCfg.dataDir);
      OfflineServer server(packCfg);
      server.sp("almanac_state").edit().putString("ziru_remaining_91004", "[2,1]").commit();
      server.ziruLastAgentId = 91004;
      server.ziruLastCount = 1;
      auto response = newObj()->put("gameplayer_list", newArr()->put(
          newObj()->put("gameplayer", Object::parse(p->toString()))));
      server.applyZiruDynamicRecruit(response);
      check(response->getJSONArray("gameplayer_list")->getJSONObject(0)
              ->getJSONObject("gameplayer")->getJSONObject("gameplayer_id")->getLong("id") == 1,
          "a corrected pack ignores removed cards in a previously saved draw pool");
    }
    {
      Config skillCfg = cfg; skillCfg.dataDir = root + "/skill-display";
      std::filesystem::create_directories(skillCfg.dataDir);
      OfflineServer server(skillCfg);
      auto freshDetail = call(server, "CMD_GET_MYCLUB_PLAYER_DATA",
          newObj()->put("gameplayer_id", newObj()->put("id", 1)->put("serial", 101)));
      auto freshChoices = freshDetail->getJSONArray("skillcard_id_list");
      check(freshChoices->length() == 67,
          "empty skill slots offer verified skills rather than marking every choice learned");
      const int specialIds[] = {11, 12, 13, 16, 18, 24, 26, 30, 31, 36, 40,
          41, 42, 53, 54, 62, 63, 64, 66, 67, 70, 73};
      for (int special : specialIds) {
        check(isZiruRandomSpecialSkillId(special), "the APK special skill belongs to the random draw pool");
        bool offered = false;
        for (int i = 0; i < freshChoices->length(); ++i) offered |= freshChoices->getInt(i) == special;
        check(offered, "APK 1.3.2 special skill is selectable");
      }
      auto owned = Object::parse(p->toString());
      auto params = Array::parse(parameters->toString());
      params->put(14, 91); params->put(71, 1); params->put(142, 42);
      owned->put("parameters", params)->put("additional_skill_id", newArr()->put(25)->put(68));
      server.ziruDb->savePlayer(101, 1, owned->toString());
      auto detail = call(server, "CMD_GET_MYCLUB_PLAYER_DATA", newObj()->put("gameplayer_id", newObj()->put("id", 1)->put("serial", 101)));
      check(detail->getJSONArray("parameters")->getInt(14) == 91,
          "player-detail endpoint uses owned progression rather than catalog stats");
      auto choices = detail->getJSONArray("skillcard_id_list");
      bool hasInnate = false, hasLearned25 = false, hasLearned68 = false, hasAvailable = false;
      for (int i = 0; i < choices->length(); ++i) {
        int v = choices->getInt(i);
        hasInnate |= v == 2; hasLearned25 |= v == 25; hasLearned68 |= v == 68;
        hasAvailable |= v == 3;
      }
      check(choices->length() == 66 && !hasInnate && !hasLearned25 && !hasLearned68 && hasAvailable,
          "skill selector excludes only innate and learned skills for this player copy");
      auto extra = detail->getJSONArray("additional_skill_id");
      check(extra->getInt(0) == 25 && extra->getInt(1) == 68,
          "stored internal skill IDs 25/68 are not remapped a second time");
      check(detail->getJSONArray("initial_booster_id_list")->length() == 3,
          "player-detail endpoint always exposes booster slots");
      check(detail->optJSONArray("additional_position") != nullptr,
          "player-detail endpoint always exposes additional positions");
      auto player = server.z27Player(newObj()->put("id", 1)->put("serial", 101));
      check(player->getJSONArray("additional_skill_id")->getInt(0) == 25,
          "match player retains the same internal learned skill identity");
      check(player->getJSONArray("parameters")->getInt(142) == 42,
          "skill synchronization preserves booster data");

      auto trainee = Object::parse(p->toString());
      trainee->getJSONObject("gameplayer_id")->put("serial", 202);
      trainee->put("additional_skill_id", newArr());
      server.ziruDb->savePlayer(202, 1, trainee->toString());
      auto request = newObj()->put("gameplayer_id_list", newArr()->put(trainee->getJSONObject("gameplayer_id")))
          ->put("training_index", 0)->put("ability_id", 3)
          ->put("item_list", newArr()->put(newObj()->put("present_num", 100)
              ->put("item", newObj()->put("item_type", "SKILL_TRAINING_SELECT"))));
      server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(request)));
      auto trained = newObj();
      server.applyZiruSkillTrainingResponse(trained);
      check(trained->getJSONObject("skill_training_result")->getString("is_success") == "YES"
          && trained->getJSONArray("gameplayer_list")->getJSONObject(0)
              ->getJSONArray("additional_skill_id")->getInt(0) == 3,
          "100-card skill selection immediately returns the updated owned player");
      auto trainedDetail = call(server, "CMD_GET_MYCLUB_PLAYER_DATA",
          newObj()->put("gameplayer_id", newObj()->put("id", 1)->put("serial", 202)));
      check(trainedDetail->getJSONArray("skillcard_id_list")->length() == 66,
          "a selected skill is removed from available choices after saving");
      request->put("ability_id", iosSkillId(26, true));
      server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(request)));
      server.applyZiruSkillTrainingResponse(newObj());
      auto replaced = server.z27Player(newObj()->put("id", 1)->put("serial", 202));
      check(replaced->getJSONArray("additional_skill_id")->getInt(0) == 26
          && replaced->getJSONArray("parameters")->getInt(kZiruSkillParameters[26]) == 1
          && replaced->getJSONArray("parameters")->getInt(kZiruSkillParameters[3]) == 0,
          "replacing a per-copy skill removes its old match flag");
      auto untouched = server.z27Player(newObj()->put("id", 1)->put("serial", 101));
      check(untouched->getJSONArray("additional_skill_id")->getInt(0) == 25,
          "training one serial does not alter another copy of the same card");
      for (int skillId : specialIds) {
        auto copy = Object::parse(p->toString());
        int serial = 300 + skillId;
        copy->getJSONObject("gameplayer_id")->put("serial", serial);
        server.ziruDb->savePlayer(serial, 1, copy->toString());
        request->put("gameplayer_id_list", newArr()->put(copy->getJSONObject("gameplayer_id")))
            ->put("ability_id", iosSkillId(skillId, true));
        server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(request)));
        auto result = newObj();
        server.applyZiruSkillTrainingResponse(result);
        auto savedCopy = server.z27Player(newObj()->put("id", 1)->put("serial", serial));
        auto storedCopy = Object::parse(*server.loadPlayer(serial));
        Value wire(Object::parse(savedCopy->toString()));
        adaptIosPlayerData(wire, true);
        int pIdx = kZiruSkillParameters[skillId];
        int iosId = iosSkillId(skillId, true);
        check(result->getJSONObject("skill_training_result")->getString("is_success") == "YES"
            && savedCopy->getJSONArray("additional_skill_id")->getInt(0) == skillId
            && storedCopy->getJSONArray("additional_skill_id")->getInt(0) == skillId
            && (!isZiruSkillParameter(pIdx) || savedCopy->getJSONArray("parameters")->getInt(pIdx) == 1)
            && wire.o->getJSONArray("additional_skill_id")->getInt(0) == iosId
            && wire.o->getJSONArray("parameters")->getInt(iosSkillLabels[iosId]) == 1
            && savedCopy->getJSONArray("parameters")->getInt(142) == 0,
            "each APK 1.3.2 special skill saves on its exact player copy");
      }
    }
    {
      Config stableCfg = cfg; stableCfg.dataDir = root + "/stable-auto";
      std::filesystem::create_directories(stableCfg.dataDir);
      OfflineServer server(stableCfg);
      auto owned = Object::parse(p->toString());
      owned->getJSONObject("gameplayer_id")->put("serial", 505);
      owned->getJSONArray("parameters")->put(14, 91);
      server.ziruDb->savePlayer(505, 1, owned->toString());
      server.ziruDb->saveProgression(505, newObj()->put("SHOOT", 8)
          ->put("_spent_total", 8)->toString());
      auto loaded = server.refreshZiruGameplayerFromCanonical(owned);
      check(loaded->getJSONArray("parameters")->getInt(14) == 91,
          "loading a saved Auto plan does not rebuild or alter its attributes");
    }
    {
      Config boosterCfg = cfg; boosterCfg.dataDir = root + "/booster-state";
      std::filesystem::create_directories(boosterCfg.dataDir);
      OfflineServer server(boosterCfg);
      auto legacy = Object::parse(p->toString());
      legacy->getJSONObject("gameplayer_id")->put("serial", 606);
      legacy->getJSONArray("initial_booster_id_list")->put(1, 64);
      legacy->getJSONArray("parameters")->put(143, 64);
      server.ziruDb->savePlayer(606, 1, legacy->toString());
      auto recovered = server.refreshZiruGameplayerFromCanonical(legacy);
      check(recovered->getJSONArray("initial_booster_id_list")->getInt(1) == 0
          && recovered->getJSONArray("parameters")->getInt(143) == 64,
          "legacy crafted booster remains active while native slot eligibility is restored");
      auto persisted = Object::parse(*server.loadPlayer(606));
      check(persisted->getJSONArray("initial_booster_id_list")->getInt(1) == 0,
          "legacy booster migration persists across restart");
      auto detail = call(server, "CMD_GET_MYCLUB_PLAYER_DATA",
          newObj()->put("gameplayer_id", newObj()->put("id", 1)->put("serial", 606)));
      check(detail->getJSONArray("parameters")->getInt(143) == 64,
          "owned booster is shown on the player-detail screen");

      auto drawn = Object::parse(p->toString());
      drawn->getJSONObject("gameplayer_id")->put("serial", 607);
      server.ziruDb->savePlayer(607, 1, drawn->toString());
      auto req = newObj()->put("gameplayer_id_list", newArr()->put(drawn->getJSONObject("gameplayer_id")))
          ->put("training_index", 1)->put("ability_id", 65)
          ->put("item_list", newArr()->put(newObj()->put("present_num", 100)
              ->put("item", newObj()->put("item_type", "ABILITY_TRAINING_SELECT"))));
      server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(req)));
      auto response = newObj();
      server.applyZiruBoosterTrainingResponse(response);
      check(response->getJSONObject("ability_training_result")->getString("is_success") == "YES"
          && response->getJSONArray("gameplayer_list")->getJSONObject(0)->getJSONArray("parameters")->getInt(143) == 65,
          "newly drawn player gets the selected booster and updated UI state");
      persisted = Object::parse(*server.loadPlayer(607));
      check(persisted->getJSONArray("initial_booster_id_list")->getInt(1) == 0,
          "selected booster never changes immutable native card metadata");
      req->put("ability_id", 66);
      server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(req)));
      response = newObj();
      server.applyZiruBoosterTrainingResponse(response);
      check(response->getJSONObject("ability_training_result")->getString("is_success") == "NO"
          && Object::parse(*server.loadPlayer(607))->getJSONArray("parameters")->getInt(143) == 65,
          "a used booster slot cannot be overwritten by another 100-card selection");
      auto precise = Object::parse(p->toString());
      precise->getJSONObject("gameplayer_id")->put("serial", 608);
      server.ziruDb->savePlayer(608, 1, precise->toString());
      req->put("gameplayer_id_list", newArr()->put(precise->getJSONObject("gameplayer_id")))
          ->put("ability_id", 239); // native catalog's Precise +6 full ID
      server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(req)));
      response = newObj();
      server.applyZiruBoosterTrainingResponse(response);
      check(response->getJSONObject("ability_training_result")->getString("is_success") == "YES"
          && Object::parse(*server.loadPlayer(608))->getJSONArray("parameters")->getInt(143) == 239,
          "Precise +6 saves its full booster ID on the exact player copy");
      req->put("ability_id", 1023);
      auto invalid = Object::parse(p->toString());
      invalid->getJSONObject("gameplayer_id")->put("serial", 609);
      server.ziruDb->savePlayer(609, 1, invalid->toString());
      req->put("gameplayer_id_list", newArr()->put(invalid->getJSONObject("gameplayer_id")));
      server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(req)));
      response = newObj();
      server.applyZiruBoosterTrainingResponse(response);
      check(response->getJSONObject("ability_training_result")->getString("is_success") == "NO"
          && Object::parse(*server.loadPlayer(609))->getJSONArray("parameters")->getInt(143) == 0,
          "unknown booster IDs cannot corrupt an unused slot");
    }
    {
      OfflineServer server(cfg);
      auto player = Object::parse(p->toString());
      player->getJSONObject("gameplayer_id")->put("serial", 404);
      player->getJSONArray("parameters")->put(0, 1);
      server.ziruDb->savePlayer(404, 1, player->toString());
      auto item = newObj()->put("gameplayer_id_list", newArr()->put(player->getJSONObject("gameplayer_id")))
          ->put("training_index", 0)->put("position_id", 2)
          ->put("item_list", newArr()->put(newObj()->put("present_num", 1)
              ->put("position_id", 1)
              ->put("item", newObj()->put("item_type", "POSITION_TRAINING_SELECT")->put("position_id", 1))));
      server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(item)));
      auto response = newObj();
      server.applyZiruPositionTrainingResponse(response);
      check(response->getJSONObject("position_training_result")->getString("additional_position") == "CB",
          "root selection wins over generic GK metadata in the 100-card item");
      check(response->getJSONArray("gameplayer_list")->getJSONObject(0)
          ->getJSONArray("additional_position")->getString(0) == "CB",
          "position training returns the updated player to the iOS screen");
      check(Object::parse(*server.loadPlayer(404))->getJSONArray("parameters")->getInt(10) == 2,
          "selected position persists to the player save");
      auto items = response->getJSONArray("item_list");
      check(items->length() == 1 && items->getJSONObject(0)->getInt("inventory_num") == 999999,
          "selected-position inventory remains available after use");
      item->put("position_id", "CMF")->put("training_index", 1);
      server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(item)));
      response = newObj();
      server.applyZiruPositionTrainingResponse(response);
      check(response->getJSONObject("position_training_result")->getString("additional_position") == "CMF",
          "APK-compatible string position selection never falls back to GK");
      item->put("position_id", 0);
      server.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(item)));
      response = newObj();
      server.applyZiruPositionTrainingResponse(response);
      check(response->getJSONObject("position_training_result")->getString("is_success") == "NO",
          "a missing selected position fails instead of silently assigning a random position");
      server.ziruDb->deletePlayerState(404);
    }
    {
      OfflineServer server(cfg);
      auto messi = Object::parse(p->toString());
      messi->getJSONObject("gameplayer_id")->put("id", 89129161334103LL);
      server.applyZiruMessiElite(messi, true, 70);
      check(messi->getJSONArray("parameters")->getInt(18) == 99 &&
          messi->getJSONObject("level_info")->getInt("customize_point") == 70,
          "Messi's earlier elite profile and spare budget are restored");
      messi->getJSONArray("parameters")->put(18, 94);
      server.applyZiruMessiElite(messi, false, 58);
      check(messi->getJSONArray("parameters")->getInt(18) == 94 &&
          messi->getJSONObject("level_info")->getInt("customize_point") == 58,
          "later manual Messi edits remain intact with the correct remaining points");
      server.ziruDb->saveProgression(555, newObj()->put("_spent_total", 163)->toString());
      check(server.ziruMessiBudget(555) == 233,
          "legacy Messi's 163 spent points remain valid alongside one fixed spare pool");
      server.ziruDb->saveProgression(555, newObj()->put("_spent_total", 180)->toString());
      check(server.ziruMessiBudget(555) == 233,
          "editing Messi does not create another spare pool on each save");
    }
    asset(root, "ZIRU_GET_VSCOM_OPPONENT.json", obj(R"({"squad_data":{"squad_gameplan":{"formation":[],"match_plan_settings":{}}},"user_info":[{}]})"));
    auto start = obj(R"({"squad_data_home":{"coach":{"coach_id":{"id":999,"serial":999}}},"commentary_info":{}})");
    asset(root, "ZIRU_START_GAME.json", start); asset(root, "ZIRU_ML_START_GAME.json", start);
    asset(root, "ZIRU_GET_GAMERESULT_LIST.json", obj(R"({"game_result_list":[{"player":[{},{}]}]})"));
    asset(root, "ZIRU_GET_VSCOM_GAME_RESULT_TEMPLATE.json", obj(R"({"game_end_result":{"match_result":{"player_data":{}},"extra_match_result":{"league_match_result":{"league_info":{"com_level_info":{},"round_match_result":{}}}}}})"));
    asset(root, "cmd_get_league_com_info.json", obj(R"({"league_info":{"com_level_info":{},"round_match_result":{}}})"));
    asset(root, "generic.json", obj(R"({"result":"NOERR"})"));
    {
      OfflineServer s(cfg);
      // The iOS career-plan reply must raise the same parameter slots as the
      // efootxi 1.3.2 native allocation table, including the goalkeeper rows.
      auto nativeBase = newArr();
      for (int i = 0; i < 147; ++i) nativeBase->put(50);
      auto nativePlan = newObj()->put("QUICKNESS", 1)->put("AERIAL", 1)
          ->put("GK1", 1)->put("GK2", 1)->put("GK3", 1);
      auto nativeResult = Array::parse(nativeBase->toString());
      s.z27ApplyProgressionPoints(nativeResult, nativePlan, nativeBase);
      for (int i = 14; i <= 40; ++i) {
        const bool quick = i == 14 || i == 38 || i == 36;
        const bool aerial = i == 23 || i == 39 || i == 35;
        const bool gk1 = i == 16 || i == 39;
        const bool gk2 = i == 30 || i == 32;
        const bool gk3 = i == 29 || i == 31;
        check(nativeResult->getInt(i) == 50 + (int)quick + (int)aerial +
              (int)gk1 + (int)gk2 + (int)gk3,
              "Auto progression uses native efootxi ability slots");
      }
      auto d10 = Object::parse(p->toString());
      auto d1 = Object::parse(p->toString());
      auto base = d10->getJSONArray("parameters");
      for (int i = 14; i <= 40; i++) base->put(i, 50);
      d1->put("parameters", Array::parse(base->toString()));
      d10->getJSONObject("level_info")->put("level_limit", 35);
      d1->getJSONObject("level_info")->put("level_limit", 35);
      s.z27ProgressOpponentPlayer(d10, 10);
      s.z27ProgressOpponentPlayer(d1, 1);
      check(d10->getJSONArray("parameters")->toString() == d1->getJSONArray("parameters")->toString(),
          "all Divisions receive the same full opponent progression budget");
      check(d10->getJSONObject("level_info")->getInt("level") == 35,
          "opponent level is raised to the card limit");
      check(d10->getJSONArray("parameters")->getInt(33) == 50 &&
            p->getJSONArray("parameters")->getInt(33) != d10->getJSONArray("parameters")->getInt(33),
          "opponent progression does not add a flat bonus to unrelated attributes");
      auto goalkeeper = Object::parse(p->toString());
      goalkeeper->getJSONArray("parameters")->put(0, 0);
      goalkeeper->getJSONObject("level_info")->put("level_limit", 35);
      int before = goalkeeper->getJSONArray("parameters")->getInt(29);
      s.z27ProgressOpponentPlayer(goalkeeper, 10);
      check(goalkeeper->getJSONArray("parameters")->getInt(29) > before,
          "zero-based GK position receives goalkeeper progression");
      auto fixedPotw = Object::parse(p->toString());
      auto fixedParams = fixedPotw->getJSONArray("parameters");
      for (int i = 14; i <= 40; i++) fixedParams->put(i, 80);
      fixedPotw->getJSONObject("level_info")->put("level", 1)->put("level_limit", 1);
      auto fixedBefore = fixedParams->toString();
      s.z27ProgressOpponentPlayer(fixedPotw, 1);
      check(fixedPotw->getJSONArray("parameters")->toString() == fixedBefore,
          "fixed level-one POTW card receives no phantom AI progression");
    }
    {
      OfflineServer s(cfg);
      auto a = call(s, "CMD_CREATE_MYCLUB_SQUAD", newObj()->put("name", "First tactic"))->getJSONObject("new_squad_data");
      auto b = call(s, "CMD_CREATE_MYCLUB_SQUAD", newObj()->put("name", "Second tactic"))->getJSONObject("new_squad_data");
      check(a->getInt("squad_id") != b->getInt("squad_id"), "creating two tactics must allocate distinct IDs");
      check(a->getString("name") == "First tactic" && a->optJSONObject("squad_data") && a->getJSONArray("base_team")->length() == 1,
          "create result matches native MyclubSquad wrapper");
      check(Object::parse(*s.loadSquad(a->getInt("squad_id")))->getString("squad_name") == "First tactic",
          "second create must preserve first tactic");
    }
    {
      OfflineServer s(cfg);
      check(s.z27State()->getInt("division") == 10, "starts at D10 by default");
      s.ziruMyLeagueSetActive(1);
      call(s, "CMD_GET_LEAGUE_COM_INFO");
      check(!s.ziruMyLeagueIsActive(), "entering AI rank exits a previous MyLeague session");
      s.ziruDb->saveMeta("z27_division_state", R"({"division":10,"best":10,"point":0,"matches":0,"win":0,"draw":0,"lose":0,"sequence":0,"completed":{}})");
      s.ziruDb->saveMeta("z27_div10_reset_v3", "1");
      auto first = call(s, "CMD_GET_VSCOM_OPPONENT"); auto id = first->getLong("game_id");
      check(coachId(call(s, "CMD_START_GAME", newObj()->put("game_id", (long long)id))) == 100, "selected coach on first start");
      call(s, "CMD_SET_MYCLUB_SQUAD_INFO", newObj()->put("squad_id", 9)->put("squad_data", newObj()->put("coach", newArr()->put(coachRef(200, 2222)))));
      auto retry = call(s, "CMD_START_GAME", newObj()->put("game_id", (long long)id));
      check(coachId(retry) == 200, "coach changes must invalidate same-kit snapshot");
      check(retry->getJSONObject("squad_data_home")->getJSONObject("coach")->getJSONObject("coach_id")->getInt("serial") == 2222, "owned coach serial preserved");
      check(coachId(call(s, "CMD_START_GAME", newObj()->put("game_id", (long long)id))) == 200, "idempotent start");
      s.ziruLastRequestBody = zc::encodeRequest(Value(newObj()->put("rqid", 42)));
      Value ml; check(zc::decodeResponse(s.handleZiruMyLeagueStartGameV71().body, ml) && coachId(ml.o) == 200, "MyLeague default uses owned coach");
      auto special = newObj()->put("squad_data", newObj()->put("coach", newArr()->put(coachRef(100, 3333))));
      s.hydrateZiruMyLeagueCoachV73(special);
      check(special->getJSONObject("squad_data")->getJSONArray("coach")->getJSONObject(0)->getJSONObject("coach_id")->getInt("serial") == 3333, "MyLeague serial preserved");
      auto win1 = result(s, id, 1, 0);
      check(s.z27State()->getInt("point") == 3 && s.z27State()->getInt("division") == 10, "first win");
      result(s, id, 1, 0); check(s.z27State()->getInt("point") == 3, "duplicate result does not count twice");
      id = call(s, "CMD_GET_VSCOM_OPPONENT")->getLong("game_id"); call(s, "CMD_START_GAME");
      auto win2 = result(s, id, 2, 0);
      check(s.z27State()->getInt("division") == 10 && s.z27State()->getInt("point") == 6, "three-match division retains six points after two wins");
      id = call(s, "CMD_GET_VSCOM_OPPONENT")->getLong("game_id"); call(s, "CMD_START_GAME");
      auto win3 = result(s, id, 1, 0);
      check(s.z27State()->getInt("division") == 9, "three wins promote after the third match");
      check(win3->getJSONObject("game_end_result")->getJSONObject("extra_match_result")->getJSONObject("league_match_result")->getJSONObject("league_info")->getString("is_promoted") == "YES", "promotion flag");
      auto info = call(s, "CMD_GET_LEAGUE_COM_INFO")->getJSONObject("league_info");
      check(info->getInt("current_division") == 9 && info->getInt("current_point") == 0, "menu reflects committed rank");
      auto login = call(s, "CMD_LOGIN")->getJSONObject("entry_info")->getJSONArray("gameplayer_list")->getJSONObject(0);
      check(login->getJSONObject("level_info")->getInt("customize_point") == 8, "Auto receives only earned level points");
      auto trained = Array::parse(parameters->toString()); trained->put(14, 80);
      auto use = newObj()->put("gameplayer_id", p->getJSONObject("gameplayer_id"))->put("parameters", trained)
          ->put("use_list", newArr()->put(newObj()->put("parameter_type", "SHOOT")->put("point", 6)));
      auto spent = call(s, "CMD_USE_MYCLUB_CAREERPLAN", newObj()->put("gameplayer_use_list", newArr()->put(use)))
          ->getJSONArray("careerplan_result_list")->getJSONObject(0)->getJSONObject("gameplayer");
      check(spent->getJSONObject("level_info")->getInt("customize_point") == 2, "Auto cannot spend phantom bonus points");
      int spentTotal = 0;
      for (int idx = 14; idx <= 40; ++idx) if (idx != 33)
        spentTotal += spent->getJSONArray("parameters")->getInt(idx);
      check(spentTotal == 70,
          "finished catalog card redistributes points without increasing its total ratings");
    }
    {
      OfflineServer s(cfg);
      check(s.z27State()->getInt("division") == 9, "rank survives process restart");
      auto p2 = call(s, "CMD_LOGIN")->getJSONObject("entry_info")->getJSONArray("gameplayer_list")->getJSONObject(0);
      int resumedTotal = 0;
      for (int idx = 14; idx <= 40; ++idx) if (idx != 33)
        resumedTotal += p2->getJSONArray("parameters")->getInt(idx);
      check(p2->getJSONObject("level_info")->getInt("customize_point") == 2 && resumedTotal == 70,
          "training and remaining balance survive restart/login");
      call(s, "CMD_LOGIN"); check(s.refreshZiruGameplayerFromCanonical(p2)->getJSONObject("level_info")->getInt("customize_point") == 2, "earned points are not added repeatedly");
      auto id = call(s, "CMD_GET_VSCOM_OPPONENT")->getLong("game_id"); call(s, "CMD_START_GAME");
      result(s, id, 0, 0, true); check(s.z27State()->getInt("point") == 0, "give-up is not a draw point");
      auto reset = newObj()->put("gameplayer_id", p->getJSONObject("gameplayer_id"))->put("use_list", newArr());
      auto p3 = call(s, "CMD_USE_MYCLUB_CAREERPLAN", newObj()->put("gameplayer_use_list", newArr()->put(reset)))
          ->getJSONArray("careerplan_result_list")->getJSONObject(0)->getJSONObject("gameplayer");
      check(p3->getJSONObject("level_info")->getInt("customize_point") == 8 && p3->getJSONArray("parameters")->getInt(14) == 70, "reset refunds only earned points");
      auto useReal = newObj()->put("gameplayer_id", p->getJSONObject("gameplayer_id"))
          ->put("use_list", newArr()->put(newObj()->put("parameter_type", "STRENGTH")->put("point", 4)));
      auto p4 = call(s, "CMD_USE_MYCLUB_CAREERPLAN", newObj()->put("gameplayer_use_list", newArr()->put(useReal)))
          ->getJSONArray("careerplan_result_list")->getJSONObject(0)->getJSONObject("gameplayer");
      check(p4->getJSONObject("level_info")->getInt("customize_point") == 4, "four earned points spent");
      int redistributedTotal = 0;
      for (int idx = 14; idx <= 40; ++idx) if (idx != 33)
        redistributedTotal += p4->getJSONArray("parameters")->getInt(idx);
      check(redistributedTotal == 70,
          "subsequent Auto allocation remains inside the finished card's native total");
      auto entryTest = call(s, "CMD_SET_MYCLUB_ENTRY_INFO", newObj()->put("base_team_id", 262143)->put("team_name", "Entry Team"));
      check(entryTest->getJSONArray("base_team")->getJSONObject(0)->getString("base_team_license") == "UNKNOWN", "entry info preserves unknown license instead of fabricating licensed assets");
    }
    // iOS wire layout, resume through CREATE_USER, and stale difficulty migration.
    cfg.iosPlayerLayout = true;
    {
      OfflineServer s(cfg);
      auto saved = Object::parse(*s.loadPlayer(101));
      saved->put("initial_booster_id_list", newArr()->put(12)->put(0)->put(0));
      auto par = saved->getJSONArray("parameters"); par->put(142, 12)->put(143, 77)->put(144, 0);
      s.ziruDb->savePlayer(101, 1, saved->toString());
      auto extra = Object::parse(saved->toString());
      extra->getJSONObject("gameplayer_id")->put("serial", 202);
      s.ziruDb->savePlayer(202, 1, extra->toString());
      auto wire = call(s, "CMD_LOGIN")->getJSONObject("entry_info")->getJSONArray("gameplayer_list")->getJSONObject(0)->getJSONArray("parameters");
      check(wire->length() == 147 && wire->getInt(142) == 0 && wire->getInt(143) == 0 && wire->getInt(144) == 12 && wire->getInt(145) == 77, "iOS booster slots are 144..146, not skill bits 142/143");
      check(Object::parse(*s.loadPlayer(101))->getJSONArray("parameters")->length() == 145, "wire translation must not mutate saved layout");
      auto use = newObj()->put("gameplayer_id", p->getJSONObject("gameplayer_id"))->put("parameters", wire)
          ->put("use_list", newArr()->put(newObj()->put("parameter_type", "STRENGTH")->put("point", 4)));
      auto trained = call(s, "CMD_USE_MYCLUB_CAREERPLAN", newObj()->put("gameplayer_use_list", newArr()->put(use)))
          ->getJSONArray("careerplan_result_list")->getJSONObject(0)->getJSONObject("gameplayer")->getJSONArray("parameters");
      check(trained->length() == 147 && trained->getInt(144) == 12 && trained->getInt(145) == 77, "iOS request roundtrip preserves trained boosters");
      auto base = Object::parse(p->toString());
      auto original = base->getJSONArray("parameters");
      while (original->length() < 149) original->put(0);
      base->put("initial_booster_id_list", newArr()->put(12)->put(0)->put(0));
      asset(root, "ZIRU_PLAYER_RECORDS.json", newObj()->put("1", newObj()->put("gameplayer", base)));
      s.ziruPlayerRecordsCache.reset();
      auto pd = call(s, "CMD_GET_MYCLUB_PLAYER_DATA", newObj()->put("gameplayer_id", p->getJSONObject("gameplayer_id")))->getJSONArray("original_parameters");
      check(pd->length() == 147 && pd->getInt(144) == 12 && pd->getInt(145) == 77, "149-element canonical records preserve owned booster slots");
      s.ziruDb->saveMeta("z27_division_state", R"({"division":3,"best":3,"point":7,"rating":1350,"matches":2,"win":2,"draw":1,"lose":0,"sequence":0,"completed":{}})");
      auto active = s.z27Meta("z27_active_match");
      active->put("division", 10)->put("cpu_level", "NORMAL");
      s.ziruDb->saveMeta("z27_active_match", active->toString());
      check(s.z27State()->getInt("division") == 3 && s.z27State()->getInt("point") == 1350, "legacy rank and rating survive rule migration");
      auto pendingId = active->getLong("game_id");
      asset(root, "ZIRU_MATCH_PROFILES.json", obj(R"({"profiles":[{"profile_id":1,"player_ids":[1],"coordinates":[],"team_style":1,"base_team_id":108}]})"));
      auto selected = call(s, "CMD_GET_VSCOM_OPPONENT");
      check(selected->getString("cpu_level") == "LEGEND" && s.z27Active()->getInt("division") == 3, "cached opponent follows preserved division at maximum CPU level");
      check(s.z27Active()->getLong("game_id") == pendingId &&
            s.z27Active()->getJSONObject("profile")->getInt("team_style") == 1 &&
            s.z27State()->getInt("point") == 1350,
            "new catalog refreshes the pending opponent without erasing its game ID or Division rating");
      active = s.z27Meta("z27_active_match");
      active->put("division", 10)->put("cpu_level", "NORMAL");
      s.ziruDb->saveMeta("z27_active_match", active->toString());
      check(call(s, "CMD_GET_VSCOM_OPPONENT")->getString("cpu_level") == "LEGEND", "already-migrated saves maintain maximum CPU level");
      auto ratedState = s.z27State();
      ratedState->getJSONObject("completed")->put(str(s.z27Active()->getLong("game_id")), true);
      s.ziruDb->saveMeta("z27_division_state", ratedState->toString());
      call(s, "CMD_GET_VSCOM_OPPONENT");
      auto ratedId = s.z27Active()->getLong("game_id");
      call(s, "CMD_START_GAME");
      result(s, ratedId, 1, 0);
      check(s.z27State()->getInt("point") == 1380 && s.z27State()->getInt("division") == 3,
            "Division 3 win adds 30 rating without resetting the save");
      sqlite3_exec(s.ziruDb->db, "PRAGMA query_only=ON", nullptr, nullptr, nullptr);
      bool failed = false;
      try { s.ziruDb->saveMeta("must_not_succeed", "1"); } catch (const std::exception&) { failed = true; }
      check(failed, "write failure must not be acknowledged as saved");
      sqlite3_exec(s.ziruDb->db, "PRAGMA query_only=OFF", nullptr, nullptr, nullptr);
    }
    {
      OfflineServer s(cfg);
      auto resumed = call(s, "CMD_CREATE_USER")->getJSONObject("entry_info");
      check(resumed->getJSONArray("gameplayer_list")->length() == 2, "CREATE_USER resumes recruits after process restart");
      check(resumed->getJSONArray("squad_list")->length() > 0, "CREATE_USER resumes saved squads");
      auto owned = Object::parse(*s.loadPlayer(101));
      owned->getJSONObject("gameplayer_id")->put("serial", 303);
      auto params = owned->getJSONArray("parameters");
      for (int id = 0; id < 72; ++id) params->put(kZiruSkillParameters[id], id == 11 ? 0 : 1);
      owned->put("additional_skill_id", newArr()->put(72)->put(73));
      s.ziruDb->savePlayer(303, 1, owned->toString());
      asset(root, "CMD_USE_MYCLUB_ITEM.json", obj(R"({"result":"NOERR"})"));
      auto req = newObj()->put("gameplayer_id_list", newArr()->put(owned->getJSONObject("gameplayer_id")))
          ->put("training_index", 2)->put("item_list", newArr()->put(newObj()->put("present_num", 1)
          ->put("item", newObj()->put("item_type", "SKILL_TRAINING"))));
      auto itemResp = call(s, "CMD_USE_MYCLUB_ITEM", req);
      auto awarded = itemResp->getJSONObject("skill_training_result")->getInt("additional_skill_id");
      check(awarded == 11, "random skill training uses only the unlearned special-skill pool");
      check(itemResp->getJSONArray("item_list")->length() > 0, "item_list replenished in CMD_USE_MYCLUB_ITEM");
    }
    {
      OfflineServer s(cfg);
      auto players = call(s, "CMD_LOGIN")->getJSONObject("entry_info")->getJSONArray("gameplayer_list");
      bool retainedSpecial = false;
      for (int i = 0; i < players->length(); ++i) {
        auto p = players->getJSONObject(i);
        if (p->getJSONObject("gameplayer_id")->getLong("serial") == 303) {
          auto skills = p->getJSONArray("additional_skill_id");
          retainedSpecial = skills->length() == 3 && skills->getInt(2) == 11;
          check(p->getJSONArray("parameters")->getInt(kZiruSkillParameters[11]) == 1, "random special skill parameter bit set in roster");
        }
      }
      check(retainedSpecial, "random special skill survives save, process restart and login");

      // Verify the random special skill survives z27Restore via CMD_GET_MYCLUB_ENTRY_INFO.
      auto entryResp = call(s, "CMD_GET_MYCLUB_ENTRY_INFO");
      auto entryPlayers = entryResp->getJSONObject("entry_info")->getJSONArray("gameplayer_list");
      bool entryHasSkill = false;
      for (int i = 0; i < entryPlayers->length(); ++i) {
        auto p = entryPlayers->getJSONObject(i);
        if (p->getJSONObject("gameplayer_id")->getLong("serial") == 303) {
          auto skills = p->optJSONArray("additional_skill_id");
          if (skills && skills->length() == 3 && skills->getInt(2) == 11) entryHasSkill = true;
          check(p->getJSONArray("parameters")->getInt(kZiruSkillParameters[11]) == 1, "special skill parameter bit survives post-match entry info");
        }
      }
      check(entryHasSkill, "additional skill survives post-match entry info");
    }
    {
      OfflineServer s(cfg);
      auto req = newObj()->put("achievement_item_info_list", newArr()->put(newObj()->put("achievement_id", 101)->put("sheet_id", 1)->put("level", 1)));
      auto resp = call(s, "CMD_RECV_MYCLUB_ACHIEVEMENT", req);
      check(resp->getJSONArray("achievement_list")->length() > 0, "achievement_list returned");
      check(resp->getJSONArray("receive_result_list")->length() > 0, "receive_result_list returned");
      check(resp->getJSONArray("receive_result_list")->getJSONObject(0)->getString("is_success") == "YES", "reward claimed successfully");
    }
    std::filesystem::create_directories(root + "/kill-save");
    pid_t child = fork();
    check(child >= 0, "spawn abrupt-exit save test");
    if (child == 0) {
      zs::ZiruDb db(root + "/kill-save/save.db");
      db.saveMeta("progress", "retained");
      zs::Prefs prefs(root + "/kill-save/prefs.json");
      prefs.edit().putString("recruited_players", "[1,2]").commit();
      _exit(0); // no C++ destructors, matching process termination after saving
    }
    int status = 0;
    check(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0, "abrupt-exit child saved");
    {
      zs::ZiruDb db(root + "/kill-save/save.db"); std::string progress;
      zs::Prefs prefs(root + "/kill-save/prefs.json");
      check(db.loadMeta("progress", progress) && progress == "retained" && prefs.getString("recruited_players", "") == "[1,2]", "DB and recruits survive exit without destructors");
    }
    std::filesystem::remove_all(root);
    std::cout << "PASS: gameplay, booster layout, all 74 skill mappings and 22 special-skill selection/training/match persistence, CREATE_USER resume, D1 migration, failed writes and abrupt-exit persistence\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << "\n"; return 1; }
}

