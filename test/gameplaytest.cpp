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
    // VA 0x10578bc00. Compare semantic parameter labels for every source skill.
    const int iosSkillLabels[74] = {
      70,107,71,72,73,108,74,75,135,136,102,116,124,130,140,76,121,77,
      120,101,78,103,104,109,127,143,134,79,80,81,114,128,82,110,83,84,
      119,85,86,98,117,118,122,141,87,88,105,89,90,99,100,138,139,126,
      129,91,92,93,106,111,112,113,125,137,115,94,123,132,142,95,131,96,97,133
    };
    auto skillIds = newArr(); for (int i = 0; i < 72; ++i) skillIds->put(i);
    Value skillWire(newObj()->put("additional_skill_id", skillIds)->put("skillcard_id_list", skillIds)
        ->put("skill_training_result", newObj()->put("additional_skill_id", 67)));
    adaptIosPlayerData(skillWire, true);
    for (int i = 0; i < 72; ++i) {
      int target = skillWire.o->getJSONArray("additional_skill_id")->getInt(i);
      check(iosSkillLabels[target] == kZiruSkillParameters[i], "skill enum must refer to the same actual skill parameter");
      check(skillWire.o->getJSONArray("skillcard_id_list")->getInt(i) == target, "training list uses the same skill mapping");
    }
    check(skillWire.o->getJSONObject("skill_training_result")->getInt("additional_skill_id") == 69, "training result maps skill scalar");
    adaptIosPlayerData(skillWire, false);
    for (int i = 0; i < 72; ++i) check(skillWire.o->getJSONArray("additional_skill_id")->getInt(i) == i, "all skill IDs roundtrip to stable save format");
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
      // Catalog records mark Neymar variants as type NORMAL even when their
      // full IDs identify Epic/Big Time cards. Keep the full ID for card art.
      auto neymarParams = newArr();
      for (int i = 0; i < 149; ++i) neymarParams->put(0);
      constexpr int64_t neymarEpicId = 89071179308448LL;
      auto neymar = newObj()->put("gameplayer_id", newObj()->put("id", (long long)neymarEpicId))
          ->put("type", "NORMAL")->put("parameters", neymarParams);
      server.z27ApplyAppearanceAndClub(neymar);
      check(neymar->getJSONObject("gameplayer_id")->getLong("id") == neymarEpicId,
          "Neymar Epic/Big Time card keeps its full variation ID");
    }
    auto p = obj(R"({"gameplayer_id":{"id":1,"serial":101},"level_info":{"level":1,"level_limit":5,"customize_point":0}})");
    auto alreadyDeveloped = Object::parse(p->toString());
    alreadyDeveloped->getJSONObject("level_info")->put("level", 5);
    check(OfflineServer::ziruUnspentNativePoints(alreadyDeveloped) == 0,
        "max-level source card with no unspent points must not be developed twice");
    alreadyDeveloped->getJSONObject("level_info")->put("customize_point", 2);
    check(OfflineServer::ziruUnspentNativePoints(alreadyDeveloped) == 2,
        "max-level source card keeps only its native unspent points");
    {
      auto messiParams = newArr();
      for (int i = 0; i < 145; ++i) messiParams->put(i == 18 ? 101 : 50);
      auto messi = newObj()->put("gameplayer_id", newObj()->put("id", 89138556575063LL))
          ->put("parameters", messiParams)
          ->put("level_info", newObj()->put("level", 1)->put("level_limit", 32)->put("customize_point", 62));
      OfflineServer::applyZiruMessiMaxProgression(messi);
      check(messiParams->getInt(14) == 98 && messiParams->getInt(17) == 99 &&
          messiParams->getInt(18) == 101 && messiParams->getInt(20) == 97 &&
          messiParams->getInt(28) == 99 && messiParams->getInt(36) == 97,
          "every Messi variation gets elite playmaking stats without losing stronger saved values");
      auto ml = messi->getJSONObject("level_info");
      check(ml->getInt("level") == 36 && ml->getInt("level_limit") == 36 &&
          ml->getInt("customize_point") == 70,
          "Messi retains the same spare-point pool as Ronaldo");
      auto unrelated = newObj()->put("gameplayer_id", newObj()->put("id", 1LL))
          ->put("parameters", newArr()->put(12))
          ->put("level_info", newObj()->put("customize_point", 5));
      OfflineServer::applyZiruMessiMaxProgression(unrelated);
      check(unrelated->getJSONObject("level_info")->getInt("customize_point") == 5,
          "Messi override leaves other players unchanged");
    }
    auto parameters = newArr(); for (int i = 0; i < 145; ++i) parameters->put(i == 14 ? 70 : 0);
    p->put("parameters", parameters);
    auto squad = newObj()->put("player_list", newArr()->put(newObj()->put("gameplayer_id", p->getJSONObject("gameplayer_id"))))
        ->put("coach", newArr()->put(coachRef(100, 1000)))->put("squad_gameplan", newObj());
    auto entry = newObj()->put("main_squad_id", 9)->put("gameplayer_list", newArr()->put(p))
        ->put("coach_list", newArr()->put(coachRef(100, 1000)->put("possession", 80))->put(coachRef(200, 2000)->put("possession", 90)))
        ->put("squad_list", newArr()->put(newObj()->put("squad_id", 9)->put("squad_data", squad)
        ->put("base_team", newArr()->put(newObj()->put("base_team_id", 108)))));
    asset(root, "CmdLogin.json", newObj()->put("entry_info", entry));
    asset(root, "CmdCreateUser.json", obj(R"({"user_code":"offline","auth_code":"local","session_id":"create"})"));
    asset(root, "ZIRU_PLAYER_RECORDS.json", newObj()->put("1", newObj()->put("gameplayer", p)));
    asset(root, "ZIRU_MATCH_TEAMS.json", obj(R"({"teams":{"108":{"base_team_id":108,"team_name":"Test","team_name_short":"TST","kits":[{"id":1,"shirt_rgb":[0,0,0]},{"id":2,"shirt_rgb":[255,255,255]}]}}})"));
    asset(root, "ZIRU_MATCH_PROFILES.json", obj(R"({"profiles":[{"profile_id":1,"player_ids":[1],"coordinates":[],"team_style":0,"base_team_id":108}]})"));
    asset(root, "ZIRU_GET_VSCOM_OPPONENT.json", obj(R"({"squad_data":{"squad_gameplan":{"formation":[],"match_plan_settings":{}}},"user_info":[{}]})"));
    auto start = obj(R"({"squad_data_home":{"coach":{"coach_id":{"id":999,"serial":999}}},"commentary_info":{}})");
    asset(root, "ZIRU_START_GAME.json", start); asset(root, "ZIRU_ML_START_GAME.json", start);
    asset(root, "ZIRU_GET_GAMERESULT_LIST.json", obj(R"({"game_result_list":[{"player":[{},{}]}]})"));
    asset(root, "ZIRU_GET_VSCOM_GAME_RESULT_TEMPLATE.json", obj(R"({"game_end_result":{"match_result":{"player_data":{}},"extra_match_result":{"league_match_result":{"league_info":{"com_level_info":{},"round_match_result":{}}}}}})"));
    asset(root, "cmd_get_league_com_info.json", obj(R"({"league_info":{"com_level_info":{},"round_match_result":{}}})"));
    asset(root, "generic.json", obj(R"({"result":"NOERR"})"));
    {
      OfflineServer s(cfg);
      auto player = Object::parse(p->toString());
      auto par = player->getJSONArray("parameters");
      par->put(0, 13);
      for (int i = 14; i <= 40; i++) par->put(i, 50);
      player->getJSONObject("level_info")->put("level_limit", 40);
      auto d10 = Object::parse(player->toString());
      auto d1 = Object::parse(player->toString());
      s.z27ProgressOpponentPlayer(d10, 10);
      s.z27ProgressOpponentPlayer(d1, 1);
      check(d1->getJSONArray("parameters")->getInt(20) > d10->getJSONArray("parameters")->getInt(20),
          "Division 1 opponent gains more finishing than Division 10");
      check(player->getJSONArray("parameters")->getInt(20) == 50,
          "opponent progression does not mutate the shared player record");
      player->getJSONObject("gameplayer_id")->put("serial", 404);
      s.ziruDb->savePlayer(404, 1, player->toString());
      constexpr int64_t fullNeymar = 89071179308448LL;
      auto ownedNeymar = Object::parse(p->toString());
      ownedNeymar->getJSONObject("gameplayer_id")->put("id", (long long)fullNeymar)->put("serial", 505);
      s.ziruDb->savePlayer(505, fullNeymar, ownedNeymar->toString());
      auto staleSquad = Object::parse(s.z27Squad(9)->toString());
      auto staleId = staleSquad->getJSONArray("player_list")->getJSONObject(0)->getJSONObject("gameplayer_id");
      staleId->put("id", 40352)->put("serial", 505);
      s.ziruDb->saveSquad(9, staleSquad->toString());
      auto repairedId = s.z27Squad(9)->getJSONArray("player_list")->getJSONObject(0)->getJSONObject("gameplayer_id");
      check(repairedId->getLong("id") == fullNeymar && repairedId->getLong("serial") == 505,
          "legacy base-card squad reference migrates to the matching owned variation");
      check(s.z27Player(repairedId)->getJSONObject("gameplayer_id")->getLong("id") == fullNeymar,
          "match startup resolves migrated owned player");
      check(Object::parse(*s.loadSquad(9))->getJSONArray("player_list")->getJSONObject(0)
          ->getJSONObject("gameplayer_id")->getLong("id") == fullNeymar,
          "squad migration persists across restart");
      s.ziruDb->saveSquad(9, squad->toString());
      s.ziruDb->deletePlayerState(505);
      auto item = newObj()->put("gameplayer_id_list", newArr()->put(player->getJSONObject("gameplayer_id")))
          ->put("training_index", 0)->put("position_id", 2)
          ->put("item_list", newArr()->put(newObj()->put("present_num", 1)
              ->put("item", newObj()->put("item_type", "POSITION_TRAINING_SELECT"))));
      s.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(item)));
      auto response = newObj();
      s.applyZiruPositionTrainingResponse(response);
      check(response->getJSONObject("position_training_result")->getString("additional_position") == "CB",
          "select position card applies the requested CB position");
      check(Object::parse(*s.loadPlayer(404))->getJSONArray("parameters")->getInt(10) == 2,
          "selected position persists to the player save");
      auto items = response->getJSONArray("item_list");
      check(items->length() == 1 && items->getJSONObject(0)->getInt("inventory_num") == 999999,
          "post-training response restores missing select-position card");
      item->put("position_id", 0);
      s.decodeZiruMyclubItemRequest(zc::encodeRequest(Value(item)));
      response = newObj();
      s.applyZiruPositionTrainingResponse(response);
      check(response->getJSONObject("position_training_result")->getString("is_success") == "NO",
          "select card with no valid choice fails instead of assigning a random position");
      s.ziruDb->deletePlayerState(404);
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
      check(s.z27State()->getInt("division") == 9, "promote as soon as menu target is reached");
      check(win2->getJSONObject("game_end_result")->getJSONObject("extra_match_result")->getJSONObject("league_match_result")->getJSONObject("league_info")->getString("is_promoted") == "YES", "promotion flag");
      auto info = call(s, "CMD_GET_LEAGUE_COM_INFO")->getJSONObject("league_info");
      check(info->getInt("current_division") == 9 && info->getInt("current_point") == 0, "menu reflects committed rank");
      auto login = call(s, "CMD_LOGIN")->getJSONObject("entry_info")->getJSONArray("gameplayer_list")->getJSONObject(0);
      check(login->getJSONObject("level_info")->getInt("customize_point") == 8, "only earned level points are offered to Auto");
      auto trained = Array::parse(parameters->toString()); trained->put(14, 80);
      auto use = newObj()->put("gameplayer_id", p->getJSONObject("gameplayer_id"))->put("parameters", trained)
          ->put("use_list", newArr()->put(newObj()->put("parameter_type", "SHOOT")->put("point", 6)));
      auto spent = call(s, "CMD_USE_MYCLUB_CAREERPLAN", newObj()->put("gameplayer_use_list", newArr()->put(use)))
          ->getJSONArray("careerplan_result_list")->getJSONObject(0)->getJSONObject("gameplayer");
      check(spent->getJSONObject("level_info")->getInt("customize_point") == 2, "Auto cannot spend phantom bonus points");
      check(spent->getJSONArray("parameters")->getInt(14) == 70,
          "client-supplied inflated stats are replaced by native progression");
    }
    {
      OfflineServer s(cfg);
      check(s.z27State()->getInt("division") == 9, "rank survives process restart");
      auto p2 = call(s, "CMD_LOGIN")->getJSONObject("entry_info")->getJSONArray("gameplayer_list")->getJSONObject(0);
      check(p2->getJSONObject("level_info")->getInt("customize_point") == 2 && p2->getJSONArray("parameters")->getInt(14) == 70, "training and remaining balance survive restart/login");
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
      check(p4->getJSONArray("parameters")->getInt(37) > 0, "kicking power auto calculated from progression points");
      check(p4->getJSONArray("parameters")->getInt(34) > 0, "speed auto calculated from progression points");
      check(p4->getJSONArray("parameters")->getInt(40) > 0, "stamina auto calculated from progression points");
      auto messiCard = Object::parse(p->toString());
      auto messiId = messiCard->getJSONObject("gameplayer_id");
      messiId->put("id", 89138556575063LL)->put("serial", 606LL);
      messiCard->getJSONObject("level_info")->put("level", 32)->put("level_limit", 32)->put("customize_point", 0);
      asset(root, "ZIRU_PLAYER_RECORDS.json", newObj()
          ->put("1", newObj()->put("gameplayer", p))
          ->put("89138556575063", newObj()->put("gameplayer", messiCard)));
      s.ziruPlayerRecordsCache.reset();
      s.ziruDb->savePlayer(606, 89138556575063LL, messiCard->toString());
      auto messiUse = newObj()->put("gameplayer_id", messiId)
          ->put("use_list", newArr()->put(newObj()->put("parameter_type", "STRENGTH")->put("point", 4)));
      auto editedMessi = call(s, "CMD_USE_MYCLUB_CAREERPLAN", newObj()
          ->put("gameplayer_use_list", newArr()->put(messiUse)))
          ->getJSONArray("careerplan_result_list")->getJSONObject(0)->getJSONObject("gameplayer");
      check(editedMessi->getJSONArray("parameters")->getInt(17) == 99 &&
          editedMessi->getJSONArray("parameters")->getInt(37) > 0 &&
          editedMessi->getJSONObject("level_info")->getInt("customize_point") == 70,
          "editing Messi retains elite stats, trained attributes and Ronaldo-like spare points");
      auto resumedMessi = s.refreshZiruGameplayerFromCanonical(editedMessi);
      check(resumedMessi->getJSONArray("parameters")->getInt(17) == 99 &&
          resumedMessi->getJSONArray("parameters")->getInt(37) > 0 &&
          Object::parse(*s.loadProgression(606))->getInt("_spent_total") == 4,
          "Messi progression survives login instead of being repaired as overbudget");
      auto chosenStats = Array::parse(resumedMessi->getJSONArray("parameters")->toString());
      chosenStats->put(17, 90)->put(20, 102);
      auto customMessiUse = newObj()->put("gameplayer_id", messiId)->put("parameters", chosenStats)
          ->put("use_list", newArr()->put(newObj()->put("parameter_type", "STRENGTH")->put("point", 4)));
      auto customMessi = call(s, "CMD_USE_MYCLUB_CAREERPLAN", newObj()
          ->put("gameplayer_use_list", newArr()->put(customMessiUse)))
          ->getJSONArray("careerplan_result_list")->getJSONObject(0)->getJSONObject("gameplayer");
      check(customMessi->getJSONArray("parameters")->getInt(17) == 90 &&
          customMessi->getJSONArray("parameters")->getInt(20) == 102 &&
          customMessi->getJSONObject("level_info")->getInt("customize_point") == 70,
          "Messi accepts manually lowered and raised attributes while retaining spare points");
      auto reloadedMessi = s.refreshZiruGameplayerFromCanonical(customMessi);
      auto matchMessi = s.z27Player(messiId);
      check(reloadedMessi->getJSONArray("parameters")->getInt(17) == 90 &&
          reloadedMessi->getJSONArray("parameters")->getInt(20) == 102 &&
          matchMessi->getJSONArray("parameters")->getInt(17) == 90 &&
          matchMessi->getJSONArray("parameters")->getInt(20) == 102,
          "Messi's chosen attributes survive login and match startup");
      s.ziruDb->deletePlayerState(606);
      asset(root, "ZIRU_PLAYER_RECORDS.json", newObj()->put("1", newObj()->put("gameplayer", p)));
      s.ziruPlayerRecordsCache.reset();
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
      s.ziruDb->saveMeta("z27_division_state", R"({"division":10,"best":10,"point":0,"matches":0,"win":0,"draw":0,"lose":0,"sequence":0,"completed":{}})");
      s.ziruDb->saveMeta("z27_div10_reset_v3", "");
      auto active = s.z27Meta("z27_active_match");
      active->put("division", 10)->put("cpu_level", "NORMAL");
      s.ziruDb->saveMeta("z27_active_match", active->toString());
      check(sqlite3_exec(s.ziruDb->db, "DELETE FROM meta WHERE k='z27_div10_reset_v3'", nullptr, nullptr, nullptr) == SQLITE_OK, "prepare legacy save");
      check(s.z27State()->getInt("division") == 10, "legacy rank migrates to D10");
      auto selected = call(s, "CMD_GET_VSCOM_OPPONENT");
      check(selected->getString("cpu_level") == "NORMAL" && s.z27Active()->getInt("division") == 10, "cached opponent migrates with D10");
      active = s.z27Meta("z27_active_match");
      active->put("division", 10)->put("cpu_level", "NORMAL");
      s.ziruDb->saveMeta("z27_active_match", active->toString());
      check(call(s, "CMD_GET_VSCOM_OPPONENT")->getString("cpu_level") == "NORMAL", "already-migrated D10 saves maintain D10 snapshots");
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
      for (int id = 0; id < 72; ++id) params->put(kZiruSkillParameters[id], id == 0 ? 0 : 1);
      owned->put("additional_skill_id", newArr());
      s.ziruDb->savePlayer(303, 1, owned->toString());
      asset(root, "CMD_USE_MYCLUB_ITEM.json", obj(R"({"result":"NOERR"})"));
      auto req = newObj()->put("gameplayer_id_list", newArr()->put(owned->getJSONObject("gameplayer_id")))
          ->put("training_index", 0)->put("item_list", newArr()->put(newObj()->put("present_num", 1)
          ->put("item", newObj()->put("item_type", "SKILL_TRAINING"))));
      auto awarded = call(s, "CMD_USE_MYCLUB_ITEM", req)->getJSONObject("skill_training_result")->getInt("additional_skill_id");
      check(awarded == 0, "training excludes all built-in skills; ID zero is a real skill");
    }
    {
      OfflineServer s(cfg);
      auto players = call(s, "CMD_LOGIN")->getJSONObject("entry_info")->getJSONArray("gameplayer_list");
      bool retainedZero = false;
      for (int i = 0; i < players->length(); ++i) {
        auto p = players->getJSONObject(i);
        if (p->getJSONObject("gameplayer_id")->getLong("serial") == 303) {
          auto skills = p->getJSONArray("additional_skill_id");
          retainedZero = skills->length() == 1 && skills->getInt(0) == 0;
        }
      }
      check(retainedZero, "valid skill zero survives save, process restart and login");
    }
    {
      OfflineServer s(cfg);
      auto inflated = Object::parse(*s.loadPlayer(101));
      inflated->getJSONArray("parameters")->put(14, 100);
      s.ziruDb->savePlayer(101, 1, inflated->toString());
      s.ziruDb->saveProgression(101, R"({"_spent_total":60,"SHOOT":60})");
      auto repaired = s.refreshZiruGameplayerFromCanonical(inflated);
      check(repaired->getJSONArray("parameters")->getInt(14) == 70 &&
          repaired->getJSONObject("level_info")->getInt("customize_point") == 8,
          "overbudget Auto save returns to canonical stats and native points");
      check(Object::parse(*s.loadProgression(101))->getInt("_spent_total") == 0,
          "obsolete overbudget progression is cleared");
      check(s.loadMeta("ziru_overbudget_backup_101") && s.loadMeta("ziru_overbudget_backup_101_progression"),
          "overbudget player and progression are backed up before repair");
    }
    {
      OfflineServer s(cfg);
      s.ziruDb->saveMeta("z27_division_state", R"({"division":1,"best":1,"point":0,"matches":9,"win":0,"draw":0,"lose":9,"sequence":0,"rating":1800,"completed":{}})");
      s.ziruDb->saveMeta("z27_div10_reset_v3", "1");
      s.ziruDb->saveMeta("z27_active_match", "");
      auto id = call(s, "CMD_GET_VSCOM_OPPONENT")->getLong("game_id");
      call(s, "CMD_START_GAME");
      auto loss = result(s, id, 0, 1);
      auto lossInfo = loss->getJSONObject("game_end_result")->getJSONObject("extra_match_result")
          ->getJSONObject("league_match_result")->getJSONObject("league_info");
      check(lossInfo->getInt("pre_rating") == 1800 && lossInfo->getInt("rating") == 1788,
          ("Division 1 loss rating: " + std::to_string(lossInfo->getInt("pre_rating")) +
           " -> " + std::to_string(lossInfo->getInt("rating"))).c_str());
      check(s.z27State()->getInt("division") == 1 && s.z27State()->getInt("rating") == 1788,
          "Division 1 retains rating across the ten-match boundary without relegation");
      result(s, id, 0, 1);
      check(s.z27State()->getInt("rating") == 1788, "duplicate match result cannot subtract rating twice");
      id = call(s, "CMD_GET_VSCOM_OPPONENT")->getLong("game_id");
      call(s, "CMD_START_GAME");
      auto win = result(s, id, 1, 0);
      auto winInfo = win->getJSONObject("game_end_result")->getJSONObject("extra_match_result")
          ->getJSONObject("league_match_result")->getJSONObject("league_info");
      check(winInfo->getInt("pre_rating") == 1788 && winInfo->getInt("rating") == 1800,
          "Division 1 win adds rating and result shows the before-and-after values");
      check(call(s, "CMD_GET_LEAGUE_COM_INFO")->getJSONObject("league_info")->getInt("rating") == 1800,
          "Division 1 menu reflects committed rating");
    }
    {
      OfflineServer s(cfg);
      check(s.z27State()->getInt("rating") == 1800 && s.z27State()->getInt("division") == 1,
          "Division 1 rating survives process restart");
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
    std::cout << "PASS: gameplay, booster layout, all 72 skill mappings/roundtrip/training/skill-zero restart, CREATE_USER resume, D1 migration, failed writes and abrupt-exit persistence\n";
    return 0;
  } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << "\n"; return 1; }
}
