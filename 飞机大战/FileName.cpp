#include<iostream>
#include<cstdlib>
#include<graphics.h>
#include<vector>
#include<string>
#include<fstream>
#include<algorithm>
#include<cstdio>
#include<cctype>
#include<windows.h>
#include<mmsystem.h>
#include<ctime>
#define NOMINMAX
#include<math.h>

#pragma comment(lib, "winmm.lib")

using namespace std;

constexpr auto WIDTH = 600;
constexpr auto HEIGHT = 1000;
constexpr unsigned SUMHP = 3;
constexpr ULONGLONG hurttime = 1200;
constexpr DWORD FRAME_TIME_MS = 8; // About 120 FPS for smoother animation
constexpr int PLAYER_SPEED = 4;
constexpr ULONGLONG FIRE_POWER_DURATION_MS = 7000;
constexpr ULONGLONG DRONE_DURATION_MS = 8000;
constexpr ULONGLONG WINGMAN_FIRE_INTERVAL_MS = 360;
constexpr int BOMB_BOSS_DAMAGE = 10;
constexpr ULONGLONG COMBO_TIMEOUT_MS = 2600;
constexpr ULONGLONG REVIVE_HOLD_MS = 8000;
constexpr ULONGLONG FREEZE_DURATION_MS = 5500;
constexpr ULONGLONG MAGNET_DURATION_MS = 8000;
constexpr ULONGLONG PIERCE_DURATION_MS = 7000;
constexpr ULONGLONG OVERLOAD_DURATION_MS = 4500;
constexpr ULONGLONG OVERLOAD_COOLDOWN_MS = 1500;
constexpr float PI = 3.1415926535f;
constexpr size_t MAX_ENEMY_BULLETS = 260;

int gameMode = 1;
int runMode = 1; // 1: three-stage mode, 2: endless mode
unsigned long long totalScore = 0;
ULONGLONG p1ShootClock = 0, p2ShootClock = 0;
bool spaceLast = false, num1Last = false;
bool gameWon = false;
string playerName;
float gEnemyMoveScale = 1.0f;

enum MUSIC_TRACK { MUSIC_NONE=-1,MUSIC_MENU,MUSIC_STAGE12,MUSIC_BOSS12,MUSIC_STAGE3,MUSIC_ENDLESS,MUSIC_PAUSE,MUSIC_OVER,MUSIC_INTRO };
int gCurrentMusic=MUSIC_NONE;
bool gShootSoundReady=false,gExplosionSoundReady=false,gPlayerDeathReady=false;

LPCTSTR MusicPath(MUSIC_TRACK track){
    if(track==MUSIC_MENU)return _T("./music/menu_loop.mp3");
    if(track==MUSIC_PAUSE)return _T("./music/pause_music.mp3");
    if(track==MUSIC_STAGE12)return _T("./music/stage12_loop.mp3");
    if(track==MUSIC_BOSS12)return _T("./music/boss12_loop.mp3");
    if(track==MUSIC_STAGE3)return _T("./music/stage3_loop.mp3");
    if(track==MUSIC_ENDLESS)return _T("./music/endless_loop.mp3");
    if(track==MUSIC_OVER)return _T("./music/gameover_loop.mp3");  // 占位文件（如不存在会自动静音）
    if(track==MUSIC_INTRO)return _T("./music/intro.mp3");
    return _T("");
}

MUSIC_TRACK GameplayMusic(int selectedRunMode,int level,bool bossActive){
    if(selectedRunMode==2)return MUSIC_ENDLESS;
    if(level>=3)return MUSIC_STAGE3;
    return bossActive?MUSIC_BOSS12:MUSIC_STAGE12;
}

void PlayMusic(MUSIC_TRACK track){
    if(track==gCurrentMusic)return;
    mciSendString(_T("stop game_bgm"),NULL,0,NULL);
    mciSendString(_T("close game_bgm"),NULL,0,NULL);
    gCurrentMusic=MUSIC_NONE;
    if(track==MUSIC_NONE)return;
    TCHAR fullPath[MAX_PATH];
    if(GetFullPathName(MusicPath(track),MAX_PATH,fullPath,NULL)==0)return;
    TCHAR command[MAX_PATH+96];
    _stprintf_s(command,MAX_PATH+96,_T("open \"%s\" type mpegvideo alias game_bgm"),fullPath);
    if(mciSendString(command,NULL,0,NULL)!=0)return;
    mciSendString(_T("setaudio game_bgm volume to 420"),NULL,0,NULL);
    if(mciSendString(_T("play game_bgm repeat"),NULL,0,NULL)==0)gCurrentMusic=track;
    else mciSendString(_T("close game_bgm"),NULL,0,NULL);
}

void PlayMciSound(LPCTSTR alias){
    TCHAR command[96];_stprintf_s(command,96,_T("stop %s"),alias);mciSendString(command,NULL,0,NULL);
    _stprintf_s(command,96,_T("play %s from 0"),alias);mciSendString(command,NULL,0,NULL);
}

// ====================== Intro 音乐管理 ======================
// intro.mp3 用独立的 alias "intro_bgm"，避免与 game_bgm 冲突
// 播放模式：不 repeat（默认就是只播一次）
void PlayIntroMusic(){
    mciSendString(_T("stop intro_bgm"),NULL,0,NULL);
    mciSendString(_T("close intro_bgm"),NULL,0,NULL);
    TCHAR fullPath[MAX_PATH];
    if(GetFullPathName(_T("./music/intro.mp3"),MAX_PATH,fullPath,NULL)==0)return;
    TCHAR command[MAX_PATH+96];
    _stprintf_s(command,MAX_PATH+96,_T("open \"%s\" type mpegvideo alias intro_bgm"),fullPath);
    if(mciSendString(command,NULL,0,NULL)!=0)return;
    mciSendString(_T("setaudio intro_bgm volume to 420"),NULL,0,NULL);
    mciSendString(_T("play intro_bgm"),NULL,0,NULL);   // 不带 repeat → 只播一次
}
void StopIntroMusic(){
    mciSendString(_T("stop intro_bgm"),NULL,0,NULL);
    mciSendString(_T("close intro_bgm"),NULL,0,NULL);
}

void InitGameAudio(){
    mciSendString(_T("close game_shoot"),NULL,0,NULL);
    mciSendString(_T("close game_explosion"),NULL,0,NULL);
    mciSendString(_T("close game_meow1"),NULL,0,NULL);
    mciSendString(_T("close game_meow2"),NULL,0,NULL);
    mciSendString(_T("close game_player_death"),NULL,0,NULL);
    TCHAR shootPath[MAX_PATH],explosionPath[MAX_PATH],command[MAX_PATH+96];
    TCHAR meow1Path[MAX_PATH], meow2Path[MAX_PATH],playerDeathPath[MAX_PATH];
    GetFullPathName(_T("./music/laserRetro_000.wav"),MAX_PATH,shootPath,NULL);
    GetFullPathName(_T("./music/explosion_enermy.mp3"),MAX_PATH,explosionPath,NULL);
    GetFullPathName(_T("./music/meow.mp3"),MAX_PATH,meow1Path,NULL);
    GetFullPathName(_T("./music/meow2.mp3"),MAX_PATH,meow2Path,NULL);
    GetFullPathName(_T("./music/player_death.mp3"),MAX_PATH,playerDeathPath,NULL);
    _stprintf_s(command,MAX_PATH+96,_T("open \"%s\" type waveaudio alias game_shoot"),shootPath);
    gShootSoundReady=mciSendString(command,NULL,0,NULL)==0;
    _stprintf_s(command,MAX_PATH+96,_T("open \"%s\" type mpegvideo alias game_explosion"),explosionPath);
    gExplosionSoundReady=mciSendString(command,NULL,0,NULL)==0;
    if(gShootSoundReady)mciSendString(_T("setaudio game_shoot volume to 650"),NULL,0,NULL);
    if(gExplosionSoundReady)mciSendString(_T("setaudio game_explosion volume to 850"),NULL,0,NULL);
    _stprintf_s(command,MAX_PATH+96,_T("open \"%s\" type mpegvideo alias game_meow1"),meow1Path);
    bool meow1Ready=mciSendString(command,NULL,0,NULL)==0;
    _stprintf_s(command,MAX_PATH+96,_T("open \"%s\" type mpegvideo alias game_meow2"),meow2Path);
    bool meow2Ready=mciSendString(command,NULL,0,NULL)==0;
    if(meow1Ready)mciSendString(_T("setaudio game_meow1 volume to 700"),NULL,0,NULL);
    if(meow2Ready)mciSendString(_T("setaudio game_meow2 volume to 700"),NULL,0,NULL);
    // 玩家飞机爆炸音效：和 game_explosion 一样的预加载模式（type mpegvideo 兼容 mp3）
    _stprintf_s(command,MAX_PATH+96,_T("open \"%s\" type mpegvideo alias game_player_death"),playerDeathPath);
    gPlayerDeathReady=mciSendString(command,NULL,0,NULL)==0;
    if(gPlayerDeathReady)mciSendString(_T("setaudio game_player_death volume to 900"),NULL,0,NULL);
}

void CloseGameAudio(){
    PlayMusic(MUSIC_NONE);
    mciSendString(_T("close game_shoot"),NULL,0,NULL);
    mciSendString(_T("close game_explosion"),NULL,0,NULL);
    mciSendString(_T("close game_meow1"),NULL,0,NULL);
    mciSendString(_T("close game_meow2"),NULL,0,NULL);
    mciSendString(_T("close game_player_death"),NULL,0,NULL);
    gShootSoundReady=false;gExplosionSoundReady=false;gPlayerDeathReady=false;
}

void PlayMeowRandom() {
    // 30% 概率播放猫叫，避免太频繁
    static ULONGLONG lastMeow = 0;
    ULONGLONG now = GetTickCount();
    if (now - lastMeow < 250) return;  // 至少间隔 250ms
    if (rand() % 100 >= 30) return;
    lastMeow = now;
    bool pick1 = (rand() % 2) == 0;
    PlayMciSound(pick1 ? _T("game_meow1") : _T("game_meow2"));
}

enum GAME_SOUND { SOUND_HIT,SOUND_EXPLOSION,SOUND_PICKUP,SOUND_BOSS,SOUND_SHOOT,SOUND_DEATH_DROP,SOUND_PLAYER_DEATH };
void PlayGameSound(GAME_SOUND type) {
    static ULONGLONG lastPlayed[7]={0,0,0,0,0,0,0};
    static const DWORD cooldown[7]={55,100,100,650,65,500,0};
    ULONGLONG now=GetTickCount();
    if(type!=SOUND_PLAYER_DEATH && now-lastPlayed[type]<cooldown[type])return;
    lastPlayed[type]=now;
    if(type==SOUND_EXPLOSION&&gExplosionSoundReady){PlayMciSound(_T("game_explosion"));return;}
    if(type==SOUND_SHOOT&&gShootSoundReady){PlayMciSound(_T("game_shoot"));return;}
    if(type==SOUND_DEATH_DROP){
        // 临时打开并播放"死亡掉落"音效（独立 mci device，不影响主音乐）
        // 占位文件：./music/player_death_drop.mp3（如不存在会自动失败，不影响游戏）
        TCHAR fullPath[MAX_PATH];
        if(GetFullPathName(_T("./music/player_death_drop.mp3"),MAX_PATH,fullPath,NULL)!=0){
            TCHAR command[MAX_PATH+96];
            mciSendString(_T("close sfx_death_drop"),NULL,0,NULL);
            _stprintf_s(command,MAX_PATH+96,_T("open \"%s\" type mpegvideo alias sfx_death_drop"),fullPath);
            if(mciSendString(command,NULL,0,NULL)==0){
                mciSendString(_T("setaudio sfx_death_drop volume to 900"),NULL,0,NULL);
                mciSendString(_T("play sfx_death_drop"),NULL,0,NULL);
            }
        }
        return;
    }
    if(type==SOUND_PLAYER_DEATH&&gPlayerDeathReady){PlayMciSound(_T("game_player_death"));return;}
    LPCTSTR alias=type==SOUND_HIT?_T("SystemAsterisk"):
        type==SOUND_EXPLOSION?_T("SystemExclamation"):
        type==SOUND_PICKUP?_T("SystemQuestion"):
        type==SOUND_SHOOT?_T("SystemAsterisk"):
        type==SOUND_DEATH_DROP?_T("SystemExclamation"):_T("SystemHand");
    if(!PlaySound(alias,NULL,SND_ALIAS|SND_ASYNC|SND_NODEFAULT))MessageBeep(type==SOUND_BOSS?MB_ICONHAND:MB_OK);
}

struct SCORE_ENTRY { string name; unsigned long long score; };
struct UPGRADE_STATE {
    int bulletDamage=1, fireRateLevel=0, pierce=0, sideBullet=0;
    int moveSpeed=0, shieldCapacity=1, bombDamage=0, lowHpDouble=0;
};
struct SAVE_DATA {
    int version=2, mode=1, runMode=1, level=1, bombCount=0, bossHp=0;
    unsigned long long score=0, levelScoreStart=0, levelElapsedMs=0;
    bool bossActive=false, p1Dead=false, p2Dead=false, p1Shield=false, p2Shield=false;
    int p1Hp=3,p2Hp=3,p1FireMs=0,p2FireMs=0,p1DroneMs=0,p2DroneMs=0;
    int p1ShieldCount=0,p2ShieldCount=0,p1Kills=0,p2Kills=0,combo=0,graze=0;
    UPGRADE_STATE upgrade;
    RECT p1Rect{},p2Rect{};
    string name="PLAYER";
};

const char* SAVE_FILE="./savegame.dat";
const char* STAGE_RANK_FILE="./leaderboard_stage.txt";
const char* ENDLESS_RANK_FILE="./leaderboard_endless.txt";
const char* PLAYER_FILE="./last_player.txt";

bool LoadLastPlayer(string& name) {
    ifstream in(PLAYER_FILE);string value;if(!(in>>value)||value.empty())return false;
    name=value;return true;
}
void SaveLastPlayer(const string& name) {
    if(!name.empty()){ofstream out(PLAYER_FILE,ios::trunc);out<<name<<'\n';}
}

string NormalizeName(string name) {
    transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return (char)tolower(c);});
    return name;
}
const char* RankFile(int selectedRunMode) {
    return selectedRunMode==2?ENDLESS_RANK_FILE:STAGE_RANK_FILE;
}
vector<SCORE_ENTRY> LoadScores(int selectedRunMode) {
    vector<SCORE_ENTRY> result; ifstream in(RankFile(selectedRunMode));
    SCORE_ENTRY e;
    while(in>>e.name>>e.score) {
        string key=NormalizeName(e.name);bool found=false;
        for(auto& old:result) if(NormalizeName(old.name)==key) {
            if(e.score>old.score){old.score=e.score;old.name=e.name;}
            found=true;break;
        }
        if(!found)result.push_back(e);
    }
    sort(result.begin(),result.end(),[](const SCORE_ENTRY& a,const SCORE_ENTRY& b){return a.score>b.score;});
    if(result.size()>10) result.resize(10); return result;
}
void AddScore(const string& name,unsigned long long score,int selectedRunMode) {
    vector<SCORE_ENTRY> scores=LoadScores(selectedRunMode);string actualName=name.empty()?"PLAYER":name,key=NormalizeName(actualName);bool found=false;
    for(auto& e:scores)if(NormalizeName(e.name)==key){if(score>e.score)e.score=score;e.name=actualName;found=true;break;}
    if(!found)scores.push_back({actualName,score});
    sort(scores.begin(),scores.end(),[](const SCORE_ENTRY& a,const SCORE_ENTRY& b){return a.score>b.score;});
    if(scores.size()>10)scores.resize(10);
    ofstream out(RankFile(selectedRunMode),ios::trunc); for(const auto& e:scores) out<<e.name<<' '<<e.score<<'\n';
}
bool HasSave() { ifstream in(SAVE_FILE); string tag; return (in>>tag)&&(tag=="PLANE_SAVE_V1"||tag=="PLANE_SAVE_V2"); }
bool LoadSave(SAVE_DATA& s) {
    ifstream in(SAVE_FILE); string tag; if(!(in>>tag)||(tag!="PLANE_SAVE_V1"&&tag!="PLANE_SAVE_V2"))return false;
    bool v2=tag=="PLANE_SAVE_V2";
    s.version=v2?2:1;
    if(v2) in>>s.name>>s.mode>>s.runMode>>s.score>>s.level>>s.levelScoreStart>>s.levelElapsedMs;
    else in>>s.name>>s.mode>>s.score>>s.level>>s.levelScoreStart>>s.levelElapsedMs;
    in>>s.bossActive>>s.bossHp>>s.bombCount;
    in>>s.p1Hp>>s.p1Dead>>s.p1Shield>>s.p1FireMs>>s.p1Rect.left>>s.p1Rect.top>>s.p1Rect.right>>s.p1Rect.bottom;
    in>>s.p2Hp>>s.p2Dead>>s.p2Shield>>s.p2FireMs>>s.p2Rect.left>>s.p2Rect.top>>s.p2Rect.right>>s.p2Rect.bottom;
    if (in.fail()) return false;
    // Older saves do not contain drone timers. They remain valid and simply
    // resume without an active drone.
    if (!(in>>s.p1DroneMs>>s.p2DroneMs)) {
        in.clear();s.p1DroneMs=0;s.p2DroneMs=0;
    }
    if(v2) {
        in>>s.p1ShieldCount>>s.p2ShieldCount>>s.p1Kills>>s.p2Kills>>s.combo>>s.graze;
        in>>s.upgrade.bulletDamage>>s.upgrade.fireRateLevel>>s.upgrade.pierce>>s.upgrade.sideBullet
          >>s.upgrade.moveSpeed>>s.upgrade.shieldCapacity>>s.upgrade.bombDamage>>s.upgrade.lowHpDouble;
        if(in.fail()) return false;
    }
    return s.level>=1 && (s.runMode==2||s.level<=3) && (s.mode==1||s.mode==2) && (s.runMode==1||s.runMode==2);
}
bool WriteSave(const SAVE_DATA& s) {
    ofstream out(SAVE_FILE,ios::trunc); if(!out)return false;
    out<<"PLANE_SAVE_V2\n"<<s.name<<' '<<s.mode<<' '<<s.runMode<<' '<<s.score<<' '<<s.level<<' '<<s.levelScoreStart<<' '<<s.levelElapsedMs<<'\n';
    out<<s.bossActive<<' '<<s.bossHp<<' '<<s.bombCount<<'\n';
    out<<s.p1Hp<<' '<<s.p1Dead<<' '<<s.p1Shield<<' '<<s.p1FireMs<<' '<<s.p1Rect.left<<' '<<s.p1Rect.top<<' '<<s.p1Rect.right<<' '<<s.p1Rect.bottom<<'\n';
    out<<s.p2Hp<<' '<<s.p2Dead<<' '<<s.p2Shield<<' '<<s.p2FireMs<<' '<<s.p2Rect.left<<' '<<s.p2Rect.top<<' '<<s.p2Rect.right<<' '<<s.p2Rect.bottom<<'\n';
    out<<s.p1DroneMs<<' '<<s.p2DroneMs<<'\n';
    out<<s.p1ShieldCount<<' '<<s.p2ShieldCount<<' '<<s.p1Kills<<' '<<s.p2Kills<<' '<<s.combo<<' '<<s.graze<<'\n';
    out<<s.upgrade.bulletDamage<<' '<<s.upgrade.fireRateLevel<<' '<<s.upgrade.pierce<<' '<<s.upgrade.sideBullet<<' '
       <<s.upgrade.moveSpeed<<' '<<s.upgrade.shieldCapacity<<' '<<s.upgrade.bombDamage<<' '<<s.upgrade.lowHpDouble<<'\n';
    return out.good();
}

inline void putimage_mask(int x, int y, IMAGE* src, IMAGE* mask) {
    putimage(x, y, mask, SRCAND);
    putimage(x, y, src, SRCPAINT);
}

// 色调偏移：把 sprite 的非透明像素按 mode 偏色。
// mode: 0 = 偏红（玩家被击中），1 = 偏白（敌机被击中）
// 重要：你的 mask 约定是 mask=BLACK 为机身（不透明），mask=WHITE 为透明区
//      （与 SRCAND/SRCPAINT 流程配套：先 AND 清掉 mask=BLACK 区域，再 PAINT 上 src）
static inline void MakeTintedCopy(IMAGE* src, IMAGE* mask, IMAGE* outSrc, IMAGE* outMask, int mode) {
    int w = src->getwidth(), h = src->getheight();
    outSrc->Resize(w, h);
    outMask->Resize(w, h);
    DWORD* sBuf = GetImageBuffer(src);
    DWORD* mBuf = GetImageBuffer(mask);
    DWORD* tsBuf = GetImageBuffer(outSrc);
    DWORD* tmBuf = GetImageBuffer(outMask);
    for (int j = 0; j < w * h; j++) {
        // mask==BLACK 才是机身（不透明）；mask==WHITE 是透明区
        DWORD mc = mBuf[j];
        bool opaque = ((mc & 0x00FFFFFF) == 0);
        if (!opaque) {
            tmBuf[j] = WHITE;  // 透明区：mask=WHITE
            tsBuf[j] = 0;       //         src=BLACK
            continue;
        }
        tmBuf[j] = mc;  // 不透明：mask=BLACK
        int r = (int)((sBuf[j] >> 16) & 0xFF);
        int g = (int)((sBuf[j] >> 8) & 0xFF);
        int b = (int)(sBuf[j] & 0xFF);
        if (mode == 0) {
            r = min(255, (int)(r * 1.6f + 90));
            g = (int)(g * 0.55f);
            b = (int)(b * 0.55f);
        } else {
            r = min(255, (int)(r + (255 - r) * 0.7f));
            g = min(255, (int)(g + (255 - g) * 0.7f));
            b = min(255, (int)(b + (255 - b) * 0.7f));
        }
        tsBuf[j] = RGB((unsigned)r, (unsigned)g, (unsigned)b);
    }
}

// 临时合成一张偏色版并贴图。tintMode<=0 时等同 putimage_mask。
// 用一个静态缓冲复用，避免每帧分配。仅用于偶发短促 tint（hit flash 等），不用于持续动画。
inline void putimage_tinted(int x, int y, IMAGE* src, IMAGE* mask, int tintMode) {
    if (tintMode <= 0) { putimage_mask(x, y, src, mask); return; }
    static IMAGE sTinted, sTintedMask;
    static int sW = -1, sH = -1;
    int w = src->getwidth(), h = src->getheight();
    if (sW != w || sH != h) { sTinted.Resize(w, h); sTintedMask.Resize(w, h); sW = w; sH = h; }
    MakeTintedCopy(src, mask, &sTinted, &sTintedMask, tintMode);
    putimage_mask(x, y, &sTinted, &sTintedMask);
}

// ============ Sprite Sheet Animation System ============
// 从Aseprite导出的水平排列精灵表中提取单帧
void LoadSpriteSheetFrames(IMAGE* frames, int frameCount, LPCTSTR sheetFile, int frameWidth, int frameHeight) {
    IMAGE sheet;
    loadimage(&sheet, sheetFile);
    int sheetWidth = sheet.getwidth();
    int sheetHeight = sheet.getheight();
    int framesPerRow = sheetWidth / frameWidth;

    for (int i = 0; i < frameCount; i++) {
        int row = i / framesPerRow;
        int col = i % framesPerRow;
        int srcX = col * frameWidth;
        int srcY = row * frameHeight;

        frames[i].Resize(frameWidth, frameHeight);
        DWORD* dstBuf = GetImageBuffer(&frames[i]);
        DWORD* srcBuf = GetImageBuffer(&sheet);

        for (int y = 0; y < frameHeight; y++) {
            for (int x = 0; x < frameWidth; x++) {
                int srcIdx = (srcY + y) * sheetWidth + (srcX + x);
                int dstIdx = y * frameWidth + x;
                if (srcY + y < sheetHeight && srcX + x < sheetWidth) {
                    dstBuf[dstIdx] = srcBuf[srcIdx];
                } else {
                    dstBuf[dstIdx] = BLACK;
                }
            }
        }
    }
}

// 自动从 PNG 的 alpha 通道生成 mask：alpha>=128 视为不透明（mask=BLACK），否则透明（mask=WHITE）。
// 适用于你自己画的带半透明边缘的死亡动画图。
static inline void BuildMaskFromAlpha(IMAGE* src, IMAGE* mask) {
    int w = src->getwidth(), h = src->getheight();
    mask->Resize(w, h);
    DWORD* s = GetImageBuffer(src);
    DWORD* m = GetImageBuffer(mask);
    for (int j = 0; j < w * h; j++) {
        BYTE a = (BYTE)((s[j] >> 24) & 0xFF);
        m[j] = (a >= 128) ? BLACK : WHITE;  // mask 约定：BLACK=机身, WHITE=透明
    }
}

