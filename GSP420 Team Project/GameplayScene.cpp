#include "GameplayScene.h"
#include "SharedStore.h"
#include <sstream>
#include <algorithm>
#include "TitleScene.h"
#include "High_Score.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>


GameplayScene::GameplayScene(SharedStore* store) :
  Scene(store),
  font(L"Arial"),
  pauseFont(L"Arial"),
  scoreText(L"derp", &font),
  playerTexture(L"Texture/Player Sprite.png"),
  player(store, playerTexture),
  particles(store, playerTexture),
  junkDist(junkChance),
  junkTexture(L"Texture/junk.png"),
  blackTex(L"Texture/Planes/black.png"),
  pauseSnd("SFX/button-37.mp3"),
  unpauseSnd("SFX/button-10.mp3"),
  hiScoreFont(L"Arial", 40.0f),
  hiScoreText(L"New High Score!", &hiScoreFont)
{
  hiScoreFont.setBold(true);

  unpauseSnd.setVolume(0.15f);

  store->oldBaseVol = store->songBaseVol;
  store->songBaseVol = SONG_VOLUME;
  store->songPath = "BGM/Mistake the Getaway.mp3";

  store->speed = store->START_SPEED;
  store->time = 0;

  pausePlane.setTexture(blackTex);
  pausePlane.opacity = 0.65f;

  rooftops.emplace_back(store, true);
  for(int i = 0; i < 3; i++) { genNextRoof(); }

  scoreBG.setBitmap(blackTex);
  scoreBG.destRect = GSPRect(0, 5, 150, 25);
  scoreBG.opacity = 0.5f;
  scoreBG.destRect.x = (store->screenWidth - scoreBG.destRect.width) / 2;

  GSPRect scoreRect = scoreBG.destRect;
  scoreRect.x += 5;

  scoreText.setRect(scoreRect);
  font.setColor(D2D1::ColorF::White);

  createPauseMenu();
}

GameplayScene::~GameplayScene() {
  gMessageHandler->RemoveRecipient(RGPSCENE);
  setPaused(false); //removes menu
}

Scene* GameplayScene::update(float dt) {
  if(paused) { return pausedUpdate(dt); }
  return playUpdate(dt);
}

Scene* GameplayScene::pausedUpdate(float dt) {
  if(store->input.IsKeyTriggered(InputManager::KEY_ESC)) { setPaused(false); }

  menu.Update();
  msgReciever.processQueue();

  switch(msgReciever.acceptedValue) {
  case 0: break;
  case 1:
    setPaused(false);
    msgReciever.acceptedValue = 0;
    break;
  case 2: return new TitleScene(store);
  }

  return this;
}

Scene* GameplayScene::playUpdate(float dt) {
  particles.update(dt);
  if(player.isDead()) { return splattedUpdate(dt); }

  if(store->input.IsKeyTriggered(InputManager::KEY_ESC)) { 
    setPaused(true);
    return this;
  }

  updateRooftops(dt);
  updateJunk(dt);
  updateJunkParticles(dt);

  updatePlayer(dt);
  if(player.isDead()) { 
    updateHiScores();
    return this;
  }

  store->time += dt;
  if(store->speed < store->MAX_SPEED) { store->speed += 30 * dt; }
  else if(store->speed > store->MAX_SPEED) { store->speed = store->MAX_SPEED; }

  updateScore(dt);
  return this;
}

Scene* GameplayScene::splattedUpdate(float dt) {
  if(store->input.IsKeyTriggered(InputManager::KEY_DASH) || store->input.IsMousePressed(InputManager::MOUSE_LEFT)) {
    return new High_Score(store);
  }

  player.update(dt, std::vector<GSPRect>(), std::deque<JunkPile>());
  updateJunkParticles(dt);

  return this;
}

void GameplayScene::updateJunk(float dt) {
  float rate = store->speed;
  if(player.isDashing()) { rate = store->DASH_SPEED; }

  for(auto& p : piles) { p.update(dt, rate); }
  piles.erase(std::remove_if(piles.begin(), piles.end(), [](JunkPile& p) { return !p.isAlive(); }), piles.end());
}

void GameplayScene::updateJunkParticles(float dt) {
  float rate = store->speed;
  if(player.isDashing()) { rate = store->DASH_SPEED; }
  if(player.isDead()) { rate = 0.0f; }

  for(auto& junk : junkParticles) { junk->update(dt, rate); }
  junkParticles.erase(std::remove_if(junkParticles.begin(), junkParticles.end(), [](std::unique_ptr<JunkParticleSystem>& p) { return p->isFinished(); }), junkParticles.end());

}

void GameplayScene::updateRooftops(float dt) {
  float rate = store->speed;
  if(player.isDashing()) { rate = store->DASH_SPEED; }

  bg.update(dt, rate); //this doesn't belong here, but whatever

  if(rooftops.front().out()) {
    rooftops.pop_front();
    genNextRoof();
  }

  for(auto& r : rooftops) { r.update(dt, rate); }
}

void GameplayScene::updateScore(float dt) {
  std::wstringstream ss;
  store->score = int(store->time * 1000);
  ss << L"Score: " << store->score;
  scoreText.setString(ss.str());
}