// 给一组 sprite sheet 单帧生成对应的 mask 数组（沿用 sheet 横向排列约定）
static inline void BuildMaskArrayFromAlpha(IMAGE* srcFrames, IMAGE* maskFrames, int frameCount, int frameWidth, int frameHeight) {
    IMAGE sheet;
    IMAGE srcS;
    // 直接基于调用方已经拆好的单帧 srcFrames 来生成 mask，避免再次访问 sheet
    for (int i = 0; i < frameCount; i++) {
        srcS = srcFrames[i];
        maskFrames[i].Resize(frameWidth, frameHeight);
        DWORD* s = GetImageBuffer(&srcS);
        DWORD* m = GetImageBuffer(&maskFrames[i]);
        for (int j = 0; j < frameWidth * frameHeight; j++) {
            BYTE a = (BYTE)((s[j] >> 24) & 0xFF);
            m[j] = (a >= 128) ? BLACK : WHITE;
        }
    }
}

// 精灵表动画类 (用于菜单背景等)
class AnimatedBG {
public:
    AnimatedBG(LPCTSTR sheetFile, int frameW, int frameH, int frames, int fps = 8)
        : frameWidth(frameW), frameHeight(frameH), totalFrames(frames),
          fps(fps), currentFrame(0), lastUpdate(0), running(true) {
        framesArray = new IMAGE[totalFrames];
        LoadSpriteSheetFrames(framesArray, totalFrames, sheetFile, frameWidth, frameHeight);
    }
    ~AnimatedBG() { delete[] framesArray; }

    void Show() {
        ULONGLONG now = GetTickCount();
        if (now - lastUpdate >= 1000 / fps) {
            currentFrame = (currentFrame + 1) % totalFrames;
            lastUpdate = now;
        }
        putimage(0, 0, &framesArray[currentFrame]);
    }

    // 只显示当前帧，不自动更新 (用于绘制前获取当前帧)
    void ShowStatic() {
        putimage(0, 0, &framesArray[currentFrame]);
    }

    int GetCurrentFrame() const { return currentFrame; }
    void SetFrame(int f) { currentFrame = f % totalFrames; }
    IMAGE* GetCurrentImage() { return &framesArray[currentFrame]; }
    int GetWidth() const { return frameWidth; }
    int GetHeight() const { return frameHeight; }

private:
    IMAGE* framesArray;
    int frameWidth, frameHeight;
    int totalFrames, fps;
    int currentFrame;
    ULONGLONG lastUpdate;
    bool running;
};

// 通用精灵图动画（带掩码，用于敌机）
// frames 表示普通帧数量，attackFrame >= 0 时表示额外提供一个攻击单帧
class AnimatedSprite {
public:
    AnimatedSprite(LPCTSTR sheetFile, int frameW, int frameH, int frames, int fps, LPCTSTR attackFile = nullptr) {
        frameWidth = frameW; frameHeight = frameH; totalFrames = frames;
        this->fps = fps; currentFrame = 0; lastUpdate = 0;
        attackActive = false; attackUntil = 0;
        srcFrames = new IMAGE[totalFrames];
        maskFrames = new IMAGE[totalFrames];
        tintedSrcFrames = new IMAGE[totalFrames];
        tintedMaskFrames = new IMAGE[totalFrames];
        LoadSpriteSheetFrames(srcFrames, totalFrames, sheetFile, frameWidth, frameHeight);
        // 生成掩码
        for (int i = 0; i < totalFrames; i++) {
            maskFrames[i].Resize(frameWidth, frameHeight);
            DWORD* srcBuf = GetImageBuffer(&srcFrames[i]);
            DWORD* maskBuf = GetImageBuffer(&maskFrames[i]);
            DWORD keyColor = srcBuf[0] & 0x00FFFFFF;
            for (int j = 0; j < frameWidth * frameHeight; j++) {
                DWORD c = srcBuf[j] & 0x00FFFFFF;
                int dr = abs((int)((c >> 16) & 255) - (int)((keyColor >> 16) & 255));
                int dg = abs((int)((c >> 8) & 255) - (int)((keyColor >> 8) & 255));
                int db = abs((int)(c & 255) - (int)(keyColor & 255));
                if (dr + dg + db < 36) {
                    maskBuf[j] = WHITE;
                    srcBuf[j] = BLACK;
                } else {
                    maskBuf[j] = BLACK;
                }
            }
        }
        // 预烘焙偏白色版（敌机被击中用）
        for (int i = 0; i < totalFrames; i++) {
            MakeTintedCopy(&srcFrames[i], &maskFrames[i], &tintedSrcFrames[i], &tintedMaskFrames[i], 1);
        }
        if (attackFile) {
            attackSrc.Resize(frameWidth, frameHeight);
            attackMask.Resize(frameWidth, frameHeight);
            loadimage(&attackSrc, attackFile);
            DWORD* srcBuf = GetImageBuffer(&attackSrc);
            DWORD* maskBuf = GetImageBuffer(&attackMask);
            DWORD keyColor = srcBuf[0] & 0x00FFFFFF;
            for (int j = 0; j < frameWidth * frameHeight; j++) {
                DWORD c = srcBuf[j] & 0x00FFFFFF;
                int dr = abs((int)((c >> 16) & 255) - (int)((keyColor >> 16) & 255));
                int dg = abs((int)((c >> 8) & 255) - (int)((keyColor >> 8) & 255));
                int db = abs((int)(c & 255) - (int)(keyColor & 255));
                if (dr + dg + db < 36) {
                    maskBuf[j] = WHITE;
                    srcBuf[j] = BLACK;
                } else {
                    maskBuf[j] = BLACK;
                }
            }
        }
    }
    ~AnimatedSprite() { delete[] srcFrames; delete[] maskFrames; delete[] tintedSrcFrames; delete[] tintedMaskFrames; }

    void TriggerAttackFlash(ULONGLONG durationMs) {
        attackActive = true;
        attackUntil = GetTickCount() + durationMs;
    }

    bool IsInAttackFlash() const { return attackActive && GetTickCount() < attackUntil; }

    // 推进普通动画帧；攻击闪帧期间冻结在攻击帧
    void Update() {
        if (attackActive) {
            if (GetTickCount() >= attackUntil) attackActive = false;
            else return;
        }
        ULONGLONG now = GetTickCount();
        if (now - lastUpdate >= 1000 / fps) {
            currentFrame = (currentFrame + 1) % totalFrames;
            lastUpdate = now;
        }
    }

    // 绘制到指定位置。攻击闪帧期间绘制攻击单帧
    void Draw(int x, int y, int tintMode = -1) {
        if (attackActive) putimage_mask(x, y, &attackSrc, &attackMask);
        else if (tintMode == 1 && currentFrame >= 0) {
            putimage_mask(x, y, &tintedSrcFrames[currentFrame], &tintedMaskFrames[currentFrame]);
        }
        else putimage_mask(x, y, &srcFrames[currentFrame], &maskFrames[currentFrame]);
    }

    int GetWidth() const { return frameWidth; }
    int GetHeight() const { return frameHeight; }

private:
    int frameWidth, frameHeight, totalFrames, fps;
    IMAGE* srcFrames;
    IMAGE* maskFrames;
    IMAGE* tintedSrcFrames;
    IMAGE* tintedMaskFrames;
    IMAGE attackSrc, attackMask;
    int currentFrame;
    ULONGLONG lastUpdate;
    bool attackActive;
    ULONGLONG attackUntil;
};

// 爆炸精灵图（带掩码，绘制指定帧）
class BoomSheet {
public:
    BoomSheet(LPCTSTR sheetFile, int frameW, int frameH, int frames) {
        frameWidth = frameW; frameHeight = frameH; totalFrames = frames;
        srcFrames = new IMAGE[totalFrames];
        maskFrames = new IMAGE[totalFrames];
        LoadSpriteSheetFrames(srcFrames, totalFrames, sheetFile, frameWidth, frameHeight);
        for (int i = 0; i < totalFrames; i++) {
            maskFrames[i].Resize(frameWidth, frameHeight);
            DWORD* srcBuf = GetImageBuffer(&srcFrames[i]);
            DWORD* maskBuf = GetImageBuffer(&maskFrames[i]);
            DWORD keyColor = srcBuf[0] & 0x00FFFFFF;
            for (int j = 0; j < frameWidth * frameHeight; j++) {
                DWORD c = srcBuf[j] & 0x00FFFFFF;
                int dr = abs((int)((c >> 16) & 255) - (int)((keyColor >> 16) & 255));
                int dg = abs((int)((c >> 8) & 255) - (int)((keyColor >> 8) & 255));
                int db = abs((int)(c & 255) - (int)(keyColor & 255));
                if (dr + dg + db < 36) {
                    maskBuf[j] = WHITE;
                    srcBuf[j] = BLACK;
                } else {
                    maskBuf[j] = BLACK;
                }
            }
        }
    }
    ~BoomSheet() { delete[] srcFrames; delete[] maskFrames; }
    int GetTotalFrames() const { return totalFrames; }
    int GetWidth() const { return frameWidth; }
    int GetHeight() const { return frameHeight; }
    void Draw(int x, int y, int frame) {
        if (frame < 0 || frame >= totalFrames) return;
        putimage_mask(x, y, &srcFrames[frame], &maskFrames[frame]);
    }
private:
    int frameWidth, frameHeight, totalFrames;
    IMAGE* srcFrames;
    IMAGE* maskFrames;
};

// 全局精灵图实例（敌机1、2 + 攻击图 + 通用小怪爆炸）
static AnimatedSprite* gEnemy1Sprite = nullptr;
static AnimatedSprite* gEnemy2Sprite = nullptr;
static AnimatedSprite* gBossSprite = nullptr;
static BoomSheet* gSmallBoom = nullptr;

// 玩家飞机精灵表动画类
class AnimatedPlayer {
public:
    AnimatedPlayer(LPCTSTR sheetFile, int frameW, int frameH, int frames, int fps = 10)
        : frameWidth(frameW), frameHeight(frameH), totalFrames(frames),
          fps(fps), currentFrame(0), lastUpdate(0) {
        srcFrames = new IMAGE[totalFrames];
        maskFrames = new IMAGE[totalFrames];
        LoadSpriteSheetFrames(srcFrames, totalFrames, sheetFile, frameWidth, frameHeight);
        
        // 生成掩码
        for (int i = 0; i < totalFrames; i++) {
            maskFrames[i].Resize(frameWidth, frameHeight);
            DWORD* srcBuf = GetImageBuffer(&srcFrames[i]);
            DWORD* maskBuf = GetImageBuffer(&maskFrames[i]);
            DWORD keyColor = srcBuf[0] & 0x00FFFFFF;
            for (int j = 0; j < frameWidth * frameHeight; j++) {
                DWORD c = srcBuf[j] & 0x00FFFFFF;
                int dr = abs((int)((c >> 16) & 255) - (int)((keyColor >> 16) & 255));
                int dg = abs((int)((c >> 8) & 255) - (int)((keyColor >> 8) & 255));
                int db = abs((int)(c & 255) - (int)(keyColor & 255));
                if (dr + dg + db < 36) {
                    maskBuf[j] = WHITE;
                    srcBuf[j] = BLACK;
                } else {
                    maskBuf[j] = BLACK;
                }
            }
        }
    }
    ~AnimatedPlayer() { delete[] srcFrames; delete[] maskFrames; }
    
    void Update() {
        ULONGLONG now = GetTickCount();
        if (now - lastUpdate >= 1000 / fps) {
            currentFrame = (currentFrame + 1) % totalFrames;
            lastUpdate = now;
        }
    }
    
    void Show(int x, int y) {
        putimage_mask(x, y, &srcFrames[currentFrame], &maskFrames[currentFrame]);
    }
    
    // 强制显示指定帧
    void ShowFrame(int x, int y, int frame) {
        int f = frame % totalFrames;
        putimage_mask(x, y, &srcFrames[f], &maskFrames[f]);
    }
    
    int GetWidth() const { return frameWidth; }
    int GetHeight() const { return frameHeight; }
    int GetFrameCount() const { return totalFrames; }
    
private:
    IMAGE* srcFrames;
    IMAGE* maskFrames;
    int frameWidth, frameHeight;
    int totalFrames, fps;
    int currentFrame;
    ULONGLONG lastUpdate;
};

void load_power_icon(IMAGE& src, IMAGE& mask, LPCTSTR file, int size=48) {
    loadimage(&src,file,size,size);
    // loadimage(..., NULL, ...) does not allocate a writable image buffer in
    // current EasyX versions.  Resize explicitly before obtaining the buffer.
    mask.Resize(size,size);
    DWORD* sb=GetImageBuffer(&src);
    DWORD* mb=GetImageBuffer(&mask);
    if (sb == nullptr || mb == nullptr) return;
    DWORD key=sb[0]&0x00FFFFFF;
    for (int i=0;i<size*size;i++) {
        DWORD c=sb[i]&0x00FFFFFF;
        int dr=abs((int)((c>>16)&255)-(int)((key>>16)&255));
        int dg=abs((int)((c>>8)&255)-(int)((key>>8)&255));
        int db=abs((int)(c&255)-(int)(key&255));
        if (dr+dg+db<36) { mb[i]=WHITE; sb[i]=BLACK; }
        else mb[i]=BLACK;
    }
}

class BK {
public:
    BK(IMAGE& img) :img(img), y(-HEIGHT) {}
    void Show() {
        if (y == 0) y = -HEIGHT;
        y += 1; putimage(0, y, &img);
    }
private:
    IMAGE& img; int y;
};

struct Ptcl { float x,y,vx,vy; int life,lmax; COLORREF col; };

void SpawnParticles(vector<Ptcl>& particles,int x,int y,COLORREF color,int count,int life=420) {
    if (particles.size()>500) particles.erase(particles.begin(),particles.begin()+particles.size()/3);
    for (int i=0;i<count;i++) {
        float angle=(rand()%360)*PI/180.0f;
        float speed=(rand()%35+15)/10.0f;
        Ptcl p;
        p.x=(float)x;p.y=(float)y;
        p.vx=cosf(angle)*speed;p.vy=sinf(angle)*speed;
        p.life=life+rand()%180;p.lmax=p.life;p.col=color;
        particles.push_back(p);
    }
}

void ShowParticles(vector<Ptcl>& particles) {
    for (int i=(int)particles.size()-1;i>=0;i--) {
        Ptcl& p=particles[i];
        p.x+=p.vx;p.y+=p.vy;p.vx*=0.98f;p.vy=p.vy*0.98f+0.025f;p.life-=16;
        if (p.life<=0) { particles.erase(particles.begin()+i);continue; }
        int size=max(1,(int)(4.0f*p.life/p.lmax));
        setfillcolor(p.col);solidcircle((int)p.x,(int)p.y,size);
    }
}

// ============ 哭哭小喵：被击败小怪时随抛物线飞出屏幕 ============
struct DeadCat {
    float x, y, vx, vy;
    int life, lmax;
    IMAGE* img;
    IMAGE* img_mask;
    float rot;        // 当前旋转角（弧度）
    float rotV;       // 角速度
};
void UpdateAndShowDeadCats(vector<DeadCat>& cats, IMAGE* defaultImg, IMAGE* defaultMask) {
    for (int i = (int)cats.size() - 1; i >= 0; i--) {
        DeadCat& c = cats[i];
        // 重力 / 空气阻力
        c.vy = c.vy * 0.997f + 0.18f;
        c.vx *= 0.997f;
        c.x += c.vx;
        c.y += c.vy;
        c.rot += c.rotV;
        c.life -= 16;
        IMAGE* img = c.img ? c.img : defaultImg;
        IMAGE* mask = c.img_mask ? c.img_mask : defaultMask;
        if (img && mask) {
            int ix = (int)lroundf(c.x);
            int iy = (int)lroundf(c.y);
            if (c.rot == 0.0f) {
                putimage_mask(ix, iy, img, mask);
            } else {
                IMAGE tmp; tmp.Resize(img->getwidth(), img->getheight());
                IMAGE tmpMask; tmpMask.Resize(mask->getwidth(), mask->getheight());
                // src 透明区=黑,机身=彩色 → 旋转空白填黑(等同透明区)
                // mask 透明区=白,机身=黑 → 旋转空白填白(等同透明区),避免黑边
                rotateimage(&tmp, img, c.rot, BLACK, false, false);
                rotateimage(&tmpMask, mask, c.rot, WHITE, false, false);
                putimage_mask(ix, iy, &tmp, &tmpMask);
            }
        }
        // 飞出屏幕或超时则清除
        if (c.life <= 0 || c.y > HEIGHT + 80 || c.x < -80 || c.x > WIDTH + 80) {
            cats.erase(cats.begin() + i);
        }
    }
}

class ME {
public:
    ME(IMAGE& img, IMAGE& img_m) :img(img), img_mask(img_m), HP(SUMHP), hurtTime(0) {
        rect.left=WIDTH/2-img.getwidth()/2;rect.right=rect.left+img.getwidth();
        rect.top=HEIGHT-img.getheight();rect.bottom=HEIGHT;
        tilt=0;
        // 为旋转准备一块离屏缓冲（一次性分配）
        rotated.Resize(img.getwidth()*2, img.getheight()*2);
        rotated_mask.Resize(img.getwidth()*2, img.getheight()*2);
        // 一次性预烘焙偏红版（被击中用）
        MakeTintedCopy(&img, &img_mask, &tintedSrc, &tintedMask, 0);
    }
    RECT& GetRect(){return rect;}
    void Show(){
        for(int i=(int)pts.size()-1;i>=0;i--){
            pts[i].x+=pts[i].vx;pts[i].y+=pts[i].vy;pts[i].life-=16;
            if(pts[i].life<=0){pts.erase(pts.begin()+i);continue;}
            int sz=(int)(5.0f*pts[i].life/pts[i].lmax)+1;
            setfillcolor(pts[i].col);solidcircle((int)pts[i].x,(int)pts[i].y,sz);
        }
        ULONGLONG now=GetTickCount();
        bool visible=hurtTime==0||now-hurtTime>=hurttime||(now/100)%2==0;
        bool stillHurt = hurtTime!=0 && now-hurtTime<hurttime;
        if(HP>0&&visible){
            // 横向倾斜：根据 tilt 旋转贴图绘制，rect 本身不变（保持原碰撞大小）
            int w=img.getwidth(), h=img.getheight();
            int cx=(rect.left+rect.right)/2, cy=(rect.top+rect.bottom)/2;
            IMAGE* useSrc = stillHurt ? &tintedSrc : &img;
            IMAGE* useMask= stillHurt ? &tintedMask: &img_mask;
            if(fabsf(tilt) < 0.005f){
                putimage_mask(rect.left, rect.top, useSrc, useMask);
            } else {
                // src 透明区=黑,机身=彩色 → 旋转空白填黑(等同透明区)
                // mask 透明区=白,机身=黑 → 旋转空白填白(等同透明区),避免黑边
                rotateimage(&rotated, useSrc, tilt, BLACK, false, false);
                rotateimage(&rotated_mask, useMask, tilt, WHITE, false, false);
                putimage_mask(cx - rotated.getwidth()/2, cy - rotated.getheight()/2, &rotated, &rotated_mask);
            }
        }
        if(HP>0&&shieldCharges>0){
            int cx=(rect.left+rect.right)/2,cy=(rect.top+rect.bottom)/2;
            int radius=(rect.right-rect.left)/2+12;
            setlinecolor(RGB(40,220,255));setlinestyle(PS_SOLID,3);
            circle(cx,cy,radius);setlinestyle(PS_SOLID,1);
            TCHAR n[8];_stprintf_s(n,8,_T("%d"),shieldCharges);
            settextcolor(RGB(180,250,255));settextstyle(16,0,_T("Consolas"));outtextxy(cx+radius-8,cy-radius,n);
        }
    }
    void Control(bool allowArrowKeys=true){
        int speed=PLAYER_SPEED+moveBonus;
        if((GetAsyncKeyState('W')&0x8000)||(allowArrowKeys&&(GetAsyncKeyState(VK_UP)&0x8000))){rect.top-=speed;rect.bottom-=speed;}
        if((GetAsyncKeyState('S')&0x8000)||(allowArrowKeys&&(GetAsyncKeyState(VK_DOWN)&0x8000))){rect.top+=speed;rect.bottom+=speed;}
        if((GetAsyncKeyState('A')&0x8000)||(allowArrowKeys&&(GetAsyncKeyState(VK_LEFT)&0x8000))){rect.left-=speed;rect.right-=speed;tilt+=0.04f;}
        if((GetAsyncKeyState('D')&0x8000)||(allowArrowKeys&&(GetAsyncKeyState(VK_RIGHT)&0x8000))){rect.left+=speed;rect.right+=speed;tilt-=0.04f;}
        if(tilt > TILT_MAX) tilt = TILT_MAX;
        if(tilt < -TILT_MAX) tilt = -TILT_MAX;
        // 没按横向键时缓动回 0
        bool leftHeld=(GetAsyncKeyState('A')&0x8000)||(allowArrowKeys&&(GetAsyncKeyState(VK_LEFT)&0x8000));
        bool rightHeld=(GetAsyncKeyState('D')&0x8000)||(allowArrowKeys&&(GetAsyncKeyState(VK_RIGHT)&0x8000));
        if(!leftHeld && !rightHeld){
            if(tilt > 0) tilt = max(0.0f, tilt - 0.025f);
            else if(tilt < 0) tilt = min(0.0f, tilt + 0.025f);
        }
        Clamp();
    }
    void Control2(){
        int speed=PLAYER_SPEED+moveBonus;
        if(GetAsyncKeyState(VK_UP)&0x8000){rect.top-=speed;rect.bottom-=speed;}
        if(GetAsyncKeyState(VK_DOWN)&0x8000){rect.top+=speed;rect.bottom+=speed;}
        if(GetAsyncKeyState(VK_LEFT)&0x8000){rect.left-=speed;rect.right-=speed;tilt+=0.04f;}
        if(GetAsyncKeyState(VK_RIGHT)&0x8000){rect.left+=speed;rect.right+=speed;tilt-=0.04f;}
        if(tilt > TILT_MAX) tilt = TILT_MAX;
        if(tilt < -TILT_MAX) tilt = -TILT_MAX;
        bool leftHeld=(GetAsyncKeyState(VK_LEFT)&0x8000);
        bool rightHeld=(GetAsyncKeyState(VK_RIGHT)&0x8000);
        if(!leftHeld && !rightHeld){
            if(tilt > 0) tilt = max(0.0f, tilt - 0.025f);
            else if(tilt < 0) tilt = min(0.0f, tilt + 0.025f);
        }
        Clamp();
    }
    void MoveCenterTo(int x,int y){
        int w=rect.right-rect.left,h=rect.bottom-rect.top;
        rect.left=x-w/2;rect.right=rect.left+w;rect.top=y-h/2;rect.bottom=rect.top+h;Clamp();
    }
    void Clamp(){
        int w=rect.right-rect.left,h=rect.bottom-rect.top;
        if(rect.left<0){rect.left=0;rect.right=w;}if(rect.right>WIDTH){rect.right=WIDTH;rect.left=WIDTH-w;}
        if(rect.top<0){rect.top=0;rect.bottom=h;}if(rect.bottom>HEIGHT){rect.bottom=HEIGHT;rect.top=HEIGHT-h;}
    }
    bool hurt(){
        hurtTime=GetTickCount();
        if(shieldCharges>0){shieldCharges--;return true;}
        if(HP>0)HP--;
        for(int i=0;i<15;i++){
            Ptcl p;p.x=(float)(rect.left+rand()%max(1,rect.right-rect.left));p.y=(float)(rect.top+rand()%max(1,rect.bottom-rect.top));
            float ang=(rand()%360)*PI/180.0f,spd=(rand()%5+2)*0.6f;
            p.vx=cosf(ang)*spd;p.vy=sinf(ang)*spd;p.life=800;p.lmax=800;p.col=RGB(rand()%256,rand()%256,rand()%256);pts.push_back(p);
        }
        return HP>0;
    }
    int GetHP()const{return (int)HP;}
    void Heal(){if(HP<SUMHP)HP++;}
    void GrantShield(){if(shieldCharges<shieldCapacity)shieldCharges++;}
    bool HasShield()const{return shieldCharges>0;}
    int GetShieldCount()const{return shieldCharges;}
    void SetUpgradeBonuses(int newMoveBonus,int newShieldCapacity){
        moveBonus=max(0,min(4,newMoveBonus));shieldCapacity=max(1,min(4,newShieldCapacity));
        shieldCharges=min(shieldCharges,shieldCapacity);
    }
    void Revive(int reviveHp=2){
        HP=(unsigned)max(1,min((int)SUMHP,reviveHp));hurtTime=0;
        int h=rect.bottom-rect.top;rect.top=HEIGHT-h;rect.bottom=HEIGHT;Clamp();
    }
    void Restore(int savedHp,bool savedShield,const RECT& savedRect,int savedShieldCount=-1){
        HP=(unsigned)max(0,min((int)SUMHP,savedHp));
        shieldCharges=savedShieldCount>=0?min(shieldCapacity,savedShieldCount):(savedShield?1:0);
        rect=savedRect;Clamp();hurtTime=0;
    }
private:
    static constexpr float TILT_MAX = 0.20f;  // ≈ 11.5°
    IMAGE &img,&img_mask;RECT rect;unsigned int HP;ULONGLONG hurtTime;
    int shieldCharges=0,shieldCapacity=1,moveBonus=0;vector<Ptcl> pts;
    float tilt = 0.0f;
    IMAGE rotated, rotated_mask;
    IMAGE tintedSrc, tintedMask;
};

// ============ Animated Player (Sprite Sheet) ============
class AnimatedME {
public:
    AnimatedME(LPCTSTR sheetFile, int frameW, int frameH, int frames, int fps = 12)
        : frameWidth(frameW), frameHeight(frameH), totalFrames(frames),
          fps(fps), currentFrame(0), lastUpdate(0), HP(SUMHP), hurtTime(0),
          shieldCharges(0), shieldCapacity(1), moveBonus(0), tilt(0) {
        srcFrames = new IMAGE[totalFrames];
        maskFrames = new IMAGE[totalFrames];
        tintedSrcFrames = new IMAGE[totalFrames];
        tintedMaskFrames = new IMAGE[totalFrames];
        LoadSpriteSheetFrames(srcFrames, totalFrames, sheetFile, frameWidth, frameHeight);

        // 生成掩码
        for (int i = 0; i < totalFrames; i++) {
            maskFrames[i].Resize(frameWidth, frameHeight);
            DWORD* srcBuf = GetImageBuffer(&srcFrames[i]);
            DWORD* maskBuf = GetImageBuffer(&maskFrames[i]);
            DWORD keyColor = srcBuf[0] & 0x00FFFFFF;
            for (int j = 0; j < frameWidth * frameHeight; j++) {
                DWORD c = srcBuf[j] & 0x00FFFFFF;
                int dr = abs((int)((c >> 16) & 255) - (int)((keyColor >> 16) & 255));
                int dg = abs((int)((c >> 8) & 255) - (int)((keyColor >> 8) & 255));
                int db = abs((int)(c & 255) - (int)(keyColor & 255));
                if (dr + dg + db < 36) {
                    maskBuf[j] = WHITE;
                    srcBuf[j] = BLACK;
                } else {
                    maskBuf[j] = BLACK;
                }
            }
        }

        // 预烘焙偏红版：每一帧复制 srcFrames/maskFrames → tintedSrcFrames/tintedMaskFrames
        for (int i = 0; i < totalFrames; i++) {
            MakeTintedCopy(&srcFrames[i], &maskFrames[i], &tintedSrcFrames[i], &tintedMaskFrames[i], 0);
        }

        // 为倾斜旋转准备离屏缓冲（一次性分配，原尺寸 * 2 容纳旋转后的范围）
        rotated.Resize(frameWidth * 2, frameHeight * 2);
        rotatedMask.Resize(frameWidth * 2, frameHeight * 2);

        // 初始化位置
        rect.left = WIDTH / 2 - frameWidth / 2;
        rect.right = rect.left + frameWidth;
        rect.top = HEIGHT - frameHeight;
        rect.bottom = HEIGHT;
    }
    ~AnimatedME() { delete[] srcFrames; delete[] maskFrames; delete[] tintedSrcFrames; delete[] tintedMaskFrames; }
    
    RECT& GetRect() { return rect; }
    int GetWidth() const { return frameWidth; }
    int GetHeight() const { return frameHeight; }
    
    void Update() {
        // 更新动画帧
        ULONGLONG now = GetTickCount();
        if (now - lastUpdate >= 1000 / fps) {
            currentFrame = (currentFrame + 1) % totalFrames;
            lastUpdate = now;
        }
        // 更新粒子
        for (int i = (int)pts.size() - 1; i >= 0; i--) {
            pts[i].x += pts[i].vx;
            pts[i].y += pts[i].vy;
            pts[i].life -= 16;
            if (pts[i].life <= 0) { pts.erase(pts.begin() + i); continue; }
            int sz = (int)(5.0f * pts[i].life / pts[i].lmax) + 1;
            setfillcolor(pts[i].col);
            solidcircle((int)pts[i].x, (int)pts[i].y, sz);
        }
    }
    
    void Show() {
        ULONGLONG now = GetTickCount();
        bool visible = hurtTime == 0 || now - hurtTime >= hurttime || (now / 100) % 2 == 0;
        bool stillHurt = hurtTime != 0 && now - hurtTime < hurttime;
        if (HP > 0 && visible) {
            int cx = (rect.left + rect.right) / 2, cy = (rect.top + rect.bottom) / 2;
            IMAGE* src = stillHurt ? &tintedSrcFrames[currentFrame] : &srcFrames[currentFrame];
            IMAGE* msk = stillHurt ? &tintedMaskFrames[currentFrame] : &maskFrames[currentFrame];
            if (fabsf(tilt) < 0.005f) {
                putimage_mask(rect.left, rect.top, src, msk);
            } else {
                rotateimage(&rotated, src, tilt, BLACK, false, false);
                rotateimage(&rotatedMask, msk, tilt, WHITE, false, false);
                putimage_mask(cx - rotated.getwidth() / 2, cy - rotated.getheight() / 2, &rotated, &rotatedMask);
            }
        }
        if (HP > 0 && shieldCharges > 0) {
            int cx = (rect.left + rect.right) / 2, cy = (rect.top + rect.bottom) / 2;
            int radius = (rect.right - rect.left) / 2 + 12;
            setlinecolor(RGB(40, 220, 255));
            setlinestyle(PS_SOLID, 3);
            circle(cx, cy, radius);
            setlinestyle(PS_SOLID, 1);
            TCHAR n[8];
            _stprintf_s(n, 8, _T("%d"), shieldCharges);
            settextcolor(RGB(180, 250, 255));
            settextstyle(16, 0, _T("Consolas"));
            outtextxy(cx + radius - 8, cy - radius, n);
        }
    }

    void Control(bool allowArrowKeys = true) {
        int speed = PLAYER_SPEED + moveBonus;
        if ((GetAsyncKeyState('W') & 0x8000) || (allowArrowKeys && (GetAsyncKeyState(VK_UP) & 0x8000))) { rect.top -= speed; rect.bottom -= speed; }
        if ((GetAsyncKeyState('S') & 0x8000) || (allowArrowKeys && (GetAsyncKeyState(VK_DOWN) & 0x8000))) { rect.top += speed; rect.bottom += speed; }
        if ((GetAsyncKeyState('A') & 0x8000) || (allowArrowKeys && (GetAsyncKeyState(VK_LEFT) & 0x8000))) { rect.left -= speed; rect.right -= speed; tilt += 0.04f; }
        if ((GetAsyncKeyState('D') & 0x8000) || (allowArrowKeys && (GetAsyncKeyState(VK_RIGHT) & 0x8000))) { rect.left += speed; rect.right += speed; tilt -= 0.04f; }
        if (tilt > TILT_MAX) tilt = TILT_MAX;
        if (tilt < -TILT_MAX) tilt = -TILT_MAX;
        bool leftHeld = (GetAsyncKeyState('A') & 0x8000) || (allowArrowKeys && (GetAsyncKeyState(VK_LEFT) & 0x8000));
        bool rightHeld = (GetAsyncKeyState('D') & 0x8000) || (allowArrowKeys && (GetAsyncKeyState(VK_RIGHT) & 0x8000));
        if (!leftHeld && !rightHeld) {
            if (tilt > 0) tilt = max(0.0f, tilt - 0.025f);
            else if (tilt < 0) tilt = min(0.0f, tilt + 0.025f);
        }
        Clamp();
    }

    void Control2() {
        int speed = PLAYER_SPEED + moveBonus;
        if (GetAsyncKeyState(VK_UP) & 0x8000) { rect.top -= speed; rect.bottom -= speed; }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) { rect.top += speed; rect.bottom += speed; }
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) { rect.left -= speed; rect.right -= speed; tilt += 0.04f; }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { rect.left += speed; rect.right += speed; tilt -= 0.04f; }
        if (tilt > TILT_MAX) tilt = TILT_MAX;
        if (tilt < -TILT_MAX) tilt = -TILT_MAX;
        bool leftHeld = (GetAsyncKeyState(VK_LEFT) & 0x8000);
        bool rightHeld = (GetAsyncKeyState(VK_RIGHT) & 0x8000);
        if (!leftHeld && !rightHeld) {
            if (tilt > 0) tilt = max(0.0f, tilt - 0.025f);
            else if (tilt < 0) tilt = min(0.0f, tilt + 0.025f);
        }
        Clamp();
    }
    
    void MoveCenterTo(int x, int y) {
        int w = rect.right - rect.left, h = rect.bottom - rect.top;
        rect.left = x - w / 2; rect.right = rect.left + w; rect.top = y - h / 2; rect.bottom = rect.top + h;
        Clamp();
    }
    
    void Clamp() {
        int w = rect.right - rect.left, h = rect.bottom - rect.top;
        if (rect.left < 0) { rect.left = 0; rect.right = w; }
        if (rect.right > WIDTH) { rect.right = WIDTH; rect.left = WIDTH - w; }
        if (rect.top < 0) { rect.top = 0; rect.bottom = h; }
        if (rect.bottom > HEIGHT) { rect.bottom = HEIGHT; rect.top = HEIGHT - h; }
    }
    
    bool hurt() {
        hurtTime = GetTickCount();
        if (shieldCharges > 0) { shieldCharges--; return true; }
        if (HP > 0) HP--;
        for (int i = 0; i < 15; i++) {
            Ptcl p;
            p.x = (float)(rect.left + rand() % max(1, rect.right - rect.left));
            p.y = (float)(rect.top + rand() % max(1, rect.bottom - rect.top));
            float ang = (rand() % 360) * PI / 180.0f, spd = (rand() % 5 + 2) * 0.6f;
            p.vx = cosf(ang) * spd;
            p.vy = sinf(ang) * spd;
            p.life = 800; p.lmax = 800;
            p.col = RGB(rand() % 256, rand() % 256, rand() % 256);
            pts.push_back(p);
        }
        return HP > 0;
    }
    
    int GetHP() const { return (int)HP; }
    void Heal() { if (HP < SUMHP) HP++; }
    void GrantShield() { if (shieldCharges < shieldCapacity) shieldCharges++; }
    bool HasShield() const { return shieldCharges > 0; }
    int GetShieldCount() const { return shieldCharges; }
    void SetUpgradeBonuses(int newMoveBonus, int newShieldCapacity) {
        moveBonus = max(0, min(4, newMoveBonus));
        shieldCapacity = max(1, min(4, newShieldCapacity));
        shieldCharges = min(shieldCharges, shieldCapacity);
    }
    void Revive(int reviveHp = 2) {
        HP = (unsigned)max(1, min((int)SUMHP, reviveHp));
        hurtTime = 0;
        int h = rect.bottom - rect.top;
        rect.top = HEIGHT - h; rect.bottom = HEIGHT;
        Clamp();
    }
    void Restore(int savedHp, bool savedShield, const RECT& savedRect, int savedShieldCount = -1) {
        HP = (unsigned)max(0, min((int)SUMHP, savedHp));
        shieldCharges = savedShieldCount >= 0 ? min(shieldCapacity, savedShieldCount) : (savedShield ? 1 : 0);
        rect = savedRect;
        Clamp();
        hurtTime = 0;
    }
    
private:
    IMAGE* srcFrames;
    IMAGE* maskFrames;
    IMAGE* tintedSrcFrames;
    IMAGE* tintedMaskFrames;
    int frameWidth, frameHeight;
    int totalFrames, fps;
    int currentFrame;
    ULONGLONG lastUpdate;
    RECT rect;
    unsigned int HP;
    ULONGLONG hurtTime;
    int shieldCharges, shieldCapacity, moveBonus;
    vector<Ptcl> pts;
    static constexpr float TILT_MAX = 0.20f;   // ≈ 11.5°
    float tilt = 0.0f;
    IMAGE rotated;
    IMAGE rotatedMask;
};

class WINGMAN {
public:
    WINGMAN(IMAGE& image,IMAGE& imageMask):img(image),mask(imageMask),x(0),y(0),visible(false) {}
    void Update(const RECT& owner,bool preferRight) {
        int w=img.getwidth(),h=img.getheight();
        float targetX=preferRight?(float)(owner.right+12):(float)(owner.left-w-12);
        if (targetX<0 || targetX+w>WIDTH)
            targetX=preferRight?(float)(owner.left-w-12):(float)(owner.right+12);
        float targetY=(float)(owner.top+(owner.bottom-owner.top-h)/2);
        if (!visible) { x=targetX;y=targetY;visible=true; }
        x+=(targetX-x)*0.18f;y+=(targetY-y)*0.18f;
        if (x<0)x=0;if(x+w>WIDTH)x=(float)(WIDTH-w);
        if (y<0)y=0;if(y+h>HEIGHT)y=(float)(HEIGHT-h);
        rect.left=(LONG)lroundf(x);rect.top=(LONG)lroundf(y);
        rect.right=rect.left+w;rect.bottom=rect.top+h;
    }
    void Show() { if (visible) putimage_mask(rect.left,rect.top,&img,&mask); }
    void Hide() { visible=false; }
    RECT GetRect() const { return rect; }
private:
    IMAGE& img;IMAGE& mask;float x,y;bool visible;RECT rect{};
};

bool MessInPoint(int x, int y, RECT& t) {
    return x>=t.left && x<=t.right && y>=t.top && y<=t.bottom;
}
bool CheckCrash(RECT& r1, RECT& r2) {
    return r1.left<r2.right && r1.right>r2.left && r1.top<r2.bottom && r1.bottom>r2.top;
}

// ============ Directional Bullet ============
class EBULLET {
public:
    enum KIND { NORMAL, LASER };
    IMAGE *img, *img_mask;
    RECT rect; float x, y, vx, vy; bool active, harmful;
    KIND kind; int age, warningFrames, activeFrames,damage,pierceLeft;
    bool grazedP1,grazedP2;
    EBULLET() : img(0), img_mask(0), x(0), y(0), vx(0), vy(0),
        active(false), harmful(false), kind(NORMAL), age(0), warningFrames(0), activeFrames(0),
        damage(1),pierceLeft(0),grazedP1(false),grazedP2(false) {}
    void Init(IMAGE& i, IMAGE& im, RECT pr, float dx, float dy, int spd) {
        img=&i; img_mask=&im; vx=dx*spd; vy=dy*spd; kind=NORMAL;
        x=(float)(pr.left+(pr.right-pr.left)/2-i.getwidth()/2);
        y=(float)pr.bottom;
        rect.left=(LONG)x;
        rect.right = rect.left+i.getwidth();
        rect.top=(LONG)y; rect.bottom=rect.top+i.getheight();
        active = true; harmful = true;
    }
    void InitDown(IMAGE& i, IMAGE& im, RECT pr, int spd=3, int forwardOffset=0) {
        // 让子弹从 enemy 底边往下 forwardOffset 像素生成，避免紧贴 enemy
        pr.top += forwardOffset; pr.bottom += forwardOffset;
        Init(i,im,pr,0,1,spd);
    }
    void InitLaser(int centerX, int startY, int warn=90, int duration=75) {
        kind=LASER; age=0; warningFrames=warn; activeFrames=duration;
        active=true; harmful=false;
        rect.left=centerX-13; rect.right=centerX+13;
        rect.top=startY; rect.bottom=HEIGHT;
    }
    bool Show(float movementScale=1.0f) {
        if (!active) return false;
        if (kind==LASER) {
            age++;
            harmful=age>warningFrames && age<=warningFrames+activeFrames;
            if (age>warningFrames+activeFrames) { active=false; harmful=false; return false; }
            int cx=(rect.left+rect.right)/2;
            if (!harmful) {
                // A thin flashing guide gives the player time to dodge.
                setlinecolor((age/8)%2?RGB(255,90,90):RGB(255,210,80));
                setlinestyle(PS_DASH,2); line(cx,rect.top,cx,rect.bottom);
                setlinestyle(PS_SOLID,1);
            } else {
                setfillcolor(RGB(255,70,50));
                solidrectangle(rect.left,rect.top,rect.right,rect.bottom);
                setfillcolor(RGB(255,245,190));
                solidrectangle(cx-4,rect.top,cx+4,rect.bottom);
            }
            return true;
        }
        if (rect.top>=HEIGHT||rect.bottom<0||rect.right<0||rect.left>WIDTH)
            { active=false; harmful=false; return false; }
        // Keep the precise position so diagonal components below one pixel do
        // not get truncated to zero every frame.
        x+=vx*movementScale; y+=vy*movementScale;
        rect.left=(LONG)lroundf(x); rect.right=rect.left+img->getwidth();
        rect.top=(LONG)lroundf(y); rect.bottom=rect.top+img->getheight();
        putimage_mask(rect.left, rect.top, img, img_mask);
        return true;
    }
    RECT& GetRect() { return rect; }
    bool IsHarmful() const { return active && harmful; }
    bool IsLaser() const { return kind==LASER; }
};

enum POWER_TYPE {
    POWER_FIRE,POWER_DRONE,POWER_LIFE,POWER_BOMB,POWER_SHIELD,
    POWER_MAGNET,POWER_FREEZE,POWER_PIERCE,POWER_OVERLOAD,POWER_REVIVE,POWER_COUNT
};
struct POWER_ART { IMAGE* image;IMAGE* mask; };

class POWERUP {
public:
    POWER_TYPE type; IMAGE *img,*mask; RECT rect; bool active;float fx,fy;
    POWERUP(POWER_TYPE t,IMAGE& i,IMAGE& m,RECT from):type(t),img(&i),mask(&m),active(true) {
        int x=(from.left+from.right)/2-i.getwidth()/2;
        if (x<0) x=0; if (x+i.getwidth()>WIDTH) x=WIDTH-i.getwidth();
        rect.left=x; rect.right=x+i.getwidth();
        rect.top=(from.top+from.bottom)/2-i.getheight()/2;
        rect.bottom=rect.top+i.getheight();
        fx=(float)rect.left;fy=(float)rect.top;
    }
    bool Show(bool magnetic=false,float targetX=0,float targetY=0) {
        if (!active || rect.top>=HEIGHT) return false;
        if(magnetic){
            float cx=fx+(rect.right-rect.left)/2.0f,cy=fy+(rect.bottom-rect.top)/2.0f;
            float dx=targetX-cx,dy=targetY-cy,dist=sqrtf(dx*dx+dy*dy);
            if(dist<240.0f&&dist>1.0f){fx+=dx/dist*5.0f;fy+=dy/dist*5.0f;}else fy+=1.0f;
        }else fy+=1.0f;
        rect.left=(LONG)lroundf(fx);rect.right=rect.left+img->getwidth();
        rect.top=(LONG)lroundf(fy);rect.bottom=rect.top+img->getheight();
        putimage_mask(rect.left,rect.top,img,mask); return true;
    }
    RECT& GetRect() { return rect; }
};

// ============ Abstract Enemy ============
class ENEMY {
public:
    // 敌人类型枚举：用于根据敌人种类选择对应的死亡贴图，避免随机错配
    enum Kind { KIND_TYPE1, KIND_TYPE2, KIND_TYPE4, KIND_TYPE5, KIND_FORMATION, KIND_BOSS };
    RECT rect; bool is_died; int boom_num, hp, maxHp, scoreVal, attackTimer;
    float targetX, targetY;
    ENEMY() : is_died(false), boom_num(0), hp(1), maxHp(1), scoreVal(1), attackTimer(0),
        targetX(WIDTH/2.0f), targetY(HEIGHT*0.8f),hitFlashUntil(0),explosionFrameAt(0) {}
    virtual ~ENEMY() {}
    virtual bool Show() = 0;
    void IsDied() { is_died=true; }
    RECT& GetRect() { return rect; }
    virtual bool CanAttack() = 0;
    int GetHP() { return hp; }
    int GetMaxHP() { return maxHp; }
    virtual bool IsBoss() { return false; }
    // 子类返回自己的类型（默认 TYPE1 兼容旧代码）
    virtual Kind GetKind() const { return KIND_TYPE1; }
    virtual int GetPhase() const { return 1; }
    virtual bool IsTransitioning() const { return false; }
    virtual void SetTarget(float x,float y) { targetX=x; targetY=y; }
    void TakeDamage(int damage=1) {
        if(is_died)return;hp-=max(1,damage);hitFlashUntil=GetTickCount()+95;PlayGameSound(SOUND_HIT);
        if(hp<=0){hp=0;is_died=true;PlayGameSound(SOUND_EXPLOSION);}
    }
    bool IsHitFlashing()const{return GetTickCount()<hitFlashUntil;}
    int ExplosionFrame(int totalFrames){
        ULONGLONG now=GetTickCount();if(explosionFrameAt==0)explosionFrameAt=now;
        if(now-explosionFrameAt>=70){boom_num+=(int)((now-explosionFrameAt)/70);explosionFrameAt=now;}
        return min(boom_num,totalFrames);
    }
    void DrawHealthBar() {
        if (is_died || IsBoss() || rect.bottom<0 || rect.top>HEIGHT) return;
        int planeWidth=rect.right-rect.left;
        int barWidth=min(70,max(24,planeWidth));
        int left=(rect.left+rect.right-barWidth)/2;
        int top=max(3,(int)rect.top-8);
        int fill=barWidth*max(0,hp)/maxHp;
        setfillcolor(RGB(55,55,65));solidrectangle(left,top,left+barWidth,top+4);
        setfillcolor(hp*3>maxHp?RGB(70,220,90):RGB(235,70,55));
        if (fill>0) solidrectangle(left,top,left+fill,top+4);
    }
    void DrawFeedback(){
        DrawHealthBar();
    }
    virtual void Move() = 0;
    virtual void GetBullets(vector<EBULLET*>& ebs, IMAGE& bi, IMAGE& bm) {}
protected:
    ULONGLONG hitFlashUntil,explosionFrameAt;
};

// ============ Type1: basic, no attack (animated sprite sheet) ============
class ENEMY_TYPE1 : public ENEMY {
    int speed;
public:
    ENEMY_TYPE1(int level, int mode) {
        hp=maxHp=1+(level>=2?1:0)+(mode==2?1:0); scoreVal=10; speed=1+level;
        int w = gEnemy1Sprite->GetWidth();
        int x=abs(rand())%max(1,WIDTH-w);
        rect.left=x; rect.top=-gEnemy1Sprite->GetHeight();
        rect.right=x+w; rect.bottom=0;
    }
    bool Show() override {
        if (is_died) {
            int frame=ExplosionFrame(gSmallBoom->GetTotalFrames());
            if(frame>=gSmallBoom->GetTotalFrames())return false;
            gSmallBoom->Draw(rect.left, rect.top, frame);
            return true;
        }
        if (rect.top>=HEIGHT) return false;
        Move(); gEnemy1Sprite->Update(); gEnemy1Sprite->Draw(rect.left, rect.top, IsHitFlashing() ? 1 : -1);
        return true;
    }
    void Move() override { int step=max(1,(int)lroundf(speed*gEnemyMoveScale));rect.top+=step;rect.bottom+=step; }
    bool CanAttack() override { return false; }
    Kind GetKind() const override { return KIND_TYPE1; }
};

// ============ Type2: fast enemy (animated sprite sheet, attack flash frame) ============
class ENEMY_TYPE2 : public ENEMY {
    int speed, attackInterval;
public:
    ENEMY_TYPE2(int level, int mode) {
        hp=maxHp=2+(level-1)/2+(mode==2?1:0); scoreVal=20;
        speed=2+level+(level>=3?1:0); attackInterval=220-level*30;attackTimer=attackInterval;
        int w = gEnemy2Sprite->GetWidth();
        int x=abs(rand())%max(1,WIDTH-w);
        rect.left=x; rect.top=-gEnemy2Sprite->GetHeight();
        rect.right=x+w; rect.bottom=0;
    }
    bool Show() override {
        if (is_died) {
            int frame=ExplosionFrame(gSmallBoom->GetTotalFrames());
            if(frame>=gSmallBoom->GetTotalFrames())return false;
            gSmallBoom->Draw(rect.left, rect.top, frame);
            return true;
        }
        if (rect.top>=HEIGHT) return false;
        Move(); gEnemy2Sprite->Update(); gEnemy2Sprite->Draw(rect.left, rect.top, IsHitFlashing() ? 1 : -1);
        return true;
    }
    void Move() override { int step=max(1,(int)lroundf(speed*gEnemyMoveScale));rect.top+=step;rect.bottom+=step; }
    bool CanAttack() override { return true; }
    Kind GetKind() const override { return KIND_TYPE2; }
    void GetBullets(vector<EBULLET*>& ebs, IMAGE& bi, IMAGE& bm) override {
        attackTimer++;
        if (attackTimer>=attackInterval) {
            attackTimer=0;
            // 攻击瞬间播放 enemy2_attack.png（攻击闪一帧），下一帧立即恢复
            gEnemy2Sprite->TriggerAttackFlash(100);
            EBULLET* eb=new EBULLET();
            // enemy2 自己向下 speed 像素/帧；子弹若用默认 3 px/帧 在 L1 会跟 enemy 完全同速。
            // 用更快的速度 + 初始位置往前偏移 6 像素，让子弹明显脱离 enemy。
            // L1 enemy=3 / L2=4 / L3=5，bullet 速度 6 在所有关卡都比 enemy 快。
            eb->InitDown(bi,bm,rect,6,8);
            ebs.push_back(eb);
        }
    }
};