void GameplayScene::updatePlayer(float dt) {
  std::vector<GSPRect> roofColliders;
  for(auto& roof : rooftops) { roofColliders.push_back(roof.getCollider()); }
  player.update(dt, roofColliders, piles);
  if(player.isDashing()) { particles.add(player.getCenterPosition()); }
}

void GameplayScene::draw() {
  bg.draw();
  particles.draw();
  for(auto& roof : rooftops) { roof.draw(); }
  for(auto& junk : junkParticles) { junk->draw(); }
  scoreBG.draw();
  scoreText.draw();
  player.draw();

  if(paused) {
    pausePlane.draw();
    menu.draw();
    for(auto& t : menuText) { t.draw(); }
  }

  if(gotHiScore) {
    GSPRect baseRect(float(store->screenWidth - 325) / 2, 250.0f, 325.0f, 100.0f);
    hiScoreFont.setColor(D2D1::ColorF::Black);

    for(float x = -3; x <= 3; x++) {
      for(float y = -3; y <= 3; y++) {
        GSPRect temp = baseRect;
        temp.moveBy(vec2f{x, y});
        hiScoreText.setRect(temp);
        hiScoreText.draw();
      }
    }

    hiScoreText.setRect(baseRect);
    hiScoreFont.setColor(D2D1::ColorF::Red);
    hiScoreText.draw();
  }
}

void GameplayScene::genNextRoof() {
  auto& prev = rooftops.back().getCollider();
  rooftops.emplace_back(store, prev.x + prev.width, prev.y, store->speed / store->START_SPEED);

  if(justDidJunk) {
    justDidJunk = false;
  }
  else if(junkDist(store->rng)) { //add junk
    auto& cur = rooftops.back().getCollider();
    vec2f position{cur.x + (cur.width / 2), cur.y};
    junkParticles.emplace_back(std::make_unique<JunkParticleSystem>(store, position, &junkTexture));
    piles.emplace_back(position, junkParticles.back().get());
    justDidJunk = true;
  }

}

void GameplayScene::setPaused(bool pause) {
  if(paused == pause) { return; }

  paused = pause;
  if(paused) {
    pauseSnd.play();
    store->bgm->setVolume(SONG_VOLUME * 0.5f);
    gMessageHandler->AddRecipient(&menu, RGPMENU);
  }
  else {
    unpauseSnd.play();
    store->bgm->setVolume(SONG_VOLUME);
    gMessageHandler->RemoveRecipient(RGPMENU);
  }
}

void GameplayScene::createPauseMenu() {
  Texture buttonTex(L"Texture/buttons.png");
  std::vector<Sprite> btnFrames;
  btnFrames.emplace_back();

  btnFrames[0].setBitmap(buttonTex);
  btnFrames[0].srcRect.height /= 3;
  btnFrames[0].destRect = btnFrames[0].srcRect;
  btnFrames[0].destRect.x = (store->screenWidth - btnFrames[0].srcRect.width) / 2;
  btnFrames[0].destRect.y = 200;

  btnFrames.push_back(btnFrames.back());
  btnFrames.back().srcRect.y += btnFrames.back().srcRect.height;
  btnFrames.push_back(btnFrames.back());
  btnFrames.back().srcRect.y += btnFrames.back().srcRect.height;

  menu.AddButton(btnFrames[0], btnFrames[1], btnFrames[2], btnFrames[0].destRect, GSPMessage(RGPSCENE, 1));
  for(auto& spr : btnFrames) { spr.destRect.y += spr.destRect.height + 20; }
  menu.AddButton(btnFrames[0], btnFrames[1], btnFrames[2], btnFrames[0].destRect, GSPMessage(RGPSCENE, 2));
  for(auto& spr : btnFrames) { spr.destRect.y += spr.destRect.height + 20; }

  gMessageHandler->AddRecipient(&msgReciever, RGPSCENE);
  store->msgTgt = RGPMENU;

  pauseFont.setColor(D2D1::ColorF::Black);
  pauseFont.setSize(30.0f);

  menuText.emplace_back(L"Resume", &pauseFont);
  menuText.emplace_back(L"Quit", &pauseFont);
  
  menuText[0].setRect(GSPRect(487.0f, 232.0f, 200.0f, 100.0f));
  menuText[1].setRect(GSPRect(512.0f, 354.0f, 200.0f, 100.0f));

}

void GameplayScene::updateHiScores() {
  std::vector<std::pair<int, std::string>> scores(5);

  {
    std::ifstream file("hiscores.txt");
    assert(file && "Failed to open file.");

    for(int i = 0; i < 5; i++) {
      std::getline(file, scores[i].second);
      scores[i].first = std::stoi(scores[i].second);
      std::getline(file, scores[i].second);
    }
  }

  time_t now;
  time(&now);
  std::stringstream ss;
  tm temp;
  localtime_s(&temp, &now);
  ss << std::put_time(&temp, "%m/%d/%Y");
  std::pair<int, std::string> myScore(store->score, ss.str());
  ss.str("");

  for(auto iter = scores.begin(); iter != scores.end(); iter++) {
    if(iter->first < myScore.first) {
      scores.insert(iter, myScore);
      scores.pop_back();
      gotHiScore = true;
      break;
    }
  }

  std::ofstream file("hiscores.txt");
  assert(file && "Failed to open file.");

  for(auto sc : scores) {
    file << sc.first << "\n" << sc.second << "\n";
  }
}