// ============ Type4: sine-wave enemy (introduced in level 2) ============
class ENEMY_TYPE4 : public ENEMY {
    IMAGE *pI, *pM, *pB, *pBM; int totalF; float centerX, fy, phase, waveSpeed, fallSpeed, amplitude;
public:
    ENEMY_TYPE4(IMAGE& i, IMAGE& im, IMAGE* b, IMAGE* bm, int level, int mode) {
        pI=&i; pM=&im; pB=b; pBM=bm; totalF=4;
        hp=maxHp=2+level/2+(mode==2?1:0); scoreVal=30;
        amplitude=70.0f+level*15.0f; waveSpeed=0.035f+level*0.004f; fallSpeed=1.25f+level*0.25f;
        if(level>=3)fallSpeed+=0.65f;attackTimer=190;
        centerX=(float)(i.getwidth()/2+amplitude+rand()%max(1,(int)(WIDTH-i.getwidth()-2*amplitude)));
        fy=(float)-i.getheight(); phase=(rand()%628)/100.0f;
        int x=(int)(centerX-i.getwidth()/2);
        rect.left=x; rect.top=(LONG)fy; rect.right=x+i.getwidth(); rect.bottom=rect.top+i.getheight();
    }
    bool Show() override {
        if (is_died) {
            int frame=ExplosionFrame(totalF);if(frame>=totalF)return false;
            putimage_mask(rect.left,rect.top,&pB[frame],&pBM[frame]);return true;
        }
        if (rect.top>=HEIGHT) return false;
        Move(); putimage_tinted(rect.left,rect.top,pI,pM, IsHitFlashing()?1:-1); return true;
    }
    void Move() override {
        phase+=waveSpeed*gEnemyMoveScale; fy+=fallSpeed*gEnemyMoveScale;
        int x=(int)lroundf(centerX+sin(phase)*amplitude-(rect.right-rect.left)/2.0f);
        int y=(int)lroundf(fy), w=rect.right-rect.left, h=rect.bottom-rect.top;
        rect.left=x; rect.right=x+w; rect.top=y; rect.bottom=y+h;
    }
    bool CanAttack() override { return true; }
    void GetBullets(vector<EBULLET*>& ebs, IMAGE& bi, IMAGE& bm) override {
        if (++attackTimer>=190) { attackTimer=0; EBULLET* eb=new EBULLET(); eb->InitDown(bi,bm,rect); ebs.push_back(eb); }
    }
    Kind GetKind() const override { return KIND_TYPE4; }
};

// ============ Type5: tracking enemy ============
class ENEMY_TYPE5 : public ENEMY {
    IMAGE *pI,*pM,*pB,*pBM; int totalF; float fx,fy,speed;
public:
    ENEMY_TYPE5(IMAGE& i,IMAGE& im,IMAGE* b,IMAGE* bm,int level,int mode) {
        pI=&i; pM=&im; pB=b; pBM=bm; totalF=4;
        hp=maxHp=3+level/2+(mode==2?1:0); scoreVal=40; speed=1.15f+level*0.18f;
        if(level>=3)speed+=0.45f;attackTimer=155;
        fx=(float)(rand()%max(1,WIDTH-i.getwidth())); fy=(float)-i.getheight();
        rect.left=(LONG)fx; rect.top=(LONG)fy; rect.right=rect.left+i.getwidth(); rect.bottom=rect.top+i.getheight();
    }
    bool Show() override {
        if(is_died){int frame=ExplosionFrame(totalF);if(frame>=totalF)return false;putimage_mask(rect.left,rect.top,&pB[frame],&pBM[frame]);return true;}
        if (rect.top>=HEIGHT) return false;
        Move(); putimage_tinted(rect.left,rect.top,pI,pM, IsHitFlashing()?1:-1); return true;
    }
    void Move() override {
        float cx=fx+(rect.right-rect.left)/2.0f;
        float dx=targetX-cx;
        float step=speed*gEnemyMoveScale;
        if (fabsf(dx)>step) fx+=(dx>0.0f?step:-step);
        else fx+=dx;
        // Track the player only on the X axis; descend at a constant speed on Y.
        fy+=step;
        int w=rect.right-rect.left,h=rect.bottom-rect.top;
        if (fx<0) fx=0; if (fx+w>WIDTH) fx=(float)(WIDTH-w);
        rect.left=(LONG)lroundf(fx); rect.right=rect.left+w; rect.top=(LONG)lroundf(fy); rect.bottom=rect.top+h;
    }
    bool CanAttack() override { return true; }
    void GetBullets(vector<EBULLET*>& ebs,IMAGE& bi,IMAGE& bm) override {
        if (++attackTimer>=155) {
            attackTimer=0; float sx=(rect.left+rect.right)/2.0f, sy=(float)rect.bottom;
            float dx=targetX-sx,dy=targetY-sy,len=sqrtf(dx*dx+dy*dy); if (len<1) len=1;
            EBULLET* eb=new EBULLET(); eb->Init(bi,bm,rect,dx/len,dy/len,3); ebs.push_back(eb);
        }
    }
    Kind GetKind() const override { return KIND_TYPE5; }
};

// ============ Formation member: fixed V-shaped slots moving as one group ============
class ENEMY_FORMATION : public ENEMY {
    IMAGE *pI,*pM,*pB,*pBM; int totalF,slot; float baseX,fy,phase;
public:
    ENEMY_FORMATION(IMAGE& i,IMAGE& im,IMAGE* b,IMAGE* bm,int level,int mode,int memberSlot,int groupX) {
        pI=&i;pM=&im;pB=b;pBM=bm;totalF=4;slot=memberSlot;
        hp=maxHp=2+(mode==2?1:0);scoreVal=25;phase=0;fy=(float)(-i.getheight()-abs(slot)*45);
        attackTimer=175;
        baseX=(float)groupX;
        int x=(int)(baseX+slot*(i.getwidth()+18));
        rect.left=x;rect.right=x+i.getwidth();rect.top=(LONG)fy;rect.bottom=rect.top+i.getheight();
    }
    bool Show() override {
        if(is_died){int frame=ExplosionFrame(totalF);if(frame>=totalF)return false;putimage_mask(rect.left,rect.top,&pB[frame],&pBM[frame]);return true;}
        if(rect.top>=HEIGHT)return false;Move();putimage_tinted(rect.left,rect.top,pI,pM, IsHitFlashing()?1:-1);return true;
    }
    void Move() override {
        phase+=0.018f*gEnemyMoveScale;fy+=1.15f*gEnemyMoveScale;
        int w=rect.right-rect.left,h=rect.bottom-rect.top;
        int x=(int)lroundf(baseX+slot*(w+18)+sin(phase)*35.0f);
        rect.left=x;rect.right=x+w;rect.top=(LONG)lroundf(fy);rect.bottom=rect.top+h;
    }
    bool CanAttack() override { return slot==0; }
    void GetBullets(vector<EBULLET*>& ebs,IMAGE& bi,IMAGE& bm) override {
        if(++attackTimer>=175){attackTimer=0;EBULLET* eb=new EBULLET();eb->InitDown(bi,bm,rect);ebs.push_back(eb);}
    }
    Kind GetKind() const override { return KIND_FORMATION; }
};

// ============ Type3: three-phase Boss with a different role per stage ============
class ENEMY_TYPE3 : public ENEMY {
    IMAGE *pB,*pBM;int totalF,patT,level,vx,ringOffset,phase;
    ULONGLONG transitionUntil;
    float drawScale;        // 视觉放大倍数（不动 sprite 原图，rect 用 scale 后尺寸，Draw 时居中）
    void UpdatePhase(){
        if(is_died)return;
        int next=hp*100>maxHp*60?1:(hp*100>maxHp*30?2:3);
        if(next!=phase){phase=next;transitionUntil=GetTickCount()+1250;patT=0;PlayGameSound(SOUND_BOSS);}
    }
public:
    ENEMY_TYPE3(IMAGE& i,IMAGE& im,IMAGE* b,IMAGE* bm,int stage,int mode){
        (void)i;(void)im;  // boss 贴图改用全局 gBossSprite（带 4 帧动画），原参数忽略
        pB=b;pBM=bm;level=stage;
        hp=maxHp=(mode==2?45:35)+stage*(mode==2?30:20);
        scoreVal=stage*100;totalF=6;patT=0;vx=2+stage;ringOffset=0;phase=1;transitionUntil=0;
        drawScale=1.35f;  // boss 视觉放大 35%
        int bw=gBossSprite->GetWidth(), bh=gBossSprite->GetHeight();
        int rw=(int)lroundf(bw*drawScale), rh=(int)lroundf(bh*drawScale);
        rect.left=WIDTH/2-rw/2;rect.right=rect.left+rw;
        // 顶在屏幕上沿会被 Boss 血条 (y=100~120) 挡住 -> 整体下移，让 boss 在血条下方
        rect.top=130;
        rect.bottom=rect.top+rh;
    }
    bool Show()override{
        if(is_died){int frame=ExplosionFrame(totalF);if(frame>=totalF)return false;putimage_mask(rect.left,rect.top,&pB[frame],&pBM[frame]);return true;}
        UpdatePhase();
        gBossSprite->Update();
        // sprite 原图按 drawScale 居中绘制
        int bw=gBossSprite->GetWidth(), bh=gBossSprite->GetHeight();
        int drawX=rect.left+(rect.right-rect.left-bw)/2;
        int drawY=rect.top +(rect.bottom-rect.top-bh)/2;
        gBossSprite->Draw(drawX, drawY, IsHitFlashing()?1:-1);
        if(phase==3){
            int cx=(rect.left+rect.right)/2,cy=(rect.top+rect.bottom)/2;
            setlinecolor((GetTickCount()/80)%2?RGB(255,30,30):RGB(255,150,20));setlinestyle(PS_SOLID,4);
            circle(cx,cy,(rect.right-rect.left)/2+12);setlinestyle(PS_SOLID,1);
        }
        return true;
    }
    void Move()override{
        UpdatePhase();int step=max(1,(int)lroundf((abs(vx)+(phase-1))*gEnemyMoveScale));
        int dir=vx<0?-1:1;rect.left+=dir*step;rect.right+=dir*step;
        if(rect.left<=0||rect.right>=WIDTH){vx=-vx;if(rect.left<0){rect.right-=rect.left;rect.left=0;}if(rect.right>WIDTH){int d=rect.right-WIDTH;rect.left-=d;rect.right=WIDTH;}}
    }
    bool IsBoss()override{return true;}
    Kind GetKind() const override { return KIND_BOSS; }
    int GetPhase()const override{return phase;}
    bool IsTransitioning()const override{return GetTickCount()<transitionUntil;}
    void RestoreHP(int savedHp){hp=max(1,min(maxHp,savedHp));phase=hp*100>maxHp*60?1:(hp*100>maxHp*30?2:3);}
    bool CanAttack()override{return true;}
    void GetBullets(vector<EBULLET*>& ebs,IMAGE& bi,IMAGE& bm)override{
        UpdatePhase();Move();if(IsTransitioning())return;patT++;
        int fanInterval=max(80,265-level*25-phase*28);
        if(patT%fanInterval==0){
            int spread=20+level*15+phase*10,step=phase==3?15:20;
            for(int a=-spread;a<=spread;a+=step){float rad=a*PI/180.0f;EBULLET* eb=new EBULLET();eb->Init(bi,bm,rect,sinf(rad),cosf(rad),phase==3?4:3);ebs.push_back(eb);}
        }
        // Stage 3 introduces its ring in phase 2, then combines it with lasers in phase 3.
        if(level>=3&&phase>=2&&patT%(phase==3?300:430)==0){
            ringOffset=(ringOffset+11)%45;int step=phase==3?24:36;
            for(int a=ringOffset;a<360+ringOffset;a+=step){float rad=a*PI/180.0f;EBULLET* eb=new EBULLET();eb->Init(bi,bm,rect,sinf(rad),cosf(rad),phase==3?3:2);ebs.push_back(eb);}
        }
        if(level>=3&&phase==3&&patT%390==0){
            int laserX=max(15,min(WIDTH-15,(int)lroundf(targetX)));EBULLET* laser=new EBULLET();laser->InitLaser(laserX,rect.bottom,90,70);ebs.push_back(laser);
        }
    }
};

// =====================================================================
// 文本工具：白字 + 黑色阴影 / 描边，在浅蓝/深蓝背景上都清晰
// =====================================================================
static inline void OutTextShadow(int x, int y, LPCTSTR s) {
    COLORREF cur = gettextcolor();
    settextcolor(RGB(15, 15, 30));
    outtextxy(x + 1, y + 1, s);
    outtextxy(x,     y + 1, s);
    outtextxy(x + 1, y,     s);
    settextcolor(cur);
    outtextxy(x, y, s);
}

static inline void OutTextShadowCentered(int y, LPCTSTR s) {
    OutTextShadow(WIDTH/2 - textwidth(s)/2, y, s);
}

void GameStart(int mode) {
    cleardevice(); settextcolor(WHITE); settextstyle(40,0,_T("\u9ed1\u4f53"));
    LPCTSTR modeText=runMode==1?_T("\u5173\u5361\u6a21\u5f0f"):_T("\u65e0\u5c3d\u6a21\u5f0f");
    TCHAR title[64];_stprintf_s(title,64,_T("%s - %s"),modeText,mode==1?_T("\u5355\u4eba"):_T("\u53cc\u4eba"));
    OutTextShadow(WIDTH/2-textwidth(title)/2,HEIGHT/2,title);
    settextstyle(22,0,_T("\u9ed1\u4f53"));
    OutTextShadow(70,HEIGHT/2+60,_T("P1: WASD / \u9f20\u6807\u62d6\u52a8    \u7a7a\u683c\u5c04\u51fb"));
    if (mode==2) OutTextShadow(70,HEIGHT/2+94,_T("P2: \u65b9\u5411\u952e\u79fb\u52a8    J / 1 / \u5c0f\u952e\u76d81\u5c04\u51fb"));
    OutTextShadow(70,HEIGHT/2+128,_T("\u62fe\u53d6\u65e0\u4eba\u673a\u9053\u5177\u53ef\u53ec\u5524\u8ddf\u968f\u50da\u673a"));
    OutTextShadow(70,HEIGHT/2+176,_T("\u6309\u4efb\u610f\u952e\u5f00\u59cb"));
    ExMessage m; while (true) { if (peekmessage(&m,EX_KEY|EX_MOUSE)) return; }
}

int SelectRunMode(){
    IMAGE bk;loadimage(&bk,_T("./images/bk3.png"),WIDTH,HEIGHT);
    LPCTSTR stage=_T("\u5173\u5361\u6a21\u5f0f\uff083\u5173Boss\uff09"),endless=_T("\u65e0\u5c3d\u6a21\u5f0f\uff08\u6301\u7eed\u52a0\u96be\uff09"),back=_T("\u8fd4\u56de");RECT r1{},r2{},rb{};
    while(true){
        BeginBatchDraw();putimage(0,0,&bk);setbkmode(TRANSPARENT);settextcolor(WHITE);settextstyle(48,0,_T("\u9ed1\u4f53"));
        LPCTSTR title=_T("\u9009\u62e9\u6e38\u620f\u7c7b\u578b");OutTextShadow(WIDTH/2-textwidth(title)/2,120,title);settextstyle(32,0,_T("\u9ed1\u4f53"));
        r1={WIDTH/2-textwidth(stage)/2-15,330,WIDTH/2+textwidth(stage)/2+15,380};OutTextShadow(WIDTH/2-textwidth(stage)/2,338,stage);
        r2={WIDTH/2-textwidth(endless)/2-15,455,WIDTH/2+textwidth(endless)/2+15,505};OutTextShadow(WIDTH/2-textwidth(endless)/2,463,endless);
        settextstyle(25,0,_T("\u9ed1\u4f53"));rb={WIDTH/2-textwidth(back)/2-15,600,WIDTH/2+textwidth(back)/2+15,640};OutTextShadow(WIDTH/2-textwidth(back)/2,605,back);EndBatchDraw();
        ExMessage m;getmessage(&m,EX_MOUSE|EX_KEY);
        if(m.message==WM_KEYDOWN&&m.vkcode==VK_ESCAPE)return 0;
        if(m.lbutton){if(MessInPoint(m.x,m.y,r1))return 1;if(MessInPoint(m.x,m.y,r2))return 2;if(MessInPoint(m.x,m.y,rb))return 0;}
    }
}

int screen2() {
    IMAGE bk; loadimage(&bk,_T("./images/bk3.png"),WIDTH,HEIGHT);
    LPCTSTR s1=_T("\u5355\u4eba\u6e38\u620f"),s2=_T("\u53cc\u4eba\u6e38\u620f"),sb=_T("\u8fd4\u56de");
    RECT r1,r2,rb;
    while (true) {
        BeginBatchDraw(); putimage(0,0,&bk); setbkmode(TRANSPARENT); settextcolor(WHITE);
        settextstyle(50,0,_T("\u9ed1\u4f53")); OutTextShadow(WIDTH/2-textwidth(_T("\u9009\u62e9\u6a21\u5f0f"))/2,HEIGHT/10,_T("\u9009\u62e9\u6a21\u5f0f"));
        settextstyle(36,0,_T("\u9ed1\u4f53"));
        r1.left=WIDTH/2-textwidth(s1)/2; r1.right=r1.left+textwidth(s1);
        r1.top=HEIGHT/3; r1.bottom=r1.top+textheight(s1);
        OutTextShadow(r1.left,r1.top,s1);
        r2.left=WIDTH/2-textwidth(s2)/2; r2.right=r2.left+textwidth(s2);
        r2.top=HEIGHT/3+100; r2.bottom=r2.top+textheight(s2);
        OutTextShadow(r2.left,r2.top,s2);
        settextstyle(28,0,_T("\u9ed1\u4f53"));
        rb.left=WIDTH/2-textwidth(sb)/2; rb.right=rb.left+textwidth(sb);
        rb.top=HEIGHT/3+220; rb.bottom=rb.top+textheight(sb);
        OutTextShadow(rb.left,rb.top,sb);
        EndBatchDraw();
        ExMessage m; getmessage(&m,EX_MOUSE);
        if (m.lbutton) {
            if (MessInPoint(m.x,m.y,r1)) { GameStart(1); return 1; }
            if (MessInPoint(m.x,m.y,r2)) { GameStart(2); return 2; }
            if (MessInPoint(m.x,m.y,rb)) return 0;
        }
    }
}

bool EnterNickname() {
    IMAGE bk; loadimage(&bk,_T("./images/bk2.png"),WIDTH,HEIGHT);
    string input;
    // W/A/S/D are registered as game-wide hotkeys. Release them while typing
    // so they arrive as ordinary character messages, then restore the controls.
    for(int id=1001;id<=1004;id++)UnregisterHotKey(NULL,id);
    UnregisterHotKey(NULL,1010); // B is the bomb hotkey during gameplay.
    auto restoreGameHotkeys=[](){
        RegisterHotKey(NULL,1001,0,'W');RegisterHotKey(NULL,1002,0,'A');
        RegisterHotKey(NULL,1003,0,'S');RegisterHotKey(NULL,1004,0,'D');
        RegisterHotKey(NULL,1010,0,'B');
    };
    while (true) {
        BeginBatchDraw(); putimage(0,0,&bk); setbkmode(TRANSPARENT);
        settextcolor(WHITE);settextstyle(42,0,_T("\u9ed1\u4f53"));
        LPCTSTR title=_T("\u8f93\u5165\u73a9\u5bb6\u6635\u79f0");OutTextShadow(WIDTH/2-textwidth(title)/2,220,title);
        wstring shown(input.begin(),input.end()); if(shown.empty())shown=_T("_");
        setfillcolor(RGB(245,245,255));solidrectangle(120,330,480,390);rectangle(120,330,480,390);
        settextcolor(RGB(30,80,180));settextstyle(32,0,_T("Consolas"));outtextxy(WIDTH/2-textwidth(shown.c_str())/2,344,shown.c_str());
        settextcolor(RGB(70,70,70));settextstyle(20,0,_T("\u9ed1\u4f53"));
        LPCTSTR tip=_T("1-10\u4f4d\u82f1\u6587/\u6570\u5b57  Enter\u786e\u8ba4  Esc\u8fd4\u56de");OutTextShadow(WIDTH/2-textwidth(tip)/2,420,tip);
        EndBatchDraw();
        ExMessage m;getmessage(&m,EX_KEY|EX_CHAR);
        if(m.message==WM_KEYDOWN&&m.vkcode==VK_ESCAPE){restoreGameHotkeys();return false;}
        if(m.message==WM_KEYDOWN&&m.vkcode==VK_RETURN){playerName=input.empty()?"PLAYER":input;restoreGameHotkeys();return true;}
        if(m.message==WM_KEYDOWN&&m.vkcode==VK_BACK){if(!input.empty())input.pop_back();continue;}
        if(m.message==WM_CHAR&&input.size()<10){char c=(char)m.ch;if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_')input.push_back(c);}
    }
}

void ShowRanking() {
    IMAGE bk;loadimage(&bk,_T("./images/bk2.png"),WIDTH,HEIGHT);
    vector<SCORE_ENTRY> stageScores=LoadScores(1),endlessScores=LoadScores(2);
    BeginBatchDraw();putimage(0,0,&bk);setbkmode(TRANSPARENT);settextcolor(WHITE);
    settextstyle(45,0,_T("\u9ed1\u4f53"));LPCTSTR title=_T("\u5206\u7c7b\u6392\u884c\u699c");OutTextShadow(WIDTH/2-textwidth(title)/2,55,title);
    settextstyle(27,0,_T("\u9ed1\u4f53"));settextcolor(RGB(30,90,190));OutTextShadow(65,130,_T("\u5173\u5361\u6a21\u5f0f TOP 10"));
    settextcolor(RGB(180,65,30));OutTextShadow(335,130,_T("\u65e0\u5c3d\u6a21\u5f0f TOP 10"));settextstyle(18,0,_T("Consolas"));
    for(int column=0;column<2;column++){
        const vector<SCORE_ENTRY>& scores=column==0?stageScores:endlessScores;int x=column==0?30:315;
        if(scores.empty()){settextcolor(RGB(220,220,220));OutTextShadow(x+70,190,_T("\u6682\u65e0\u8bb0\u5f55"));}
        for(size_t i=0;i<scores.size();i++){wstring name(scores[i].name.begin(),scores[i].name.end());TCHAR row[96];
            _stprintf_s(row,96,_T("%2d %-9s %8llu"),(int)i+1,name.c_str(),scores[i].score);
            settextcolor(i==0?RGB(255,200,80):WHITE);OutTextShadow(x,185+(int)i*48,row);}
    }
    settextcolor(RGB(230,230,230));settextstyle(21,0,_T("\u9ed1\u4f53"));LPCTSTR tip=_T("\u6309 Enter \u6216 Esc \u8fd4\u56de");OutTextShadow(WIDTH/2-textwidth(tip)/2,780,tip);EndBatchDraw();
    while(true){ExMessage m;getmessage(&m,EX_KEY|EX_MOUSE);if((m.message==WM_KEYDOWN&&(m.vkcode==VK_RETURN||m.vkcode==VK_ESCAPE))||m.lbutton)return;}
}

// ============================================================
// Intro 开场动画：4 个场景依次播放
//  - intro1: 黑底，静止 1 秒
//  - intro2: 黑底叠加显示，短停留后原图所有非透明像素过渡为纯白（透明处仍是黑底）
//  - intro3: 静止 1 秒后向下飞出屏幕（直线匀速）
//  - intro4: 从屏幕底部上升到指定位置，停一帧后向右飞出屏幕
// ============================================================

// 等待指定毫秒数。期间按 Enter 或 Esc 立即返回 true（中断）。
// ms == 0 表示不等待，只检查一次按键。
static bool WaitOrAbort(int ms) {
    ULONGLONG endAt = GetTickCount() + (ULONGLONG)ms;
    while (GetTickCount() < endAt) {
        Sleep(10);
        ExMessage m;
        while (peekmessage(&m, EX_KEY)) {
            if (m.message == WM_KEYDOWN && (m.vkcode == VK_RETURN || m.vkcode == VK_ESCAPE)) {
                while (peekmessage(nullptr, EX_KEY | EX_MOUSE)) {}
                return true;
            }
        }
    }
    return false;
}

// 把单帧 RGBA 图片拷成黑白掩码：alpha>=128 涂黑（机身），alpha<128 涂白（透明）
// 与 GameOver 用的 BuildMaskFromAlpha 约定一致。
static inline void IntroBuildAlphaMask(const IMAGE& src, IMAGE& mask, int w, int h) {
    mask.Resize(w, h);
    DWORD* sBuf = const_cast<DWORD*>(GetImageBuffer(const_cast<IMAGE*>(&src)));
    DWORD* mBuf = GetImageBuffer(&mask);
    for (int i = 0; i < w * h; i++) {
        BYTE a = (BYTE)((sBuf[i] >> 24) & 0xFF);
        mBuf[i] = (a >= 128) ? BLACK : WHITE;
    }
}

// 进入 Intro 时自动调用 onEnter；RAII guard，确保 onExit 在任何 return 前调用
typedef void (*IntroGuardFn)(void);
struct IntroScopeGuard {
    IntroGuardFn onExit;
    bool armed;
    IntroScopeGuard(IntroGuardFn onEnter, IntroGuardFn exitFn) : onExit(exitFn), armed(true) {
        if (onEnter) onEnter();
    }
    ~IntroScopeGuard() { if (armed && onExit) onExit(); }
};

void Intro() {
    // 启动 intro 音乐；所有 return 都会自动 Stop
    IntroScopeGuard guard(PlayIntroMusic, StopIntroMusic);

    // ----- 加载 4 张 intro 素材 -----
    IMAGE intro1_src, intro1_mask;
    IMAGE intro2_src, intro2_mask;        // 会原地修改（白化）
    IMAGE intro3_src, intro3_mask;
    IMAGE intro4_src, intro4_mask;

    // 用 "加载到临时图" + 再创建一份可写的副本方式（避免某些 PNG 加载为只读）
    auto LoadIntro = [](LPCTSTR path, IMAGE& src, IMAGE& mask) {
        IMAGE raw; loadimage(&raw, path);
        int w = raw.getwidth(), h = raw.getheight();
        src.Resize(w, h);
        DWORD* dst = GetImageBuffer(&src);
        DWORD* s = GetImageBuffer(&raw);
        for (int i = 0; i < w * h; i++) dst[i] = s[i];
        IntroBuildAlphaMask(src, mask, w, h);
    };

    LoadIntro(_T("./images/intro1.png"), intro1_src, intro1_mask);
    LoadIntro(_T("./images/intro2.png"), intro2_src, intro2_mask);
    LoadIntro(_T("./images/intro3.png"), intro3_src, intro3_mask);
    LoadIntro(_T("./images/intro4.png"), intro4_src, intro4_mask);

    const int iw = intro1_src.getwidth();
    const int ih = intro1_src.getheight();
    const int ix = (WIDTH  - iw) / 2;   // 居中 X
    const int iy = (HEIGHT - ih) / 2;   // 居中 Y

    // 把任意输入消息清掉，避免 Intro 期间堆积按键
    while (peekmessage(nullptr, EX_KEY | EX_MOUSE)) {}

    // ============= 场景 1: intro1 静止 1 秒 =============
    {
        setbkcolor(BLACK); cleardevice();
        putimage_mask(ix, iy, &intro1_src, &intro1_mask);
        FlushBatchDraw();
    }
    if (WaitOrAbort(1000)) return;

    // ============= 场景 2: intro2 显示 → 白化过渡 =============
    //   a) 先短暂停留（约 200ms）让玩家看清
    //   b) 然后做 ~700ms 的白化过渡（非透明像素 → 白色）
    ULONGLONG s2Start = GetTickCount();
    const ULONGLONG s2StaticMs = 200;   // 短停留
    const ULONGLONG s2FadeMs   = 700;   // 过渡时长
    bool s2BackedUp = false;
    IMAGE s2Origin;
    while (true) {
        ULONGLONG now = GetTickCount();
        ULONGLONG el = now - s2Start;

        setbkcolor(BLACK); cleardevice();
        if (el > s2StaticMs) {
            double t = (double)(el - s2StaticMs) / (double)s2FadeMs;
            if (t > 1.0) t = 1.0;
            // 首次进入过渡时，备份一次原始像素
            if (!s2BackedUp) {
                s2BackedUp = true;
                s2Origin.Resize(iw, ih);
                DWORD* dstO = GetImageBuffer(&s2Origin);
                DWORD* s = GetImageBuffer(&intro2_src);
                for (int i = 0; i < iw * ih; i++) dstO[i] = s[i];
            }
            // 原地 lerp（用备份算出当前帧的新像素）
            DWORD* srcOrig = GetImageBuffer(&s2Origin);
            DWORD* dstNow  = GetImageBuffer(&intro2_src);
            DWORD* maskNow = GetImageBuffer(&intro2_mask);
            for (int i = 0; i < iw * ih; i++) {
                DWORD p = srcOrig[i];
                BYTE a = (BYTE)((p >> 24) & 0xFF);
                if (a < 128) {
                    // 透明像素：清零 + mask=WHITE（保持透明）
                    dstNow[i] = 0;
                    maskNow[i] = WHITE;
                } else {
                    // 不透明像素：白化 RGB + mask=BLACK（保持显示）
                    DWORD r = p & 0xFF;
                    DWORD g = (p >> 8) & 0xFF;
                    DWORD b = (p >> 16) & 0xFF;
                    DWORD nr = (DWORD)(r + (255 - r) * t);
                    DWORD ng = (DWORD)(g + (255 - g) * t);
                    DWORD nb = (DWORD)(b + (255 - b) * t);
                    dstNow[i] = (a << 24) | (nb << 16) | (ng << 8) | nr;
                    maskNow[i] = BLACK;
                }
            }
        }
        putimage_mask(ix, iy, &intro2_src, &intro2_mask);
        FlushBatchDraw();
        if (WaitOrAbort(0)) return;        // 每帧检查 ESC/Enter
        if (el >= s2StaticMs + s2FadeMs) break;
    }

    // ============= 场景 3: intro3 静止 1 秒 → 向下掉出屏幕 =============
    {
        setbkcolor(BLACK); cleardevice();
        putimage_mask(ix, iy, &intro3_src, &intro3_mask);
        FlushBatchDraw();
    }
    if (WaitOrAbort(1000)) return;

    const int dropSpeed = 8;  // 像素/帧
    int i3y = iy;
    while (true) {
        setbkcolor(BLACK); cleardevice();
        if (i3y > HEIGHT) break;
        putimage_mask(ix, i3y, &intro3_src, &intro3_mask);
        FlushBatchDraw();
        if (WaitOrAbort(0)) return;        // 每帧检查
        i3y += dropSpeed;
    }

    // ============= 场景 4: intro4 从下方上升 → 停留 → 向右飞出 =============
    //   a) 从 y = HEIGHT 升到 iy，用时 ~600ms
    //   b) 停顿 ~500ms
    //   c) 向右飞出（x 递增），直到 x > WIDTH
    const int    i4flySpeed = 12;
    const ULONGLONG s4RiseMs = 600;
    const ULONGLONG s4HoldMs = 500;
    ULONGLONG s4RiseAt = GetTickCount();
    int i4x = ix, i4y = HEIGHT;
    bool rising = true, holding = false;
    while (true) {
        ULONGLONG now = GetTickCount();
        ULONGLONG elR = now - s4RiseAt;
        if (rising) {
            double t = (double)elR / s4RiseMs;
            if (t >= 1.0) { t = 1.0; rising = false; holding = true; s4RiseAt = now; }
            i4y = (int)(HEIGHT + (iy - HEIGHT) * t);
        } else if (holding) {
            i4y = iy;
            if (elR >= s4HoldMs) { holding = false; s4RiseAt = now; }
        } else {
            i4y = iy;
            i4x += i4flySpeed;
            if (i4x > WIDTH) break;
        }
        setbkcolor(BLACK); cleardevice();
        putimage_mask(i4x, i4y, &intro4_src, &intro4_mask);
        FlushBatchDraw();
        if (WaitOrAbort(0)) return;
    }
}


// 0=start with current player, 1=continue save, 2=switch player, 3=ranking, 4=exit
int Welcome() {
    PlayMusic(MUSIC_MENU);
    // title_bk-Sheet.png: 4帧动画，每帧600x1000 (游戏窗口尺寸)，水平排列
    AnimatedBG menuBg(_T("./images/title_bk-Sheet.png"), WIDTH, HEIGHT, 4, 6);

    // ----- 新标题: title.png (517×185 RGBA) -----
    // mask 约定：BLACK=机身（不透明），WHITE=透明（与 BuildMaskFromAlpha 一致）
    IMAGE title_src, title_mask;
    {
        IMAGE raw; loadimage(&raw, _T("./images/title.png"));
        int tw = raw.getwidth(), th = raw.getheight();
        title_src.Resize(tw, th);
        DWORD* dst = GetImageBuffer(&title_src);
        DWORD* s = GetImageBuffer(&raw);
        for (int i = 0; i < tw * th; i++) dst[i] = s[i];
        title_mask.Resize(tw, th);
        DWORD* m = GetImageBuffer(&title_mask);
        for (int i = 0; i < tw * th; i++) {
            BYTE a = (BYTE)((dst[i] >> 24) & 0xFF);
            m[i] = (a >= 128) ? BLACK : WHITE;
        }
    }

    // ----- UFO 飞入动画: UFO1-Sheet-Sheet.png 504×85, 3 帧 (每帧 168×85) -----
    const int UFO_FW = 168, UFO_FH = 85, UFO_FRAMES = 3, UFO_FPS = 8;
    IMAGE ufo_src[UFO_FRAMES], ufo_mask[UFO_FRAMES];
    LoadSpriteSheetFrames(ufo_src, UFO_FRAMES, _T("./images/UFO1-Sheet-Sheet.png"), UFO_FW, UFO_FH);
    BuildMaskArrayFromAlpha(ufo_src, ufo_mask, UFO_FRAMES, UFO_FW, UFO_FH);
    static int ufoCurFrame = 0;
    static ULONGLONG ufoLastUpdate = 0;
    auto ufoFrameNow = [&]() {
        ULONGLONG now = GetTickCount();
        if (now - ufoLastUpdate >= 1000 / UFO_FPS) {
            ufoCurFrame = (ufoCurFrame + 1) % UFO_FRAMES;
            ufoLastUpdate = now;
        }
        return ufoCurFrame;
    };

    // UFO 入场起点/终点
    const int   ufoStartX   = -UFO_FW;   // 屏幕外左侧
    const int   ufoEndX     = 54;        // 中间偏左
    const int   ufoY        = 50;
    const ULONGLONG ufoSlideMs = 500;    // 半秒

    // 排版常量（避开 UFO + 新标题）
    const int TITLE_X      = (WIDTH - 517) / 2;   // 42
    const int TITLE_Y      = 120;                  // 标题占 y=120..305
    const int PLAYER_Y     = 325;
    const int MENU_START_Y = 385;
    const int MENU_GAP     = 88;
    const int MENU_H       = 48;
    LPCTSTR labels[5]={_T("s0"),_T("s1"),_T("s2"),_T("s3"),_T("s4")};RECT rs[5];
    // 给 labels 填上真正的中文
    labels[0]=_T("\u5f00\u59cb\u6e38\u620f");
    labels[1]=_T("\u7ee7\u7eed\u5b58\u6863");
    labels[2]=_T("\u5207\u6362\u73a9\u5bb6");
    labels[3]=_T("\u5386\u53f2\u6392\u884c\u699c");
    labels[4]=_T("\u9000\u51fa\u6e38\u620f");
    // ----- UFO 入场动画（约 500ms）-----
    {
        ULONGLONG ufoStartAt = GetTickCount();
        ufoLastUpdate = ufoStartAt;
        while (GetTickCount() - ufoStartAt < ufoSlideMs) {
            ULONGLONG el = GetTickCount() - ufoStartAt;
            double t = (double)el / ufoSlideMs;
            if (t > 1.0) t = 1.0;
            int ufoX = (int)(ufoStartX + (ufoEndX - ufoStartX) * t);
            int fr = ufoFrameNow();
            BeginBatchDraw();
            menuBg.Show();
            putimage_mask(ufoX, ufoY, &ufo_src[fr], &ufo_mask[fr]);
            EndBatchDraw();
            Sleep(16);
        }
    }
    // 最后一帧停留一小会儿
    Sleep(120);

    while(true){
        SAVE_DATA coverSave;bool saveAvailable=LoadSave(coverSave);
        BeginBatchDraw();
        menuBg.Show();

        // UFO 最终位置
        putimage_mask(ufoEndX, ufoY, &ufo_src[ufoFrameNow()], &ufo_mask[ufoFrameNow()]);

        // title.png 自绘标题
        putimage_mask(TITLE_X, TITLE_Y, &title_src, &title_mask);

        setbkmode(TRANSPARENT);settextcolor(WHITE);
        wstring currentName(playerName.begin(),playerName.end());if(currentName.empty())currentName=_T("\u672a\u9009\u62e9");
        TCHAR current[64];_stprintf_s(current,64,_T("\u5f53\u524d\u73a0\u5bb6: %s"),currentName.c_str());
        settextstyle(22,0,_T("\u9ed1\u4f53"));settextcolor(RGB(135,205,255));OutTextShadow(WIDTH/2-textwidth(current)/2,PLAYER_Y,current);
        settextstyle(34,0,_T("\u9ed1\u4f53"));
        for(int i=0;i<5;i++){
            rs[i].left=WIDTH/2-textwidth(labels[i])/2-20;
            rs[i].right=WIDTH/2+textwidth(labels[i])/2+20;
            rs[i].top=MENU_START_Y+i*MENU_GAP;
            rs[i].bottom=rs[i].top+MENU_H;
            settextcolor(i==1&&!saveAvailable?RGB(150,150,150):WHITE);
            OutTextShadow(WIDTH/2-textwidth(labels[i])/2,rs[i].top,labels[i]);
        }
        settextstyle(18,0,_T("\u9ed1\u4f53"));
        if(saveAvailable){
            wstring savedName(coverSave.name.begin(),coverSave.name.end());TCHAR detail[128];
            _stprintf_s(detail,128,_T("\u5b58\u6863: %s   LEVEL %d   Score %llu"),savedName.c_str(),coverSave.level,coverSave.score);
            settextcolor(RGB(135,205,255));OutTextShadow(WIDTH/2-textwidth(detail)/2,rs[1].bottom+2,detail);
        }else{
            LPCTSTR none=_T("\u6682\u65e0\u5b58\u6863\uff08\u6e38\u620f\u4e2d\u6309 Esc \u4fdd\u5b58\uff09");settextcolor(RGB(200,200,200));OutTextShadow(WIDTH/2-textwidth(none)/2,rs[1].bottom+2,none);
        }
        EndBatchDraw();
        Sleep(20);
        ExMessage m;
        // 非阻塞：先消化队列里的所有鼠标消息，再处理最后一帧点击
        while(peekmessage(&m, EX_MOUSE)) {
            if(m.lbutton){
                for(int i=0;i<5;i++)if(MessInPoint(m.x,m.y,rs[i])){if(i==1&&!saveAvailable)break;return i;}
            }
        }
    }
}

void Over() {
    PlayMusic(MUSIC_OVER);

    // --- 加载 GameOver 资源 ---
    constexpr int TEXTS_FRAMES = 2;
    constexpr int TEXTS_FW = 1006 / TEXTS_FRAMES;  // 503
    constexpr int TEXTS_FH = 127;
    IMAGE goTexts_src[TEXTS_FRAMES];
    IMAGE goTexts_mask[TEXTS_FRAMES];
    IMAGE goAnim1_src, goAnim1_mask;
    IMAGE goAnim2_src, goAnim2_mask;
    LoadSpriteSheetFrames(goTexts_src, TEXTS_FRAMES, _T("./images/gameover_texts-Sheet.png"), TEXTS_FW, TEXTS_FH);
    BuildMaskArrayFromAlpha(goTexts_src, goTexts_mask, TEXTS_FRAMES, TEXTS_FW, TEXTS_FH);
    loadimage(&goAnim1_src, _T("./images/gameover_anim1.png"));
    BuildMaskArrayFromAlpha(&goAnim1_src, &goAnim1_mask, 1, goAnim1_src.getwidth(), goAnim1_src.getheight());
    loadimage(&goAnim2_src, _T("./images/gameover_anim2.png"));
    BuildMaskArrayFromAlpha(&goAnim2_src, &goAnim2_mask, 1, goAnim2_src.getwidth(), goAnim2_src.getheight());

    // --- 布局 ---
    const int textsW = TEXTS_FW;       // 503
    const int textsH = TEXTS_FH;       // 127
    const int animW  = goAnim1_src.getwidth();   // 274
    const int animH  = goAnim1_src.getheight();  // 319
    const int textsX = (WIDTH - textsW) / 2;
    const int textsY = 90;                       // 靠上
    const int animX  = (WIDTH - animW) / 2;
    const int animY  = textsY + textsH + 80;     // 文字下方，居中靠中
    const int scoreY = animY + animH + 60;       // 动画下方
    const int tipY   = scoreY + 60;              // 提示再往下

    // --- 准备文案 ---
    TCHAR sScore[128];
    _stprintf_s(sScore, 128, _T("\u603b\u5f97\u5206\uff1a%llu"), totalScore);
    LPCTSTR sTip = _T("\u6309 Enter \u952e\u8fd4\u56de\u4e3b\u83dc\u5355");

    ULONGLONG t0 = GetTickCount();
    int  textFrame = 0;        // 0 or 1 - 抖动文字帧（图自带抖动效果）
    int  animState = 0;        // 0=显示1(1秒), 1=闪烁过渡, 2=显示2
    ULONGLONG animStateAt = t0;
    int  blinkCount = 0;       // 已完成闪烁次数

    // 闪烁参数：每次闪 1↔2 各 100ms，总共 2 次（1↔2↔1↔2），约 400ms
    const DWORD BLINK_HALF_MS = 100;
    const int   BLINK_TIMES   = 2;
    bool blinkShowTwo = false;

    while (true) {
        ULONGLONG now = GetTickCount();

        // ---- 处理 Enter 退出 ----
        ExMessage m;
        while (peekmessage(&m, EX_KEY)) {
            if (m.message == WM_KEYDOWN && m.vkcode == 0x0D) {
                FlushBatchDraw();
                return;
            }
        }

        // ---- 1) 黑底 ----
        setbkcolor(BLACK); cleardevice();

        // ---- 2) 文字（图自带抖动动画帧，每 80ms 切帧） ----
        ULONGLONG textsPeriod = 80;
        if (now - t0 >= textsPeriod) {
            int step = (int)((now - t0) / textsPeriod);
            textFrame = (textFrame + step) % TEXTS_FRAMES;
            t0 += step * textsPeriod;
        }
        putimage_mask(textsX, textsY,
            &goTexts_src[textFrame], &goTexts_mask[textFrame]);

        // ---- 3) 动画：先1，再闪烁2次到2 ----
        if (animState == 0) {
            // 显示 anim1，持续 1000ms
            putimage_mask(animX, animY, &goAnim1_src, &goAnim1_mask);
            if (now - animStateAt >= 1000) {
                animState = 1;
                animStateAt = now;
                blinkCount = 0;
                blinkShowTwo = false;
            }
        } else if (animState == 1) {
            // 在 1 和 2 之间闪烁
            if (now - animStateAt >= BLINK_HALF_MS) {
                blinkShowTwo = !blinkShowTwo;
                animStateAt += BLINK_HALF_MS;
                if (!blinkShowTwo) blinkCount++;
                if (blinkCount >= BLINK_TIMES && !blinkShowTwo) {
                    animState = 2;
                    animStateAt = now;
                }
            }
            if (blinkShowTwo) putimage_mask(animX, animY, &goAnim2_src, &goAnim2_mask);
            else              putimage_mask(animX, animY, &goAnim1_src, &goAnim1_mask);
        } else {
            // animState == 2: 稳定显示 anim2
            putimage_mask(animX, animY, &goAnim2_src, &goAnim2_mask);
        }

        // ---- 4) 总分 ----
        setbkmode(TRANSPARENT);
        settextcolor(WHITE); settextstyle(40, 0, _T("\u9ed1\u4f53"));
        OutTextShadowCentered(scoreY, sScore);

        // ---- 5) Enter 提示（轻微呼吸效果） ----
        // 半透明闪烁（每 500ms 一次）
        int alphaTick = (int)((now / 500) % 2);
        settextcolor(alphaTick ? RGB(255, 255, 255) : RGB(160, 160, 180));
        settextstyle(24, 0, _T("\u9ed1\u4f53"));
        OutTextShadowCentered(tipY, sTip);

        FlushBatchDraw();
        Sleep(10);
    }
}

int PauseMenu() {
    PlayMusic(MUSIC_PAUSE);
    LPCTSTR tc=_T("  \u7ee7\u7eed\u6e38\u620f  "),ts=_T(" \u4fdd\u5b58\u5e76\u8fd4\u56de\u83dc\u5355 "),tm=_T("  \u653e\u5f03\u8fd4\u56de\u83dc\u5355  "),te=_T("  \u9000\u51fa\u6e38\u620f  ");
    RECT cr,sr,mr,er;
    settextstyle(28,0,_T("\u9ed1\u4f53"));
    cr.left=WIDTH/2-textwidth(tc)/2-20; cr.right=cr.left+textwidth(tc)+40;
    cr.top=HEIGHT/2-105; cr.bottom=cr.top+45;
    sr.left=WIDTH/2-textwidth(ts)/2-20;sr.right=sr.left+textwidth(ts)+40;sr.top=cr.bottom+15;sr.bottom=sr.top+45;
    mr.left=WIDTH/2-textwidth(tm)/2-20; mr.right=mr.left+textwidth(tm)+40;mr.top=sr.bottom+15; mr.bottom=mr.top+45;
    er.left=WIDTH/2-textwidth(te)/2-20; er.right=er.left+textwidth(te)+40;
    er.top=mr.bottom+15; er.bottom=er.top+45;
    BeginBatchDraw();
    setfillcolor(RGB(35,35,55)); solidrectangle(0,0,WIDTH,48);
    settextcolor(WHITE); settextstyle(34,0,_T("\u9ed1\u4f53"));
    outtextxy(WIDTH/2-textwidth(_T("\u6e38\u620f\u6682\u505c"))/2,8,_T("\u6e38\u620f\u6682\u505c"));
    settextstyle(28,0,_T("\u9ed1\u4f53"));
    setfillcolor(RGB(50,50,70)); setlinecolor(RGB(90,90,120));
    solidrectangle(cr.left,cr.top,cr.right,cr.bottom); rectangle(cr.left,cr.top,cr.right,cr.bottom);
    settextcolor(RGB(200,220,255)); outtextxy(cr.left+20,cr.top+8,tc);
    setfillcolor(RGB(42,75,62));solidrectangle(sr.left,sr.top,sr.right,sr.bottom);rectangle(sr.left,sr.top,sr.right,sr.bottom);
    settextcolor(RGB(180,255,210));outtextxy(sr.left+20,sr.top+8,ts);
    setfillcolor(RGB(60,50,50)); solidrectangle(mr.left,mr.top,mr.right,mr.bottom);
    rectangle(mr.left,mr.top,mr.right,mr.bottom);
    settextcolor(RGB(255,200,200)); outtextxy(mr.left+20,mr.top+8,tm);
    setfillcolor(RGB(80,40,40)); solidrectangle(er.left,er.top,er.right,er.bottom);
    rectangle(er.left,er.top,er.right,er.bottom);
    settextcolor(RGB(255,150,150)); outtextxy(er.left+20,er.top+8,te);
    EndBatchDraw();
    while (true) {
        ExMessage m; getmessage(&m,EX_KEY|EX_MOUSE);
        if ((m.message==WM_KEYDOWN&&m.vkcode==VK_ESCAPE)||(m.lbutton&&MessInPoint(m.x,m.y,cr))) {
            while(peekmessage(&m,EX_KEY)); return 0;
        }
        if ((m.message==WM_KEYDOWN&&m.vkcode==49)||(m.lbutton&&MessInPoint(m.x,m.y,sr))) return 3;
        if ((m.message==WM_KEYDOWN&&m.vkcode==50)||(m.lbutton&&MessInPoint(m.x,m.y,mr))) return 1;
        if ((m.message==WM_KEYDOWN&&m.vkcode==51)||(m.lbutton&&MessInPoint(m.x,m.y,er))) { CloseGameAudio();closegraph();timeEndPeriod(1);exit(0);return 2; }
        Sleep(30);
    }
}
void SpawnEnemy(vector<ENEMY*>& es, IMAGE& e1i, IMAGE& e1m, IMAGE* e1b, IMAGE* e1bm,
    IMAGE& e2i, IMAGE& e2m, IMAGE* e2b, IMAGE* e2bm, int level, int mode) {
    int r = rand() % 100;
    if (level==1) {
        if (r<70) es.push_back(new ENEMY_TYPE1(level,mode));
        else es.push_back(new ENEMY_TYPE2(level,mode));
    } else if (level==2) {
        if (r<30) es.push_back(new ENEMY_TYPE1(level,mode));
        else if (r<55) es.push_back(new ENEMY_TYPE2(level,mode));
        else if (r<78) es.push_back(new ENEMY_TYPE4(e2i,e2m,e2b,e2bm,level,mode));
        else {
            int center=WIDTH/2-e2i.getwidth()/2;
            for(int slot=-1;slot<=1;slot++) es.push_back(new ENEMY_FORMATION(e2i,e2m,e2b,e2bm,level,mode,slot,center));
        }
    } else {
        if (r<18) es.push_back(new ENEMY_TYPE1(level,mode));
        else if (r<38) es.push_back(new ENEMY_TYPE2(level,mode));
        else if (r<60) es.push_back(new ENEMY_TYPE4(e2i,e2m,e2b,e2bm,level,mode));
        else if (r<82) es.push_back(new ENEMY_TYPE5(e2i,e2m,e2b,e2bm,level,mode));
        else {
            int center=WIDTH/2-e2i.getwidth()/2;
            for(int slot=-1;slot<=1;slot++) es.push_back(new ENEMY_FORMATION(e2i,e2m,e2b,e2bm,level,mode,slot,center));
        }
    }
}

void TryDropPowerup(vector<POWERUP*>& items,RECT where,POWER_ART art[],int level,int mode){
    int dropChance=max(7,(mode==2?19:15)-(min(level,3)-1)*2);
    if(rand()%100>=dropChance)return;
    int weighted[]={POWER_FIRE,POWER_FIRE,POWER_DRONE,POWER_LIFE,POWER_BOMB,POWER_SHIELD,
        POWER_MAGNET,POWER_FREEZE,POWER_PIERCE,POWER_OVERLOAD,POWER_REVIVE};
    POWER_TYPE type=(POWER_TYPE)weighted[rand()%(sizeof(weighted)/sizeof(weighted[0]))];
    if(type==POWER_REVIVE&&mode!=2)type=POWER_LIFE;
    items.push_back(new POWERUP(type,*art[type].image,*art[type].mask,where));
}

void ApplyUpgrade(UPGRADE_STATE& u,int id){
    if(id==0)u.bulletDamage=min(5,u.bulletDamage+1);
    else if(id==1)u.fireRateLevel=min(5,u.fireRateLevel+1);
    else if(id==2)u.pierce=1;
    else if(id==3)u.sideBullet=min(2,u.sideBullet+1);
    else if(id==4)u.moveSpeed=min(3,u.moveSpeed+1);
    else if(id==5)u.shieldCapacity=min(3,u.shieldCapacity+1);
    else if(id==6)u.bombDamage=min(4,u.bombDamage+1);
    else if(id==7)u.lowHpDouble=1;
}

void UpgradeSelect(IMAGE& background,UPGRADE_STATE& u,int clearedLevel){
    static LPCTSTR names[8]={_T("\u5b50\u5f39\u4f24\u5bb3 +1"),_T("\u5c04\u901f +15%"),_T("\u5b50\u5f39\u7a7f\u900f 1 \u6b21"),_T("\u989d\u5916\u4fa7\u7ffc\u5b50\u5f39"),
        _T("\u79fb\u52a8\u901f\u5ea6\u63d0\u5347"),_T("\u62a4\u76fe\u5bb9\u91cf +1"),_T("\u70b8\u5f39\u4f24\u5bb3\u63d0\u5347"),_T("\u4f4e\u8840\u91cf\u53cc\u500d\u4f24\u5bb3")};
    static LPCTSTR desc[8]={_T("\u6240\u6709\u6211\u65b9\u5b50\u5f39\u9020\u6210\u66f4\u9ad8\u4f24\u5bb3"),_T("\u7f29\u77ed\u6bcf\u6b21\u5c04\u51fb\u95f4\u9694"),_T("\u547d\u4e2d\u540e\u53ef\u7ee7\u7eed\u98de\u884c\u4e00\u6b21"),_T("\u666e\u901a\u5c04\u51fb\u589e\u52a0\u4fa7\u5411\u5f39"),
        _T("\u4e24\u540d\u73a9\u5bb6\u5747\u751f\u6548"),_T("\u53ef\u540c\u65f6\u4fdd\u7559\u66f4\u591a\u62a4\u76fe"),_T("\u589e\u5f3a\u666e\u901a\u4e0e\u5408\u4f53\u70b8\u5f39"),_T("HP=1 \u65f6\u5b50\u5f39\u4f24\u5bb3\u7ffb\u500d")};
    int choices[3];for(int i=0;i<3;i++){bool duplicate;do{choices[i]=rand()%8;duplicate=false;for(int j=0;j<i;j++)if(choices[j]==choices[i])duplicate=true;}while(duplicate);}
    RECT cards[3];
    while(true){
        BeginBatchDraw();putimage(0,0,&background);setbkmode(TRANSPARENT);settextcolor(RGB(245,245,255));settextstyle(42,0,_T("\u9ed1\u4f53"));
        TCHAR title[64];_stprintf_s(title,64,_T("LEVEL %d \u5b8c\u6210 - \u4e09\u9009\u4e00\u5347\u7ea7"),clearedLevel);outtextxy(WIDTH/2-textwidth(title)/2,105,title);
        for(int i=0;i<3;i++){cards[i]={55,245+i*190,545,390+i*190};
            setfillcolor(i==0?RGB(35,75,130):RGB(50,55,85));setlinecolor(RGB(170,215,255));solidrectangle(cards[i].left,cards[i].top,cards[i].right,cards[i].bottom);rectangle(cards[i].left,cards[i].top,cards[i].right,cards[i].bottom);
            TCHAR number[8];_stprintf_s(number,8,_T("%d"),i+1);settextcolor(RGB(255,220,80));settextstyle(33,0,_T("Consolas"));outtextxy(78,cards[i].top+25,number);
            settextcolor(WHITE);settextstyle(29,0,_T("\u9ed1\u4f53"));outtextxy(130,cards[i].top+22,names[choices[i]]);
            settextcolor(RGB(205,220,245));settextstyle(20,0,_T("\u9ed1\u4f53"));outtextxy(130,cards[i].top+76,desc[choices[i]]);
        }
        settextcolor(RGB(230,230,230));settextstyle(19,0,_T("\u9ed1\u4f53"));outtextxy(105,850,_T("\u6309 1 / 2 / 3 \u6216\u70b9\u51fb\u9009\u62e9\uff0c\u5347\u7ea7\u4ec5\u5728\u672c\u6b21\u6311\u6218\u6709\u6548"));EndBatchDraw();
        ExMessage m;getmessage(&m,EX_KEY|EX_MOUSE);int chosen=-1;
        if(m.message==WM_KEYDOWN&&m.vkcode>='1'&&m.vkcode<='3')chosen=m.vkcode-'1';
        if(m.lbutton)for(int i=0;i<3;i++)if(MessInPoint(m.x,m.y,cards[i]))chosen=i;
        if(chosen>=0){ApplyUpgrade(u,choices[chosen]);PlayGameSound(SOUND_PICKUP);return;}
    }
}

bool Play(bool resumeGame=false) {
    SAVE_DATA saved; bool resuming=resumeGame&&LoadSave(saved);
    if(resuming){gameMode=saved.mode;runMode=saved.runMode;playerName=saved.name;}
    HWND gameWindow = GetHWnd();
    LONG_PTR exStyle = GetWindowLongPtr(gameWindow, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_NOACTIVATE | WS_EX_TRANSPARENT);
    SetWindowLongPtr(gameWindow, GWL_EXSTYLE, exStyle);
    EnableWindow(gameWindow, TRUE);
    ShowWindow(gameWindow, SW_RESTORE);
    SetForegroundWindow(gameWindow);
    SetActiveWindow(gameWindow);
    SetFocus(gameWindow);
    setbkcolor(WHITE); cleardevice();
    totalScore = resuming?saved.score:0;
    gameWon = false;
    bool is_play = true, p1Dead = false, p2Dead = false;
    bool recordResult=false;
    int bsing = 0;

    // ===== Load images =====
    IMAGE me_image, me_image_mask, p2_image, p2_image_mask;
    IMAGE wing1_image,wing1_mask,wing2_image,wing2_mask;
    IMAGE enemy_image, enemy_image_mask;
    IMAGE enemy2_image, enemy2_image_mask;
    IMAGE enemy3_image, enemy3_image_mask;
    IMAGE bullet_image, bullet_image_mask;
    IMAGE enemy_bullet_image, enemy_bullet_image_mask;
    IMAGE enemy1_death_src, enemy1_death_mask;
    IMAGE enemy2_death_src, enemy2_death_mask;
    IMAGE fire_icon,fire_mask,life_icon,life_mask,bomb_icon,bomb_mask,shield_icon,shield_mask,drone_icon,drone_mask;
    IMAGE magnet_icon,magnet_mask,freeze_icon,freeze_mask,pierce_icon,pierce_mask,overload_icon,overload_mask,revive_icon,revive_mask;
    IMAGE bk_image;

    // Player
    loadimage(&me_image, _T("./images/me1_src.png"));
    loadimage(&me_image_mask, _T("./images/me1_mask.png"));
    loadimage(&wing1_image,_T("./images/me1_src.png"),44,54);
    loadimage(&wing1_mask,_T("./images/me1_mask.png"),44,54);
    // P2 (tinted)
    loadimage(&p2_image, _T("./images/me1_src.png"));
    loadimage(&p2_image_mask, _T("./images/me1_mask.png"));
    loadimage(&wing2_image,_T("./images/me1_src.png"),44,54);
    loadimage(&wing2_mask,_T("./images/me1_mask.png"),44,54);
    {
        IMAGE* orangeImages[2]={&p2_image,&wing2_image};
        IMAGE* orangeMasks[2]={&p2_image_mask,&wing2_mask};
        for(int imageIndex=0;imageIndex<2;imageIndex++) {
            DWORD* buf = GetImageBuffer(orangeImages[imageIndex]);
            DWORD* mb = GetImageBuffer(orangeMasks[imageIndex]);
            int w = orangeImages[imageIndex]->getwidth(), h = orangeImages[imageIndex]->getheight();
            for (int i = 0; i < w*h; i++) {
                if (mb[i] != 0) {
                    BYTE r = (buf[i]>>16)&0xFF, g = (buf[i]>>8)&0xFF, b = buf[i]&0xFF;
                    r = min(255, r*255/128); g = min(255, g*160/128); b = b*40/128;
                    buf[i] = RGB(r,g,b);
                }
            }
        }
    }

    // Enemy1 (already has src+mask)
    loadimage(&enemy_image, _T("./images/enemy1_src.png"));
    loadimage(&enemy_image_mask, _T("./images/enemy1_mask.png"));

    // Enemy2 (has src+mask now)
    loadimage(&enemy2_image, _T("./images/enemy2_src.png"));
    loadimage(&enemy2_image_mask, _T("./images/enemy2_mask.png"));

    // Enemy3 (generated from enemy3_n1.png)
    loadimage(&enemy3_image, _T("./images/enemy3_src.png"));
    loadimage(&enemy3_image_mask, _T("./images/enemy3_mask.png"));

    // 哭哭小喵：被击败小怪抛物线掉出屏幕
    auto loadWithKeyMask = [](IMAGE& out, IMAGE& outMask, LPCTSTR file) {
        IMAGE raw; loadimage(&raw, file);
        int w = raw.getwidth(), h = raw.getheight();
        if (w <= 0 || h <= 0) return;
        out.Resize(w, h); outMask.Resize(w, h);
        DWORD* srcBuf = GetImageBuffer(&raw);
        DWORD* dstBuf = GetImageBuffer(&out);
        DWORD* maskBuf = GetImageBuffer(&outMask);
        if (!srcBuf || !dstBuf || !maskBuf) return;
        DWORD keyColor = srcBuf[0] & 0x00FFFFFF;
        for (int j = 0; j < w * h; j++) {
            DWORD c = srcBuf[j] & 0x00FFFFFF;
            int dr = abs((int)((c >> 16) & 255) - (int)((keyColor >> 16) & 255));
            int dg = abs((int)((c >> 8) & 255) - (int)((keyColor >> 8) & 255));
            int db = abs((int)(c & 255) - (int)(keyColor & 255));
            if (dr + dg + db < 36) {
                dstBuf[j] = BLACK;
                maskBuf[j] = WHITE;
            } else {
                dstBuf[j] = srcBuf[j];
                maskBuf[j] = BLACK;
            }
        }
    };
    loadWithKeyMask(enemy1_death_src, enemy1_death_mask, _T("./images/enemy1_death.png"));
    loadWithKeyMask(enemy2_death_src, enemy2_death_mask, _T("./images/enemy2death.png"));

    // Bullet
    // 玩家子弹（弹头朝上，正好朝上射——保持原图）
    IMAGE bullet_src_raw, bullet_mask_raw;
    loadimage(&bullet_src_raw, _T("./images/bullet_player.png"));
    int bulletW = bullet_src_raw.getwidth();
    int bulletH = bullet_src_raw.getheight();
    bullet_image.Resize(bulletW, bulletH);
    bullet_image_mask.Resize(bulletW, bulletH);   // 必须先 Resize，否则 GetImageBuffer 返回 nullptr
    DWORD* srcBuf = GetImageBuffer(&bullet_src_raw);
    DWORD* dstBuf = GetImageBuffer(&bullet_image);
    DWORD* maskBuf = GetImageBuffer(&bullet_image_mask);
    if (srcBuf && dstBuf && maskBuf && bulletW > 0 && bulletH > 0) {
        DWORD keyColor = srcBuf[0] & 0x00FFFFFF;
        for (int j = 0; j < bulletW * bulletH; j++) {
            DWORD c = srcBuf[j] & 0x00FFFFFF;
            int dr = abs((int)((c >> 16) & 255) - (int)((keyColor >> 16) & 255));
            int dg = abs((int)((c >> 8) & 255) - (int)((keyColor >> 8) & 255));
            int db = abs((int)(c & 255) - (int)(keyColor & 255));
            if (dr + dg + db < 36) {
                dstBuf[j] = BLACK;
                maskBuf[j] = WHITE;
            } else {
                dstBuf[j] = srcBuf[j];
                maskBuf[j] = BLACK;
            }
        }
    }

    // 敌人子弹（原图弹头朝上，敌人向下发射——先上下翻转生成新图与掩码）
    IMAGE enemy_bullet_raw;
    loadimage(&enemy_bullet_raw, _T("./images/bullet_enermy.png"));
    int ebW = enemy_bullet_raw.getwidth();
    int ebH = enemy_bullet_raw.getheight();
    if (ebW > 0 && ebH > 0) {
        DWORD* ebRaw = GetImageBuffer(&enemy_bullet_raw);
        DWORD ebKeyColor = ebRaw[0] & 0x00FFFFFF;
        // 先生成去底/掩码（弹头朝上版）
        IMAGE ebUpright; ebUpright.Resize(ebW, ebH);
        IMAGE ebUprightMask; ebUprightMask.Resize(ebW, ebH);
        DWORD* ebU = GetImageBuffer(&ebUpright);
        DWORD* ebUM = GetImageBuffer(&ebUprightMask);
        for (int y = 0; y < ebH; y++) {
            for (int x = 0; x < ebW; x++) {
                int idx = y * ebW + x;
                DWORD c = ebRaw[idx] & 0x00FFFFFF;
                int dr = abs((int)((c >> 16) & 255) - (int)((ebKeyColor >> 16) & 255));
                int dg = abs((int)((c >> 8) & 255) - (int)((ebKeyColor >> 8) & 255));
                int db = abs((int)(c & 255) - (int)(ebKeyColor & 255));
                if (dr + dg + db < 36) {
                    ebU[idx] = BLACK;
                    ebUM[idx] = WHITE;
                } else {
                    ebU[idx] = ebRaw[idx];
                    ebUM[idx] = BLACK;
                }
            }
        }
        enemy_bullet_image.Resize(ebW, ebH);
        enemy_bullet_image_mask.Resize(ebW, ebH);
        DWORD* ebD = GetImageBuffer(&enemy_bullet_image);
        DWORD* ebDM = GetImageBuffer(&enemy_bullet_image_mask);
        // 180° 旋转：对 src/掩码 各自镜像翻转。
        // 注：180° 翻转后 BLACK/WHITE 含义不变，所以掩码也可以同样翻转。
        for (int y = 0; y < ebH; y++) {
            for (int x = 0; x < ebW; x++) {
                int srcIdx = y * ebW + x;
                int dstIdx = (ebH - 1 - y) * ebW + (ebW - 1 - x);
                ebD[dstIdx] = ebU[srcIdx];
                ebDM[dstIdx] = ebUM[srcIdx];
            }
        }
    }

    // Power-up icons
    load_power_icon(fire_icon,fire_mask,_T("./images/bullet_supply.png"));
    loadimage(&life_icon,_T("./images/aid_life_src.png"));
    loadimage(&life_mask,_T("./images/aid_life_mask.png"));
    load_power_icon(bomb_icon,bomb_mask,_T("./images/bomb_supply.png"));
    load_power_icon(shield_icon,shield_mask,_T("./images/shield_supply.png"));
    load_power_icon(drone_icon,drone_mask,_T("./images/me2.png"));
    loadimage(&magnet_icon,_T("./images/magnet_src.png"));loadimage(&magnet_mask,_T("./images/magnet_mask.png"));
    loadimage(&freeze_icon,_T("./images/freeze_src.png"));loadimage(&freeze_mask,_T("./images/freeze_mask.png"));
    loadimage(&pierce_icon,_T("./images/pierce_src.png"));loadimage(&pierce_mask,_T("./images/pierce_mask.png"));
    loadimage(&overload_icon,_T("./images/overload_src.png"));loadimage(&overload_mask,_T("./images/overload_mask.png"));
    loadimage(&revive_icon,_T("./images/revive_src.png"));loadimage(&revive_mask,_T("./images/revive_mask.png"));
    POWER_ART powerArt[POWER_COUNT]={
        {&fire_icon,&fire_mask},{&drone_icon,&drone_mask},{&life_icon,&life_mask},{&bomb_icon,&bomb_mask},{&shield_icon,&shield_mask},
        {&magnet_icon,&magnet_mask},{&freeze_icon,&freeze_mask},{&pierce_icon,&pierce_mask},{&overload_icon,&overload_mask},{&revive_icon,&revive_mask}
    };

    // Background
    loadimage(&bk_image, _T("./images/bk2.png"), WIDTH, HEIGHT*2);

    // Enemy1 explosions (original src+mask)
    IMAGE e1boom[3], e1boom_mask[3];
    const TCHAR* e1bFiles[] = {
        _T("./images/enemy1_down2_src.png"), _T("./images/enemy1_down2_mask.png"),
        _T("./images/enemy1_down3_src.png"), _T("./images/enemy1_down3_mask.png"),
        _T("./images/enemy1_down4_src.png"), _T("./images/enemy1_down4_mask.png") };
    for (int i = 0; i < 3; i++) {
        loadimage(&e1boom[i], e1bFiles[i*2]);
        loadimage(&e1boom_mask[i], e1bFiles[i*2+1]);
    }

    // Enemy2 explosions (processed)
    IMAGE e2boom[4], e2boom_mask[4];
    const TCHAR* e2F[] = {_T("./images/enemy2_down1_src.png"),_T("./images/enemy2_down1_mask.png"),
        _T("./images/enemy2_down2_src.png"),_T("./images/enemy2_down2_mask.png"),
        _T("./images/enemy2_down3_src.png"),_T("./images/enemy2_down3_mask.png"),
        _T("./images/enemy2_down4_src.png"),_T("./images/enemy2_down4_mask.png")};
    for (int i=0; i<4; i++) {
        loadimage(&e2boom[i], e2F[i*2]);
        loadimage(&e2boom_mask[i], e2F[i*2+1]);
    }

    // Enemy3 explosions (processed)
    IMAGE e3boom[6], e3boom_mask[6];
    const TCHAR* e3F[] = {_T("./images/enemy3_down1_src.png"),_T("./images/enemy3_down1_mask.png"),
        _T("./images/enemy3_down2_src.png"),_T("./images/enemy3_down2_mask.png"),
        _T("./images/enemy3_down3_src.png"),_T("./images/enemy3_down3_mask.png"),
        _T("./images/enemy3_down4_src.png"),_T("./images/enemy3_down4_mask.png"),
        _T("./images/enemy3_down5_src.png"),_T("./images/enemy3_down5_mask.png"),
        _T("./images/enemy3_down6_src.png"),_T("./images/enemy3_down6_mask.png")};
    for (int i=0; i<6; i++) {
        loadimage(&e3boom[i], e3F[i*2]);
        loadimage(&e3boom_mask[i], e3F[i*2+1]);
    }

    BK bk(bk_image);

    // HP UI 资源 (icon + 心形)
    IMAGE icon1_img, icon2_img, heart1_img, heart2_img;
    loadimage(&icon1_img, _T("./images/icon1.png"));
    loadimage(&icon2_img, _T("./images/icon2.png"));
    loadimage(&heart1_img, _T("./images/heart1.png"));
    loadimage(&heart2_img, _T("./images/heart2.png"));

    // 初始化小怪精灵图与统一爆炸图
    if (!gEnemy1Sprite) gEnemy1Sprite = new AnimatedSprite(_T("./images/enemy1-Sheet.png"), 57, 57, 2, 6);
    if (!gEnemy2Sprite) gEnemy2Sprite = new AnimatedSprite(_T("./images/enemy2_sheets.png"), 77, 72, 3, 8, _T("./images/enemy2_attack.png"));
    if (!gBossSprite) gBossSprite = new AnimatedSprite(_T("./images/boss-Sheet.png"), 203, 185, 4, 6);
    if (!gSmallBoom) gSmallBoom = new BoomSheet(_T("./images/boom_enermu-Sheet.png"), 60, 60, 5);

    // P1 使用精灵表动画 (player-Sheet.png: 3帧)
    // 注意：需要知道每帧的尺寸。如果不确定，可以从图片总尺寸除以帧数计算
    AnimatedME me(_T("./images/player-Sheet.png"), 126, 126, 3, 10);
    ME me2(p2_image, p2_image_mask);
    WINGMAN wing1(wing1_image,wing1_mask);
    WINGMAN wing2(wing2_image,wing2_mask);
    me2.GetRect().left = WIDTH/4 - me.GetWidth()/2;
    me2.GetRect().right = me2.GetRect().left + me.GetWidth();
    me2.GetRect().bottom = HEIGHT;
    me2.GetRect().top = HEIGHT - me_image.getheight();

    vector<ENEMY*> es;
    vector<EBULLET*> ebs, p1bs, p2bs;
    vector<POWERUP*> powerups;
    vector<Ptcl> effectParticles;
    vector<DeadCat> deadCats;

    // ---- 死亡动画资源 ----
    // player_boom: 504x126, 4 帧, 每帧 126x126, ~8 fps
    // death1:      252x126, 2 帧, 每帧 126x126, ~5 fps, 播完停在最后一帧
    // death2:      252x126, 2 帧, 每帧 126x126, ~5 fps, 同时缓慢下坠+旋转出屏
    constexpr int DEATH_FRAME = 126;
    constexpr int PLAYER_BOOM_FRAMES = 4;
    constexpr int PLAYER_BOOM_FPS = 8;
    constexpr int DEATH1_FRAMES = 2;
    constexpr int DEATH1_FPS = 5;
    constexpr int DEATH1_HOLD_MS = 2000;  // 播完后停 2 秒
    constexpr int DEATH2_FRAMES = 2;
    constexpr int DEATH2_FPS = 5;
    IMAGE player_boom_src[PLAYER_BOOM_FRAMES];
    IMAGE player_boom_mask[PLAYER_BOOM_FRAMES];
    IMAGE death1_src[DEATH1_FRAMES];
    IMAGE death1_mask[DEATH1_FRAMES];
    IMAGE death2_src[DEATH2_FRAMES];
    IMAGE death2_mask[DEATH2_FRAMES];
    LoadSpriteSheetFrames(player_boom_src, PLAYER_BOOM_FRAMES, _T("./images/player_boom.png"), DEATH_FRAME, DEATH_FRAME);
    LoadSpriteSheetFrames(death1_src, DEATH1_FRAMES, _T("./images/death1.png"), DEATH_FRAME, DEATH_FRAME);
    LoadSpriteSheetFrames(death2_src, DEATH2_FRAMES, _T("./images/death2.png"), DEATH_FRAME, DEATH_FRAME);
    BuildMaskArrayFromAlpha(player_boom_src, player_boom_mask, PLAYER_BOOM_FRAMES, DEATH_FRAME, DEATH_FRAME);
    BuildMaskArrayFromAlpha(death1_src, death1_mask, DEATH1_FRAMES, DEATH_FRAME, DEATH_FRAME);
    BuildMaskArrayFromAlpha(death2_src, death2_mask, DEATH2_FRAMES, DEATH_FRAME, DEATH_FRAME);

    // 死亡阶段：0=未触发, 1=boom, 2=death1 动画+等待, 3=death2 下坠, 4=动画结束->退出 Play()
    // 仅 gameMode==1（单人模式）才走这个流程
    int deathPhase = 0;
    int deathAnimFrame = 0;
    ULONGLONG deathAnimFrameAt = 0;
    ULONGLONG deathPhaseAt = 0;  // 当前阶段进入时间
    ULONGLONG phase21EnterAt = 0;  // 21 阶段进入时间（专用，避免被其他代码干扰）
    int phase21FrameCount = 0;  // 21 阶段已等待的帧数（备用）
    int deathCenterX = 0, deathCenterY = 0;  // 死亡时玩家中心点
    float death2X = 0, death2Y = 0, death2VX = 0, death2VY = 0, death2Rot = 0, death2RotV = 0;
    int death2Frame = 0;

    ULONGLONG hurtlast = 0, hurtlast2 = 0;
    ULONGLONG gameStart = GetTickCount();
    ULONGLONG levelStart = gameStart;
    ULONGLONG levelElapsedBase = 0;
    ULONGLONG lastSpawn = gameStart - 1200;
    ULONGLONG levelBannerUntil = gameStart + 2000;
    unsigned long long levelScoreStart = 0;
    int currentLevel = 1;
    bool bossActive = false;
    ULONGLONG firePowerUntil1=0,firePowerUntil2=0;
    ULONGLONG droneUntil1=0,droneUntil2=0;
    ULONGLONG magnetUntil1=0,magnetUntil2=0,pierceUntil1=0,pierceUntil2=0;
    ULONGLONG overloadUntil1=0,overloadUntil2=0,overloadCooldown1=0,overloadCooldown2=0,freezeUntil=0;
    ULONGLONG wingShootClock1=0,wingShootClock2=0;
    ULONGLONG p1DeathAt=0,p2DeathAt=0,linkShieldReadyAt=0;
    ULONGLONG shakeUntil=0,bombFxUntil=0,phaseBannerUntil=0,bossSummonClock=0;
    int bombCount=0;
    bool bombLast=false,bomb2Last=false;int pendingBombPlayer=0;ULONGLONG pendingBombAt=0;
    int comboCount=0,grazeCount=0,p1Kills=0,p2Kills=0,p1Contribution=0,p2Contribution=0,lastBossPhase=1;
    ULONGLONG comboLastKill=0;
    UPGRADE_STATE upgrade;

    if(resuming){
        currentLevel=saved.level;levelScoreStart=saved.levelScoreStart;upgrade=saved.upgrade;
        levelElapsedBase=saved.levelElapsedMs;bombCount=max(0,saved.bombCount);
        p1Dead=saved.p1Dead;p2Dead=saved.p2Dead;
        me.SetUpgradeBonuses(upgrade.moveSpeed,upgrade.shieldCapacity);me2.SetUpgradeBonuses(upgrade.moveSpeed,upgrade.shieldCapacity);
        me.Restore(saved.p1Hp,saved.p1Shield,saved.p1Rect,saved.version>=2?saved.p1ShieldCount:-1);
        me2.Restore(saved.p2Hp,saved.p2Shield,saved.p2Rect,saved.version>=2?saved.p2ShieldCount:-1);
        p1Kills=saved.p1Kills;p2Kills=saved.p2Kills;comboCount=saved.combo;grazeCount=saved.graze;
        firePowerUntil1=gameStart+max(0,saved.p1FireMs);firePowerUntil2=gameStart+max(0,saved.p2FireMs);
        droneUntil1=gameStart+max(0,saved.p1DroneMs);droneUntil2=gameStart+max(0,saved.p2DroneMs);
        bossActive=saved.bossActive;
        if(bossActive){
            ENEMY_TYPE3* boss=new ENEMY_TYPE3(enemy3_image,enemy3_image_mask,e3boom,e3boom_mask,currentLevel,gameMode);
            boss->RestoreHP(saved.bossHp);es.push_back(boss);
        }
    }else{me.SetUpgradeBonuses(upgrade.moveSpeed,upgrade.shieldCapacity);me2.SetUpgradeBonuses(upgrade.moveSpeed,upgrade.shieldCapacity);}
    if(p1Dead)p1DeathAt=gameStart;if(p2Dead)p2DeathAt=gameStart;

    while (is_play) {
        ULONGLONG frameStart = GetTickCount();
        bsing++;
        // 死亡动画阶段 2 起：停止背景音乐（阶段 1 期间继续播放）
        if (gameMode==1 && deathPhase >= 2) {
            PlayMusic(MUSIC_NONE);
        } else {
            PlayMusic(GameplayMusic(runMode,currentLevel,bossActive));
        }

        // EasyX may occasionally leave VS Code as the active window after a
        // debug launch. A click anywhere in the game client area explicitly
        // gives keyboard focus back to the game.
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
            POINT cursor;
            GetCursorPos(&cursor);
            ScreenToClient(gameWindow, &cursor);
            RECT client;
            GetClientRect(gameWindow, &client);
            if (PtInRect(&client, cursor)) {
                SetForegroundWindow(gameWindow);
                SetActiveWindow(gameWindow);
                SetFocus(gameWindow);
            }
        }

        // Optional mouse-drag control, adapted from the reference touch-control layer.
        ExMessage mouseMessage;
        while (peekmessage(&mouseMessage,EX_MOUSE))
            if (!p1Dead && mouseMessage.lbutton)
                me.MoveCenterTo(mouseMessage.x,mouseMessage.y);

        bool wing1Active=!p1Dead && frameStart<droneUntil1;
        bool wing2Active=gameMode==2 && !p2Dead && frameStart<droneUntil2;
        if (wing1Active) wing1.Update(me.GetRect(),true); else wing1.Hide();
        if (wing2Active) wing2.Update(me2.GetRect(),false); else wing2.Hide();

        if(comboCount>0&&frameStart-comboLastKill>COMBO_TIMEOUT_MS)comboCount=0;
        if(overloadUntil1&&frameStart>=overloadUntil1){overloadUntil1=0;overloadCooldown1=frameStart+OVERLOAD_COOLDOWN_MS;}
        if(overloadUntil2&&frameStart>=overloadUntil2){overloadUntil2=0;overloadCooldown2=frameStart+OVERLOAD_COOLDOWN_MS;}
        gEnemyMoveScale=frameStart<freezeUntil?0.42f:1.0f;

        auto scoreMultiplier=[&](){return 1.0f+min(20,comboCount)*0.05f;};
        auto awardKill=[&](ENEMY* enemy,int owner){
            comboCount++;comboLastKill=frameStart;int gained=(int)lroundf(enemy->scoreVal*scoreMultiplier());totalScore+=gained;
            if(owner==1){p1Kills++;p1Contribution+=gained;}else if(owner==2){p2Kills++;p2Contribution+=gained;}
            shakeUntil=max(shakeUntil,frameStart+110);
            // 非 Boss 小怪：随机抛出一只哭哭小喵 + 偶尔播放猫叫
            if (!enemy->IsBoss()) {
                PlayMeowRandom();
                DeadCat dc;
                RECT er = enemy->GetRect();
                int cx = (er.left + er.right) / 2;
                int cy = (er.top + er.bottom) / 2;
                dc.x = (float)cx;
                dc.y = (float)cy;
                // 给一个向斜上/斜两侧的初速度，让小喵先飞起来再被重力拉下来
                float ang = (float)(-PI/2 + ((rand() % 120) - 60) * PI / 180.0f);  // -30° ~ +30° 相对于"正上"
                float spd = 2.5f + (rand() % 100) / 30.0f;  // ~2.5 ~ 5.8
                dc.vx = cosf(ang) * spd;
                dc.vy = sinf(ang) * spd;
                int life = 2400 + rand() % 1200;
                dc.life = life; dc.lmax = life;
                // 根据敌人种类选对应的死亡贴图：enemy1 → enemy1_death，enemy2 → enemy2death，其他用 enemy2death 作为通用兜底
                ENEMY::Kind k = enemy->GetKind();
                if (k == ENEMY::KIND_TYPE1) {
                    dc.img = &enemy1_death_src; dc.img_mask = &enemy1_death_mask;
                } else if (k == ENEMY::KIND_TYPE2) {
                    dc.img = &enemy2_death_src; dc.img_mask = &enemy2_death_mask;
                } else {
                    // TYPE4/TYPE5/FORMATION/BOSS：兜底用 enemy2_death（视觉与 enemy2 最接近）
                    dc.img = &enemy2_death_src; dc.img_mask = &enemy2_death_mask;
                }
                dc.rot = 0.0f;
                dc.rotV = ((rand() % 200) - 100) / 1000.0f;  // ±0.1 弧度/帧的慢转
                deadCats.push_back(dc);
            }
        };

        auto firePlayerBullets = [&](vector<EBULLET*>& shots,RECT plane,bool powered,bool overloaded,bool temporaryPierce,int hp) {
            int count=1+upgrade.sideBullet*2;if(powered)count=max(count,3);if(overloaded)count=max(count,5);
            for (int i=0;i<count;i++) {
                float center=(count-1)/2.0f;float offset=(i-center)*16.0f;float direction=(i-center)*0.16f;
                EBULLET* eb=new EBULLET();
                eb->Init(bullet_image,bullet_image_mask,plane,direction,-1,overloaded?7:5);
                eb->x+=offset;eb->rect.left=(LONG)lroundf(eb->x);eb->rect.right=eb->rect.left+bullet_image.getwidth();
                int bh=bullet_image.getheight();
                eb->rect.top=plane.top-bh; eb->rect.bottom=plane.top;
                eb->y=(float)eb->rect.top;
                eb->damage=upgrade.bulletDamage*((upgrade.lowHpDouble&&hp<=1)?2:1);
                eb->pierceLeft=(upgrade.pierce||temporaryPierce)?1:0;
                shots.push_back(eb);
            }
            PlayGameSound(SOUND_SHOOT);
        };

        int fireInterval=220;for(int i=0;i<upgrade.fireRateLevel;i++)fireInterval=max(70,fireInterval*85/100);

        // ---- P1 shoot ----
        if (!p1Dead) {
            bool sNow = (GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
            if (sNow && frameStart>=overloadCooldown1 && frameStart-p1ShootClock>(frameStart<overloadUntil1?70:fireInterval)) {
                firePlayerBullets(p1bs,me.GetRect(),frameStart<firePowerUntil1,frameStart<overloadUntil1,frameStart<pierceUntil1,me.GetHP());
                p1ShootClock = frameStart;
            }
            spaceLast = sNow;
        }

        // ---- P2 shoot ----
        if (gameMode==2 && !p2Dead) {
            bool n1Now = ((GetAsyncKeyState(0x61)|GetAsyncKeyState('1')|GetAsyncKeyState('J'))&0x8000)!=0;
            if (n1Now && frameStart>=overloadCooldown2 && frameStart-p2ShootClock>(frameStart<overloadUntil2?70:fireInterval)) {
                firePlayerBullets(p2bs,me2.GetRect(),frameStart<firePowerUntil2,frameStart<overloadUntil2,frameStart<pierceUntil2,me2.GetHP());
                p2ShootClock = frameStart;
            }
            num1Last = n1Now;
        }

        // Drone pickups deploy a small follower that shoots independently.
        if (wing1Active && frameStart-wingShootClock1>=WINGMAN_FIRE_INTERVAL_MS) {
            firePlayerBullets(p1bs,wing1.GetRect(),false,false,frameStart<pierceUntil1,me.GetHP());wingShootClock1=frameStart;
        }
        if (wing2Active && frameStart-wingShootClock2>=WINGMAN_FIRE_INTERVAL_MS) {
            firePlayerBullets(p2bs,wing2.GetRect(),false,false,frameStart<pierceUntil2,me2.GetHP());wingShootClock2=frameStart;
        }

        // ---- Shared bombs: P1 uses B, P2 uses N; presses within 280 ms combine ----
        auto executeBomb=[&](bool combined,int owner){
            if(bombCount<=0)return;bombCount--;int bossDamage=BOMB_BOSS_DAMAGE+upgrade.bombDamage*4;if(combined)bossDamage*=2;
            for(auto e:es)if(!e->is_died){RECT er=e->GetRect();SpawnParticles(effectParticles,(er.left+er.right)/2,(er.top+er.bottom)/2,
                combined?RGB(100,220,255):RGB(255,150,45),e->IsBoss()?32:18,650);
                if(e->IsBoss()){e->TakeDamage(bossDamage);if(e->is_died)awardKill(e,owner);}else{e->IsDied();PlayGameSound(SOUND_EXPLOSION);awardKill(e,owner);}}
            for(auto b:ebs)delete b;ebs.clear();bombFxUntil=frameStart+(combined?760:560);shakeUntil=frameStart+(combined?420:260);PlayGameSound(SOUND_EXPLOSION);
        };
        bool bombNow=(GetAsyncKeyState('B')&0x8000)!=0;
        bool bomb2Now=gameMode==2&&(GetAsyncKeyState('N')&0x8000)!=0;
        bool p1BombPressed=bombNow&&!bombLast,p2BombPressed=bomb2Now&&!bomb2Last;
        if(gameMode==1&&p1BombPressed)executeBomb(false,1);
        else{
            if(p1BombPressed){if(pendingBombPlayer==2&&frameStart-pendingBombAt<=280){executeBomb(true,1);pendingBombPlayer=0;}else{pendingBombPlayer=1;pendingBombAt=frameStart;}}
            if(p2BombPressed){if(pendingBombPlayer==1&&frameStart-pendingBombAt<=280){executeBomb(true,2);pendingBombPlayer=0;}else{pendingBombPlayer=2;pendingBombAt=frameStart;}}
            if(pendingBombPlayer&&frameStart-pendingBombAt>280){executeBomb(false,pendingBombPlayer);pendingBombPlayer=0;}
        }
        bombLast=bombNow;bomb2Last=bomb2Now;

        // ---- End-of-level Boss trigger ----
        ULONGLONG levelPlayedMs = levelElapsedBase+frameStart-levelStart;
        if(runMode==2)currentLevel=1+(int)(levelPlayedMs/60000);
        unsigned long long levelTargetScore = 260ULL + currentLevel*120ULL;
        if (runMode==1 && !bossActive && (levelPlayedMs>=45000 || totalScore-levelScoreStart>=levelTargetScore)) {
            for (auto e:es) delete e; es.clear();
            for (auto b:ebs) delete b; ebs.clear();
            es.push_back(new ENEMY_TYPE3(enemy3_image,enemy3_image_mask,e3boom,e3boom_mask,currentLevel,gameMode));
            bossActive=true;
            levelBannerUntil=frameStart+2000;
            bossSummonClock=frameStart;lastBossPhase=1;PlayGameSound(SOUND_BOSS);
        }

        // ---- Enemy bullets (each enemy owns its own attack timer) ----
        ENEMY* activeBoss=nullptr;
        for(auto e:es)if(e->IsBoss()&&!e->is_died){activeBoss=e;break;}
        if(activeBoss&&activeBoss->GetPhase()!=lastBossPhase){
            lastBossPhase=activeBoss->GetPhase();phaseBannerUntil=frameStart+1250;
            for(auto b:ebs)delete b;ebs.clear();
        }
        // Level 2 Boss specializes in calling two converging formations.
        if(activeBoss&&currentLevel==2&&frameStart-bossSummonClock>=6500&&!activeBoss->IsTransitioning()){
            int leftCenter=WIDTH/3-enemy2_image.getwidth()/2,rightCenter=WIDTH*2/3-enemy2_image.getwidth()/2;
            for(int slot=-1;slot<=1;slot++){es.push_back(new ENEMY_FORMATION(enemy2_image,enemy2_image_mask,e2boom,e2boom_mask,2,gameMode,slot,leftCenter));
                es.push_back(new ENEMY_FORMATION(enemy2_image,enemy2_image_mask,e2boom,e2boom_mask,2,gameMode,slot,rightCenter));}
            bossSummonClock=frameStart;
        }
        for (auto enemy:es) {
            // In two-player mode each enemy follows/aims at the closer living player.
            RECT er=enemy->GetRect();
            float ex=(er.left+er.right)/2.0f,ey=(er.top+er.bottom)/2.0f;
            RECT targetRect=me.GetRect();
            if (p1Dead && gameMode==2 && !p2Dead) targetRect=me2.GetRect();
            else if (gameMode==2 && !p2Dead && !p1Dead) {
                RECT p2r=me2.GetRect();
                float p1dx=(targetRect.left+targetRect.right)/2.0f-ex,p1dy=(targetRect.top+targetRect.bottom)/2.0f-ey;
                float p2dx=(p2r.left+p2r.right)/2.0f-ex,p2dy=(p2r.top+p2r.bottom)/2.0f-ey;
                if(p2dx*p2dx+p2dy*p2dy<p1dx*p1dx+p1dy*p1dy) targetRect=p2r;
            }
            enemy->SetTarget((targetRect.left+targetRect.right)/2.0f,(targetRect.top+targetRect.bottom)/2.0f);
            if (enemy->CanAttack() && !enemy->is_died && enemy->GetRect().top>=0)
                enemy->GetBullets(ebs, enemy_bullet_image, enemy_bullet_image_mask);
        }
        // Keep long Boss fights stable without freezing enemy movement/timers.
        while (ebs.size()>MAX_ENEMY_BULLETS) { delete ebs.back(); ebs.pop_back(); }

        if(!p1Dead && !(gameMode==1 && deathPhase>0))me.Control(gameMode==1);
        if(gameMode==2&&!p2Dead && !(gameMode==1 && deathPhase>0))me2.Control2();
        bool linkedShield=false;
        if(gameMode==2&&!p1Dead&&!p2Dead){RECT a=me.GetRect(),b=me2.GetRect();float dx=(a.left+a.right-b.left-b.right)/2.0f,dy=(a.top+a.bottom-b.top-b.bottom)/2.0f;linkedShield=dx*dx+dy*dy<=170.0f*170.0f;}

        // ---- Render ----
        bool inDeathAnim = (gameMode == 1 && deathPhase > 0);
        BeginBatchDraw(); cleardevice();
        if (inDeathAnim && deathPhase >= 2) {
            // 阶段 2 起：全黑屏（不画背景）
            setorigin(0,0);
            setbkcolor(BLACK); cleardevice();
        } else {
            // 阶段 1（保留背景滚动）或正常游戏：照旧画背景
            int shakeX=0,shakeY=0;if(frameStart<shakeUntil){shakeX=rand()%9-4;shakeY=rand()%9-4;}setorigin(shakeX,shakeY);
            bk.Show();
            if (!inDeathAnim) {
                ShowParticles(effectParticles);
                UpdateAndShowDeadCats(deadCats, &enemy1_death_src, &enemy1_death_mask);
            }
            if (!inDeathAnim && !p1Dead) { me.Update(); me.Show(); }
            if (gameMode==2 && !inDeathAnim && !p2Dead) me2.Show();
            if (wing1Active && !inDeathAnim) wing1.Show();
            if (wing2Active && !inDeathAnim) wing2.Show();
            if(linkedShield){RECT a=me.GetRect(),b=me2.GetRect();int ax=(a.left+a.right)/2,ay=(a.top+a.bottom)/2,bx=(b.left+b.right)/2,by=(b.top+b.bottom)/2;
                setlinecolor(frameStart>=linkShieldReadyAt?RGB(70,235,255):RGB(90,100,120));setlinestyle(PS_DASH,2);line(ax,ay,bx,by);setlinestyle(PS_SOLID,1);}
        }

        // ---- UI ----
        setbkmode(TRANSPARENT);
        // Score (左上角仍然保留)
        if (gameMode==1) {
            TCHAR inf[64];
            _stprintf_s(inf,64,_T("Score:%llu"),totalScore);
            settextcolor(RED); settextstyle(24,0,_T("\u9ed1\u4f53")); outtextxy(10,10,inf);
        } else {
            TCHAR inf[64];
            _stprintf_s(inf,64,_T("Score:%llu"),totalScore);
            settextcolor(RED); settextstyle(24,0,_T("\u9ed1\u4f53")); outtextxy(10,10,inf);
        }

        // 心心+icon 血量 UI
        // 绘制顺序：icon (最左) -> heart1, heart2, heart3 (从左到右)
        // 当 HP 为 k 时，最右侧的 (SUMHP - k) 个心变成 heart2
        {
            auto drawHpBar = [&](int hp, int anchorX, int anchorY, bool anchorRight) {
                int iconW = icon1_img.getwidth();
                int iconH = icon1_img.getheight();
                int heartW = heart1_img.getwidth();
                int heartH = heart1_img.getheight();
                int gap = 6;  // icon 与心之间间距
                int heartGap = 4;
                int totalW = iconW + gap + SUMHP * heartW + (SUMHP - 1) * heartGap;
                int startX = anchorRight ? anchorX - totalW : anchorX;
                int y = anchorY;

                // icon
                IMAGE& icon = (hp <= 1) ? icon2_img : icon1_img;
                putimage(startX, y, &icon);
                // hearts
                int heartsStartX = startX + iconW + gap;
                for (int i = 0; i < SUMHP; i++) {
                    // i 从左到右。最右侧的 i = SUMHP-1
                    // 当 HP=k 时，第 0..(k-1) 是 heart1，其余 (SUMHP-k) 个是 heart2 (右侧)
                    bool isFull = (i < hp);
                    IMAGE& heart = isFull ? heart1_img : heart2_img;
                    putimage(heartsStartX + i * (heartW + heartGap), y, &heart);
                }
            };
            int hpY = 12;  // 与上方 Score 同基线
            if (!p1Dead) drawHpBar(me.GetHP(), 10, hpY, false);
            if (gameMode==2 && !p2Dead) drawHpBar(me2.GetHP(), WIDTH - 10, hpY, true);
        }

        TCHAR levelText[32];
        if(runMode==1)_stprintf_s(levelText,32,_T("LEVEL %d / 3"),currentLevel);
        else _stprintf_s(levelText,32,_T("ENDLESS  TIER %d"),currentLevel);
        settextcolor(RGB(30,80,180)); settextstyle(22,0,_T("\u9ed1\u4f53"));
        outtextxy(WIDTH/2-textwidth(levelText)/2,12,levelText);

        int fireSeconds1=frameStart<firePowerUntil1?(int)((firePowerUntil1-frameStart+999)/1000):0;
        int droneSeconds1=frameStart<droneUntil1?(int)((droneUntil1-frameStart+999)/1000):0;
        TCHAR itemText[96];
        _stprintf_s(itemText,96,_T("BOMB B/N:%d  FIRE:%ds  DRONE:%ds"),
            bombCount,fireSeconds1,droneSeconds1);
        settextcolor(RGB(30,110,190)); settextstyle(19,0,_T("\u9ed1\u4f53"));
        outtextxy(10,66,itemText);

        ENEMY* bossForUi=nullptr;
        for (auto e:es) if (e->IsBoss() && !e->is_died) { bossForUi=e; break; }
        if (bossForUi) {
            const int bx=120, by=100, bw=360, bh=20;
            setfillcolor(RGB(55,55,65)); solidrectangle(bx,by,bx+bw,by+bh);
            int hp=bossForUi->GetHP(); if (hp<0) hp=0;
            int fill=bw*hp/bossForUi->GetMaxHP();
            setfillcolor(bossForUi->IsHitFlashing()?WHITE:RGB(220,45,45)); solidrectangle(bx,by,bx+fill,by+bh);
            setlinecolor(BLACK); rectangle(bx,by,bx+bw,by+bh);
            TCHAR bossText[32]; _stprintf_s(bossText,32,_T("BOSS  %d / %d"),hp,bossForUi->GetMaxHP());
            settextcolor(WHITE); settextstyle(18,0,_T("\u9ed1\u4f53"));
            OutTextShadow(WIDTH/2-textwidth(bossText)/2,by+23,bossText);
        }

        if (frameStart<levelBannerUntil) {
            TCHAR banner[48];
            if (bossActive) _stprintf_s(banner,48,_T("LEVEL %d  BOSS"),currentLevel);
            else _stprintf_s(banner,48,_T("LEVEL %d  START"),currentLevel);
            settextcolor(RGB(200,45,45)); settextstyle(42,0,_T("\u9ed1\u4f53"));
            outtextxy(WIDTH/2-textwidth(banner)/2,HEIGHT/2-50,banner);
        }

        if(frameStart<phaseBannerUntil&&activeBoss){
            TCHAR phaseText[64];_stprintf_s(phaseText,64,_T("BOSS PHASE %d - \u505c\u706b\u8b66\u544a"),activeBoss->GetPhase());
            settextcolor(RGB(255,70,55));settextstyle(34,0,_T("\u9ed1\u4f53"));outtextxy(WIDTH/2-textwidth(phaseText)/2,190,phaseText);
        }

        TCHAR comboText[96];_stprintf_s(comboText,96,_T("COMBO x%d   SCORE x%.2f   GRAZE %d"),comboCount,scoreMultiplier(),grazeCount);
        settextcolor(comboCount>=10?RGB(255,190,35):RGB(230,235,255));settextstyle(18,0,_T("Consolas"));outtextxy(10,90,comboText);
        if(gameMode==2){TCHAR team[128];_stprintf_s(team,128,_T("P1 K:%d C:%d   P2 K:%d C:%d"),p1Kills,p1Contribution,p2Kills,p2Contribution);
            settextcolor(RGB(210,225,255));outtextxy(WIDTH/2-textwidth(team)/2,950,team);}
        if(gameMode==2&&(p1Dead!=p2Dead)){
            ULONGLONG diedAt=p1Dead?p1DeathAt:p2DeathAt;int remain=max(0,(int)((REVIVE_HOLD_MS-(min(REVIVE_HOLD_MS,frameStart-diedAt))+999)/1000));
            TCHAR reviveText[64];_stprintf_s(reviveText,64,_T("\u575a\u6301 %d \u79d2\u590d\u6d3b %s"),remain,p1Dead?_T("P1"):_T("P2"));
            settextcolor(RGB(80,245,160));settextstyle(25,0,_T("\u9ed1\u4f53"));outtextxy(WIDTH/2-textwidth(reviveText)/2,880,reviveText);
        }

        for (auto& b : p1bs) if (!inDeathAnim) b->Show();
        if (gameMode==2) for (auto& b : p2bs) if (!inDeathAnim) b->Show();
        for (auto& b : ebs) if (!inDeathAnim) b->Show(frameStart<freezeUntil?0.42f:1.0f);

        if(frameStart<bombFxUntil){
            ULONGLONG left=bombFxUntil-frameStart;if(left>480){setfillcolor(WHITE);solidrectangle(0,0,WIDTH,HEIGHT);}
            int radius=(int)((760-min<ULONGLONG>(760,left))*0.75);setlinecolor(left%100<50?RGB(255,230,80):RGB(80,220,255));setlinestyle(PS_SOLID,5);circle(WIDTH/2,HEIGHT/2,radius);setlinestyle(PS_SOLID,1);
        }

        auto applyPower = [&](POWER_TYPE type,int owner) {
            ULONGLONG& fireUntil=owner==1?firePowerUntil1:firePowerUntil2;
            ULONGLONG& droneUntil=owner==1?droneUntil1:droneUntil2;ULONGLONG& magnetUntil=owner==1?magnetUntil1:magnetUntil2;
            ULONGLONG& pierceUntil=owner==1?pierceUntil1:pierceUntil2;ULONGLONG& overloadUntil=owner==1?overloadUntil1:overloadUntil2;
            ULONGLONG& overloadCooldown=owner==1?overloadCooldown1:overloadCooldown2;
            if(type==POWER_FIRE)fireUntil=frameStart+FIRE_POWER_DURATION_MS;
            else if(type==POWER_DRONE)droneUntil=frameStart+DRONE_DURATION_MS;
            else if(type==POWER_LIFE){if(owner==1)me.Heal();else me2.Heal();}
            else if(type==POWER_BOMB)bombCount++;
            else if(type==POWER_SHIELD){if(owner==1)me.GrantShield();else me2.GrantShield();}
            else if(type==POWER_MAGNET)magnetUntil=frameStart+MAGNET_DURATION_MS;
            else if(type==POWER_FREEZE)freezeUntil=frameStart+FREEZE_DURATION_MS;
            else if(type==POWER_PIERCE)pierceUntil=frameStart+PIERCE_DURATION_MS;
            else if(type==POWER_OVERLOAD){overloadUntil=frameStart+OVERLOAD_DURATION_MS;overloadCooldown=0;}
            else if(type==POWER_REVIVE&&gameMode==2){
                if(owner==1&&p2Dead){p2Dead=false;p2DeathAt=0;me2.Revive();}
                else if(owner==2&&p1Dead){p1Dead=false;p1DeathAt=0;me.Revive();}
                else {if(owner==1)me.Heal();else me2.Heal();}
            }
            PlayGameSound(SOUND_PICKUP);
        };
        for (auto pit=powerups.begin();pit!=powerups.end();) {
            POWERUP* p=*pit;
            // 死亡动画阶段：道具静止且不渲染
            if (inDeathAnim) { ++pit; continue; }
            bool magnetic=false;float mx=0,my=0,best=1e9f;
            if(!p1Dead&&frameStart<magnetUntil1){RECT r=me.GetRect();float x=(r.left+r.right)/2.0f,y=(r.top+r.bottom)/2.0f,dx=x-(p->rect.left+p->rect.right)/2.0f,dy=y-(p->rect.top+p->rect.bottom)/2.0f;best=dx*dx+dy*dy;mx=x;my=y;magnetic=true;}
            if(gameMode==2&&!p2Dead&&frameStart<magnetUntil2){RECT r=me2.GetRect();float x=(r.left+r.right)/2.0f,y=(r.top+r.bottom)/2.0f,dx=x-(p->rect.left+p->rect.right)/2.0f,dy=y-(p->rect.top+p->rect.bottom)/2.0f,d=dx*dx+dy*dy;if(d<best){mx=x;my=y;magnetic=true;}}
            if (!p->Show(magnetic,mx,my)) { delete p; pit=powerups.erase(pit); continue; }
            bool picked=false;
            if (!p1Dead && CheckCrash(p->GetRect(),me.GetRect())) {
                applyPower(p->type,1); picked=true;
            } else if (gameMode==2 && !p2Dead && CheckCrash(p->GetRect(),me2.GetRect())) {
                applyPower(p->type,2); picked=true;
            }
            if (picked) { delete p; pit=powerups.erase(pit); }
            else pit++;
        }

        // ---- Enemy loop ----
        auto hitEnemyWith=[&](vector<EBULLET*>& shots,ENEMY* enemy,int owner){
            for(auto bit=shots.begin();bit!=shots.end();++bit){EBULLET* bullet=*bit;
                if(bullet->active&&CheckCrash(bullet->GetRect(),enemy->GetRect())){
                    int damage=bullet->damage;bool keep=bullet->pierceLeft>0;
                    if(keep){bullet->pierceLeft--;bullet->y=(float)(enemy->GetRect().top-bullet_image.getheight()-2);bullet->rect.top=(LONG)bullet->y;bullet->rect.bottom=bullet->rect.top+bullet_image.getheight();}
                    else{bullet->active=false;delete bullet;shots.erase(bit);}
                    enemy->TakeDamage(damage);RECT hitRect=enemy->GetRect();SpawnParticles(effectParticles,(hitRect.left+hitRect.right)/2,(hitRect.top+hitRect.bottom)/2,
                        enemy->is_died?RGB(255,125,35):RGB(255,245,120),enemy->is_died?22:8,enemy->is_died?600:300);
                    if(enemy->is_died){awardKill(enemy,owner);if(!enemy->IsBoss())TryDropPowerup(powerups,enemy->GetRect(),powerArt,currentLevel,gameMode);}
                    return;
                }
            }
        };
        auto it = es.begin();
        while (it != es.end()) {
            // 死亡动画阶段：完全冻结敌机（不 Move、不 Show、不参与碰撞）
            if (inDeathAnim) { ++it; continue; }
            // Player bullets vs enemy
            if(!(*it)->is_died)hitEnemyWith(p1bs,*it,1);
            if(gameMode==2&&!(*it)->is_died)hitEnemyWith(p2bs,*it,2);

            // Enemy body vs players
            if(!(*it)->is_died&&!p1Dead&&CheckCrash((*it)->GetRect(),me.GetRect())){
                if(frameStart-hurtlast<hurttime){}
                else if(linkedShield&&frameStart>=linkShieldReadyAt){linkShieldReadyAt=frameStart+2800;SpawnParticles(effectParticles,(me.GetRect().left+me.GetRect().right)/2,(me.GetRect().top+me.GetRect().bottom)/2,RGB(80,235,255),14,450);PlayGameSound(SOUND_HIT);hurtlast=frameStart;}
                else {if(!me.hurt()){p1Dead=true;p1DeathAt=frameStart;PlayGameSound(SOUND_PLAYER_DEATH);}hurtlast=frameStart;}
            }
            if(gameMode==2&&!(*it)->is_died&&!p2Dead&&CheckCrash((*it)->GetRect(),me2.GetRect())){
                if(frameStart-hurtlast2<hurttime){}
                else if(linkedShield&&frameStart>=linkShieldReadyAt){linkShieldReadyAt=frameStart+2800;SpawnParticles(effectParticles,(me2.GetRect().left+me2.GetRect().right)/2,(me2.GetRect().top+me2.GetRect().bottom)/2,RGB(80,235,255),14,450);PlayGameSound(SOUND_HIT);hurtlast2=frameStart;}
                else {if(!me2.hurt()){p2Dead=true;p2DeathAt=frameStart;PlayGameSound(SOUND_PLAYER_DEATH);}hurtlast2=frameStart;}
            }

            if (!(*it)->Show()) { delete *it; it=es.erase(it); }
            else { (*it)->DrawFeedback();it++; }
        }

        // ---- Enemy bullets vs players ----
        for (auto eit=ebs.begin(); eit!=ebs.end(); ) {
            if (!(*eit)->active) { delete *eit; eit=ebs.erase(eit); continue; }
            bool hit=false;
            if((*eit)->IsHarmful()&&!(*eit)->IsLaser()){
                RECT br=(*eit)->GetRect();
                if(!p1Dead&&!(*eit)->grazedP1){RECT pr=me.GetRect(),grazeArea=pr;grazeArea.left-=18;grazeArea.right+=18;grazeArea.top-=18;grazeArea.bottom+=18;
                    if(CheckCrash(br,grazeArea)&&!CheckCrash(br,pr)){(*eit)->grazedP1=true;grazeCount++;int gain=(int)lroundf(5*scoreMultiplier());totalScore+=gain;p1Contribution+=gain;}}
                if(gameMode==2&&!p2Dead&&!(*eit)->grazedP2){RECT pr=me2.GetRect(),grazeArea=pr;grazeArea.left-=18;grazeArea.right+=18;grazeArea.top-=18;grazeArea.bottom+=18;
                    if(CheckCrash(br,grazeArea)&&!CheckCrash(br,pr)){(*eit)->grazedP2=true;grazeCount++;int gain=(int)lroundf(5*scoreMultiplier());totalScore+=gain;p2Contribution+=gain;}}
            }
            if ((*eit)->IsHarmful() && !p1Dead && CheckCrash((*eit)->GetRect(),me.GetRect())) {
                if (frameStart-hurtlast>=hurttime) {
                    if(linkedShield&&frameStart>=linkShieldReadyAt)linkShieldReadyAt=frameStart+2800;
                    else if(!me.hurt()){p1Dead=true;p1DeathAt=frameStart;PlayGameSound(SOUND_PLAYER_DEATH);}hurtlast=frameStart;
                }
                (*eit)->active=false; delete *eit; eit=ebs.erase(eit); hit=true;
            }
            if (!hit && (*eit)->IsHarmful() && gameMode==2 && !p2Dead && CheckCrash((*eit)->GetRect(),me2.GetRect())) {
                if (frameStart-hurtlast2>=hurttime) {
                    if(linkedShield&&frameStart>=linkShieldReadyAt)linkShieldReadyAt=frameStart+2800;
                    else if(!me2.hurt()){p2Dead=true;p2DeathAt=frameStart;PlayGameSound(SOUND_PLAYER_DEATH);}hurtlast2=frameStart;
                }
                (*eit)->active=false; delete *eit; eit=ebs.erase(eit); hit=true;
            }
            if (!hit) eit++;
        }

        // In co-op, a surviving teammate automatically revives the fallen player after holding out.
        if(gameMode==2&&p1Dead&&!p2Dead&&frameStart-p1DeathAt>=REVIVE_HOLD_MS){p1Dead=false;p1DeathAt=0;me.Revive();PlayGameSound(SOUND_PICKUP);}
        if(gameMode==2&&p2Dead&&!p1Dead&&frameStart-p2DeathAt>=REVIVE_HOLD_MS){p2Dead=false;p2DeathAt=0;me2.Revive();PlayGameSound(SOUND_PICKUP);}

        // ---- Advance after defeating the current level Boss ----
        bool bossStillPresent=false;
        for (auto e:es) if (e->IsBoss()) { bossStillPresent=true; break; }
        bool levelCleared=runMode==1&&bossActive&&!bossStillPresent;

        // ---- Timed spawn and gradual difficulty increase ----
        levelPlayedMs = levelElapsedBase+frameStart-levelStart;
        size_t maxEnemies=runMode==2?min<size_t>(18,5+currentLevel+(size_t)(levelPlayedMs/18000)):
            min<size_t>(currentLevel>=3?13:10,3+currentLevel+(size_t)(levelPlayedMs/12000));
        ULONGLONG difficultyReduction=levelPlayedMs/(runMode==2?75:90)+(min(currentLevel,3)-1)*(currentLevel>=3?260:180);
        ULONGLONG spawnInterval=difficultyReduction>=820?230:1050-difficultyReduction;
        if (!bossActive&&!levelCleared&&is_play&&es.size()<maxEnemies&&frameStart-lastSpawn>=spawnInterval && deathPhase==0) {
            SpawnEnemy(es, enemy_image,enemy_image_mask, e1boom,e1boom_mask,
                enemy2_image,enemy2_image_mask, e2boom,e2boom_mask,min(currentLevel,3),gameMode);
            lastSpawn = frameStart;
        }

        // ---- Death phase: 单人死亡动画 ----
        // 注意：deathPhase 取值 1=boom, 2=death1 动画, 21=停帧等待, 3=death2 下坠, 4=退出
        // 阶段 4 才退出，所以条件是 != 4
        if (gameMode==1 && deathPhase != 4 && deathPhase > 0) {
            // 先清空玩家、僚机、粒子（避免死亡画面叠着玩家残影）
            // 推进阶段机
            ULONGLONG now = frameStart;
            if (deathPhase == 1) {
                // 玩家爆炸动画
                ULONGLONG period = 1000 / PLAYER_BOOM_FPS;
                if (now - deathAnimFrameAt >= period) {
                    int step = (int)((now - deathAnimFrameAt) / period);
                    deathAnimFrame += step;
                    deathAnimFrameAt += step * period;
                }
                if (deathAnimFrame >= PLAYER_BOOM_FRAMES) {
                    // boom 完成 -> 进入 death1
                    deathPhase = 2;
                    deathAnimFrame = 0;
                    deathAnimFrameAt = now;
                    deathPhaseAt = now;
                }
            } else if (deathPhase == 2) {
                // death1 动画 + 2 秒等待
                ULONGLONG d1Period = 1000 / DEATH1_FPS;
                if (deathAnimFrame < DEATH1_FRAMES) {
                    if (now - deathAnimFrameAt >= d1Period) {
                        int step = (int)((now - deathAnimFrameAt) / d1Period);
                        deathAnimFrame += step;
                        deathAnimFrameAt += step * d1Period;
                    }
                    if (deathAnimFrame >= DEATH1_FRAMES) {
                        // 播完 -> 锁死在最后一帧，并切到等待模式
                        deathAnimFrame = DEATH1_FRAMES - 1;
                        deathPhaseAt = now;
                        phase21EnterAt = now;  // 21 阶段专用计时起点
                        phase21FrameCount = 0;  // 重置帧计数器
                        deathPhase = 21;  // 进入"death1 已播完、停帧等待 2 秒"
                    }
                }
            } else if (deathPhase == 21) {
                // 等待 2 秒（时间 OR 帧数，二者满足其一即切换）
                phase21FrameCount++;
                if (now - phase21EnterAt >= DEATH1_HOLD_MS || phase21FrameCount >= 240) {
                    // 初始化 death2 下坠粒子（直接往下掉）
                    death2X = (float)deathCenterX;
                    death2Y = (float)deathCenterY;
                    death2VX = ((rand() % 100) - 50) / 100.0f;  // -0.5 ~ +0.5 横向微摆
                    death2VY = 0.8f;  // 初速度向下（先慢后快）
                    death2Rot = 0.0f;
                    death2RotV = ((rand() % 100) < 50 ? -1 : 1) * (0.035f + (rand() % 50) / 1000.0f);
                    death2Frame = 0;
                    ULONGLONG d2Period = 1000 / DEATH2_FPS;
                    deathAnimFrameAt = now;
                    deathPhase = 3;
                    deathPhaseAt = now;
                    phase21EnterAt = 0;
                    phase21FrameCount = 0;
                    PlayGameSound(SOUND_DEATH_DROP);
                }
            } else if (deathPhase == 3) {
                // death2 下坠 + 旋转
                ULONGLONG d2Period = 1000 / DEATH2_FPS;
                if (now - deathAnimFrameAt >= d2Period) {
                    int step = (int)((now - deathAnimFrameAt) / d2Period);
                    death2Frame += step;
                    deathAnimFrameAt += step * d2Period;
                    if (death2Frame >= DEATH2_FRAMES) death2Frame = DEATH2_FRAMES - 1;
                }
                // 重力 / 阻力（与 deadcat 一致 ~ 1.5 秒掉出屏）
                death2VY = death2VY * 0.997f + 0.18f;
                death2VX *= 0.997f;
                death2X += death2VX;
                death2Y += death2VY;
                death2Rot += death2RotV;
                // 出屏判定
                if (death2Y > HEIGHT + 80) {
                    deathPhase = 4;
                    deathPhaseAt = now;
                }
            }
            // 渲染死亡画面（覆盖在场景之上，背景不变）
            int dx = deathCenterX - DEATH_FRAME / 2;
            int dy = deathCenterY - DEATH_FRAME / 2;
            if (deathPhase == 1) {
                putimage_mask(dx, dy, &player_boom_src[min(deathAnimFrame, PLAYER_BOOM_FRAMES - 1)], &player_boom_mask[min(deathAnimFrame, PLAYER_BOOM_FRAMES - 1)]);
            } else if (deathPhase == 2 || deathPhase == 21) {
                int fi = min(deathAnimFrame, DEATH1_FRAMES - 1);
                putimage_mask(dx, dy, &death1_src[fi], &death1_mask[fi]);
            } else if (deathPhase == 3) {
                IMAGE* src = &death2_src[min(death2Frame, DEATH2_FRAMES - 1)];
                IMAGE* msk = &death2_mask[min(death2Frame, DEATH2_FRAMES - 1)];
                int ix = (int)lroundf(death2X) - DEATH_FRAME / 2;
                int iy = (int)lroundf(death2Y) - DEATH_FRAME / 2;
                if (fabsf(death2Rot) < 0.005f) {
                    putimage_mask(ix, iy, src, msk);
                } else {
                    IMAGE rotTmp; rotTmp.Resize(src->getwidth(), src->getheight());
                    IMAGE rotTmpM; rotTmpM.Resize(msk->getwidth(), msk->getheight());
                    rotateimage(&rotTmp, src, death2Rot, BLACK, false, false);
                    rotateimage(&rotTmpM, msk, death2Rot, WHITE, false, false);
                    putimage_mask(ix, iy, &rotTmp, &rotTmpM);
                }
            }
        }

        setorigin(0,0);EndBatchDraw();

        if(levelCleared){
            for(auto e:es)delete e;es.clear();for(auto b:ebs)delete b;ebs.clear();
            PlayMusic(MUSIC_MENU);
            UpgradeSelect(bk_image,upgrade,currentLevel);me.SetUpgradeBonuses(upgrade.moveSpeed,upgrade.shieldCapacity);me2.SetUpgradeBonuses(upgrade.moveSpeed,upgrade.shieldCapacity);
            if(currentLevel>=3){gameWon=true;recordResult=true;is_play=false;}
            else{currentLevel++;bossActive=false;levelStart=GetTickCount();levelElapsedBase=0;levelScoreStart=totalScore;lastSpawn=levelStart-1200;levelBannerUntil=levelStart+2000;}
        }

        // ---- Pause ----
        ExMessage esc;
        if (peekmessage(&esc,EX_KEY) && esc.message==WM_KEYDOWN && esc.vkcode==VK_ESCAPE) {
            while(peekmessage(&esc,EX_KEY));
            MUSIC_TRACK resumeMusic=GameplayMusic(runMode,currentLevel,bossActive);
            int cmd = PauseMenu();
            if (cmd==0) PlayMusic(resumeMusic);
            else if (cmd==1) is_play=false;
            else if (cmd==2) exit(0);
            else if(cmd==3){
                SAVE_DATA s;s.name=playerName;s.mode=gameMode;s.runMode=runMode;s.score=totalScore;s.level=currentLevel;
                s.levelScoreStart=levelScoreStart;s.levelElapsedMs=levelElapsedBase+frameStart-levelStart;s.bossActive=bossActive;s.bombCount=bombCount;
                s.p1Hp=me.GetHP();s.p1Dead=p1Dead;s.p1Shield=me.HasShield();s.p1Rect=me.GetRect();
                s.p2Hp=me2.GetHP();s.p2Dead=p2Dead;s.p2Shield=me2.HasShield();s.p2Rect=me2.GetRect();
                s.p1FireMs=frameStart<firePowerUntil1?(int)(firePowerUntil1-frameStart):0;
                s.p2FireMs=frameStart<firePowerUntil2?(int)(firePowerUntil2-frameStart):0;
                s.p1DroneMs=frameStart<droneUntil1?(int)(droneUntil1-frameStart):0;
                s.p2DroneMs=frameStart<droneUntil2?(int)(droneUntil2-frameStart):0;
                s.p1ShieldCount=me.GetShieldCount();s.p2ShieldCount=me2.GetShieldCount();s.p1Kills=p1Kills;s.p2Kills=p2Kills;s.combo=comboCount;s.graze=grazeCount;s.upgrade=upgrade;
                for(auto e:es)if(e->IsBoss()&&!e->is_died){s.bossHp=e->GetHP();break;}
                WriteSave(s);is_play=false;
            }
        }

        // ---- Death check ----
        // 单人模式：进入死亡动画阶段机（boom -> death1+等待 -> death2 下坠 -> Over）
        if (gameMode==1 && p1Dead && deathPhase==0) {
            deathPhase = 1;
            deathAnimFrame = 0;
            deathAnimFrameAt = frameStart;
            deathPhaseAt = frameStart;
            RECT pr = me.GetRect();
            deathCenterX = (pr.left + pr.right) / 2;
            deathCenterY = (pr.top + pr.bottom) / 2;
            // 停止一切敌机/子弹更新：保留显示，但不再有交互
        }
        if (gameMode==2 && p1Dead && p2Dead) {is_play=false;recordResult=true;}

        // ---- Death phase complete -> 退出主循环进入 Over() ----
        if (gameMode==1 && deathPhase == 4) {
            is_play = false;
            recordResult = true;
            gameWon = false;
        }

        // ---- Cleanup player bullets ----
        auto clr = [](vector<EBULLET*>& v) {
            for (auto it=v.begin(); it!=v.end(); )
                if (!(*it)->active) { delete *it; it=v.erase(it); }
                else it++;
        };
        clr(p1bs); clr(p2bs);

        ULONGLONG frameUsed = GetTickCount() - frameStart;
        if (frameUsed < FRAME_TIME_MS)
            Sleep((DWORD)(FRAME_TIME_MS - frameUsed));
    }

    for (auto e : es) delete e; es.clear();
    for (auto b : ebs) delete b; ebs.clear();
    for (auto b : p1bs) delete b; p1bs.clear();
    for (auto b : p2bs) delete b; p2bs.clear();
    for (auto p : powerups) delete p; powerups.clear();
    if(recordResult){AddScore(playerName,totalScore,runMode);remove(SAVE_FILE);Over();}
    return true;
}

int main() {
    // EasyX uses pixel coordinates. Prevent Windows display scaling from
    // enlarging the 600x1000 game window and pushing the player off-screen.
    using SetDpiAwareFn = BOOL(WINAPI*)();
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        if (auto setDpiAware = reinterpret_cast<SetDpiAwareFn>(
                GetProcAddress(user32, "SetProcessDPIAware"))) {
            setDpiAware();
        }
    }
    timeBeginPeriod(1);
    initgraph(WIDTH, HEIGHT);
    InitGameAudio();
    // Reserve the game controls while the game is running. This prevents
    // VS Code from receiving W/A/S/D when Windows incorrectly leaves the
    // editor focused after launching the EasyX window.
    RegisterHotKey(NULL, 1001, 0, 'W');
    RegisterHotKey(NULL, 1002, 0, 'A');
    RegisterHotKey(NULL, 1003, 0, 'S');
    RegisterHotKey(NULL, 1004, 0, 'D');
    RegisterHotKey(NULL, 1005, 0, VK_UP);
    RegisterHotKey(NULL, 1006, 0, VK_DOWN);
    RegisterHotKey(NULL, 1007, 0, VK_LEFT);
    RegisterHotKey(NULL, 1008, 0, VK_RIGHT);
    RegisterHotKey(NULL, 1009, 0, VK_SPACE);
    RegisterHotKey(NULL, 1010, 0, 'B');
    srand((unsigned)time(NULL));
    bool hasPlayer=LoadLastPlayer(playerName);

    // 开场动画：仅在游戏启动时播放一次（GameOver → 主菜单不再触发）
    Intro();

    while (true) {
        int action=Welcome();
        if(action==4)break;
        if(action==3){ShowRanking();continue;}
        if(action==2){if(EnterNickname()){hasPlayer=true;SaveLastPlayer(playerName);}continue;}
        if(action==1){SAVE_DATA check;if(LoadSave(check)){playerName=check.name;hasPlayer=true;SaveLastPlayer(playerName);Play(true);}continue;}
        if(!hasPlayer){if(!EnterNickname())continue;hasPlayer=true;SaveLastPlayer(playerName);}
        runMode=SelectRunMode();
        if(runMode==0)continue;
        gameMode = screen2();
        if (gameMode == 0) continue;
        Play(false);
    }
    CloseGameAudio();closegraph();timeEndPeriod(1);
    return 0;
}


