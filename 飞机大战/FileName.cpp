#include<iostream>
#include<cstdlib>
#include<graphics.h>
#include<vector>
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
constexpr int BOMB_BOSS_DAMAGE = 10;

int gameMode = 1;
unsigned long long totalScore = 0;
ULONGLONG p1ShootClock = 0, p2ShootClock = 0;
bool spaceLast = false, num1Last = false;
bool gameWon = false;

inline void putimage_mask(int x, int y, IMAGE* src, IMAGE* mask) {
    putimage(x, y, mask, SRCAND);
    putimage(x, y, src, SRCPAINT);
}

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

class ME {
public:
    ME(IMAGE& img, IMAGE& img_m) :img(img), img_mask(img_m), HP(SUMHP), hurtTime(0) {
        rect.left = WIDTH/2 - img.getwidth()/2;
        rect.right = rect.left + img.getwidth();
        rect.top = HEIGHT - img.getheight();
        rect.bottom = HEIGHT;
    }
    RECT& GetRect() { return rect; }
    void Show() {
        for (int i=(int)pts.size()-1; i>=0; i--) {
            pts[i].x += pts[i].vx; pts[i].y += pts[i].vy;
            pts[i].life -= 16;
            if (pts[i].life <= 0) { pts.erase(pts.begin()+i); continue; }
            float a = (float)pts[i].life/pts[i].lmax;
            int sz = (int)(5*a)+1;
            setfillcolor(pts[i].col);
            solidcircle((int)pts[i].x, (int)pts[i].y, sz);
        }
        ULONGLONG now = GetTickCount();
        bool visible = hurtTime == 0 || now - hurtTime >= hurttime || (now / 100) % 2 == 0;
        if (HP > 0 && visible) putimage_mask(rect.left,rect.top,&img,&img_mask);
        if (HP > 0 && shield) {
            int cx=(rect.left+rect.right)/2, cy=(rect.top+rect.bottom)/2;
            int radius=(rect.right-rect.left)/2+12;
            setlinecolor(RGB(40,220,255)); setlinestyle(PS_SOLID,3);
            circle(cx,cy,radius); setlinestyle(PS_SOLID,1);
        }
    }
    void Control() {
        if ((GetAsyncKeyState('W') | GetAsyncKeyState(VK_UP)) & 0x8000)
            { rect.top-=PLAYER_SPEED; rect.bottom-=PLAYER_SPEED; }
        if ((GetAsyncKeyState('S') | GetAsyncKeyState(VK_DOWN)) & 0x8000)
            { rect.top+=PLAYER_SPEED; rect.bottom+=PLAYER_SPEED; }
        if ((GetAsyncKeyState('A') | GetAsyncKeyState(VK_LEFT)) & 0x8000)
            { rect.left-=PLAYER_SPEED; rect.right-=PLAYER_SPEED; }
        if ((GetAsyncKeyState('D') | GetAsyncKeyState(VK_RIGHT)) & 0x8000)
            { rect.left+=PLAYER_SPEED; rect.right+=PLAYER_SPEED; }
        Clamp();
    }
    void Control2() {
        if (GetAsyncKeyState(VK_UP)&0x8000)    { rect.top-=PLAYER_SPEED; rect.bottom-=PLAYER_SPEED; }
        if (GetAsyncKeyState(VK_DOWN)&0x8000)  { rect.top+=PLAYER_SPEED; rect.bottom+=PLAYER_SPEED; }
        if (GetAsyncKeyState(VK_LEFT)&0x8000)  { rect.left-=PLAYER_SPEED; rect.right-=PLAYER_SPEED; }
        if (GetAsyncKeyState(VK_RIGHT)&0x8000) { rect.left+=PLAYER_SPEED; rect.right+=PLAYER_SPEED; }
        Clamp();
    }
    void Clamp() {
        int w = rect.right-rect.left, h = rect.bottom-rect.top;
        if (rect.left<0) { rect.left=0; rect.right=w; }
        if (rect.right>WIDTH) { rect.right=WIDTH; rect.left=WIDTH-w; }
        if (rect.top<0) { rect.top=0; rect.bottom=h; }
        if (rect.bottom>HEIGHT) { rect.bottom=HEIGHT; rect.top=HEIGHT-h; }
    }
    bool hurt() {
        hurtTime = GetTickCount();
        if (shield) { shield=false; return true; }
        HP--;
        for (int i=0; i<15; i++) {
            Ptcl p;
            p.x = rect.left + rand()%(rect.right-rect.left);
            p.y = rect.top + rand()%(rect.bottom-rect.top);
            float ang = (rand()%360)*3.14159f/180.0f;
            float spd = (rand()%5+2)*0.6f;
            p.vx = cos(ang)*spd; p.vy = sin(ang)*spd;
            p.life = 800; p.lmax = 800;
            p.col = RGB(rand()%256, rand()%256, rand()%256);
            pts.push_back(p);
        }
        return (HP==0)?false:true;
    }
    int GetHP() { return HP; }
    void Heal() { if (HP<SUMHP) HP++; }
    void GrantShield() { shield=true; }
    bool HasShield() { return shield; }
private:
    IMAGE &img, &img_mask;
    RECT rect; unsigned int HP; ULONGLONG hurtTime; bool shield=false;
    vector<Ptcl> pts;
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
    IMAGE *img, *img_mask;
    RECT rect; float x, y, vx, vy; bool active;
    EBULLET() : img(0), img_mask(0), x(0), y(0), vx(0), vy(0), active(false) {}
    void Init(IMAGE& i, IMAGE& im, RECT pr, float dx, float dy, int spd) {
        img=&i; img_mask=&im; vx=dx*spd; vy=dy*spd;
        x=(float)(pr.left+(pr.right-pr.left)/2-i.getwidth()/2);
        y=(float)pr.bottom;
        rect.left=(LONG)x;
        rect.right = rect.left+i.getwidth();
        rect.top=(LONG)y; rect.bottom=rect.top+i.getheight();
        active = true;
    }
    void InitDown(IMAGE& i, IMAGE& im, RECT pr) { Init(i,im,pr,0,1,3); }
    bool Show() {
        if (!active) return false;
        if (rect.top>=HEIGHT||rect.bottom<0||rect.right<0||rect.left>WIDTH)
            { active=false; return false; }
        // Keep the precise position so diagonal components below one pixel do
        // not get truncated to zero every frame.
        x+=vx; y+=vy;
        rect.left=(LONG)lroundf(x); rect.right=rect.left+img->getwidth();
        rect.top=(LONG)lroundf(y); rect.bottom=rect.top+img->getheight();
        putimage_mask(rect.left, rect.top, img, img_mask);
        return true;
    }
    RECT& GetRect() { return rect; }
};

enum POWER_TYPE { POWER_FIRE, POWER_LIFE, POWER_BOMB, POWER_SHIELD };

class POWERUP {
public:
    POWER_TYPE type; IMAGE *img,*mask; RECT rect; bool active;
    POWERUP(POWER_TYPE t,IMAGE& i,IMAGE& m,RECT from):type(t),img(&i),mask(&m),active(true) {
        int x=(from.left+from.right)/2-i.getwidth()/2;
        if (x<0) x=0; if (x+i.getwidth()>WIDTH) x=WIDTH-i.getwidth();
        rect.left=x; rect.right=x+i.getwidth();
        rect.top=(from.top+from.bottom)/2-i.getheight()/2;
        rect.bottom=rect.top+i.getheight();
    }
    bool Show() {
        if (!active || rect.top>=HEIGHT) return false;
        rect.top+=1; rect.bottom+=1;
        putimage_mask(rect.left,rect.top,img,mask); return true;
    }
    RECT& GetRect() { return rect; }
};

// ============ Abstract Enemy ============
class ENEMY {
public:
    RECT rect; bool is_died; int boom_num, hp, maxHp, scoreVal, attackTimer;
    ENEMY() : is_died(false), boom_num(0), hp(1), maxHp(1), scoreVal(1), attackTimer(0) {}
    virtual ~ENEMY() {}
    virtual bool Show() = 0;
    void IsDied() { is_died=true; }
    RECT& GetRect() { return rect; }
    virtual bool CanAttack() = 0;
    int GetHP() { return hp; }
    int GetMaxHP() { return maxHp; }
    virtual bool IsBoss() { return false; }
    void TakeDamage(int damage=1) { hp-=damage; if (hp<=0) is_died=true; }
    virtual void Move() = 0;
    virtual void GetBullets(vector<EBULLET*>& ebs, IMAGE& bi, IMAGE& bm) {}
};

// ============ Type1: basic, no attack ============
class ENEMY_TYPE1 : public ENEMY {
    IMAGE *pI, *pM, *pB, *pBM; int speed;
public:
    ENEMY_TYPE1(IMAGE& i, IMAGE& im, IMAGE* b, IMAGE* bm, int level, int mode) {
        pI=&i; pM=&im; pB=b; pBM=bm;
        hp=maxHp=1+(level>=2?1:0)+(mode==2?1:0); scoreVal=10; speed=1+level;
        int x=abs(rand())%(WIDTH-i.getwidth());
        rect.left=x; rect.top=-i.getheight();
        rect.right=x+i.getwidth(); rect.bottom=0;
    }
    bool Show() override {
        if (is_died) {
            if (boom_num>=3) return false;
            putimage_mask(rect.left, rect.top, &pB[boom_num], &pBM[boom_num]);
            boom_num++; return true;
        }
        if (rect.top>=HEIGHT) return false;
        Move(); putimage_mask(rect.left,rect.top,pI,pM);
        return true;
    }
    void Move() override { rect.top+=speed; rect.bottom+=speed; }
    bool CanAttack() override { return false; }
};

// ============ Type2: fast enemy ============
class ENEMY_TYPE2 : public ENEMY {
    IMAGE *pI, *pM, *pB, *pBM; int totalF, speed, attackInterval;
public:
    ENEMY_TYPE2(IMAGE& i, IMAGE& im, IMAGE* b, IMAGE* bm, int level, int mode) {
        pI=&i; pM=&im; pB=b; pBM=bm;
        hp=maxHp=2+(level-1)/2+(mode==2?1:0); scoreVal=20; totalF=4;
        speed=2+level; attackInterval=220-level*30;
        int x=abs(rand())%(WIDTH-i.getwidth());
        rect.left=x; rect.top=-i.getheight();
        rect.right=x+i.getwidth(); rect.bottom=0;
    }
    bool Show() override {
        if (is_died) {
            if (boom_num>=totalF) return false;
            putimage_mask(rect.left, rect.top, &pB[boom_num], &pBM[boom_num]);
            boom_num++; return true;
        }
        if (rect.top>=HEIGHT) return false;
        Move(); putimage_mask(rect.left,rect.top,pI,pM);
        return true;
    }
    void Move() override { rect.top+=speed; rect.bottom+=speed; }
    bool CanAttack() override { return true; }
    void GetBullets(vector<EBULLET*>& ebs, IMAGE& bi, IMAGE& bm) override {
        attackTimer++;
        if (attackTimer>=attackInterval) { attackTimer=0;
            EBULLET* eb=new EBULLET();
            eb->InitDown(bi,bm,rect);
            ebs.push_back(eb);
        }
    }
};

// ============ Type4: side-moving enemy (introduced in level 2) ============
class ENEMY_TYPE4 : public ENEMY {
    IMAGE *pI, *pM, *pB, *pBM; int totalF, vx, vy;
public:
    ENEMY_TYPE4(IMAGE& i, IMAGE& im, IMAGE* b, IMAGE* bm, int level, int mode) {
        pI=&i; pM=&im; pB=b; pBM=bm; totalF=4;
        hp=maxHp=2+level/2+(mode==2?1:0); scoreVal=30; vx=2+level; vy=1+level;
        int x=abs(rand())%(WIDTH-i.getwidth());
        rect.left=x; rect.top=-i.getheight();
        rect.right=x+i.getwidth(); rect.bottom=0;
        if (rand()%2) vx=-vx;
    }
    bool Show() override {
        if (is_died) {
            if (boom_num>=totalF) return false;
            putimage_mask(rect.left,rect.top,&pB[boom_num],&pBM[boom_num]);
            boom_num++; return true;
        }
        if (rect.top>=HEIGHT) return false;
        Move(); putimage_mask(rect.left,rect.top,pI,pM); return true;
    }
    void Move() override {
        rect.left+=vx; rect.right+=vx;
        rect.top+=vy; rect.bottom+=vy;
        if (rect.left<=0 || rect.right>=WIDTH) {
            vx=-vx;
            if (rect.left<0) { rect.right-=rect.left; rect.left=0; }
            if (rect.right>WIDTH) { int d=rect.right-WIDTH; rect.left-=d; rect.right=WIDTH; }
        }
    }
    bool CanAttack() override { return false; }
};

// ============ Type3: boss, scatter+ring ============
class ENEMY_TYPE3 : public ENEMY {
    IMAGE *pI, *pM, *pB, *pBM; int totalF, patT, level, vx;
public:
    ENEMY_TYPE3(IMAGE& i, IMAGE& im, IMAGE* b, IMAGE* bm, int stage, int mode) {
        pI=&i; pM=&im; pB=b; pBM=bm; level=stage;
        hp=maxHp=(mode==2?45:35)+stage*(mode==2?30:20);
        scoreVal=stage*100; totalF=6; patT=0; vx=stage>=2?2+stage:0;
        rect.left=WIDTH/2-i.getwidth()/2;
        rect.right=rect.left+i.getwidth();
        rect.top=0; rect.bottom=i.getheight();
    }
    bool Show() override {
        if (is_died) {
            if (boom_num>=totalF) return false;
            putimage_mask(rect.left, rect.top, &pB[boom_num], &pBM[boom_num]);
            boom_num++; return true;
        }
        putimage_mask(rect.left,rect.top,pI,pM);
        return true;
    }
    void Move() override {
        if (vx==0) return;
        rect.left+=vx; rect.right+=vx;
        if (rect.left<=0 || rect.right>=WIDTH) vx=-vx;
    }
    bool IsBoss() override { return true; }
    bool CanAttack() override { return true; }
    void GetBullets(vector<EBULLET*>& ebs, IMAGE& bi, IMAGE& bm) override {
        patT++; Move();
        int fanInterval=360-level*45;
        if (patT%fanInterval==0) {
            int spread=20+20*level;
            for (int a=-spread; a<=spread; a+=20) {
                float rad=a*3.14159f/180.0f;
                EBULLET* eb=new EBULLET();
                eb->Init(bi,bm,rect,sin(rad),cos(rad),3);
                ebs.push_back(eb);
            }
        }
        int ringInterval=900-level*100;
        if (level>=2 && patT%ringInterval==0) {
            for (int a=0; a<360; a+=45) {
                float rad=a*3.14159f/180.0f;
                EBULLET* eb=new EBULLET();
                eb->Init(bi,bm,rect,sin(rad),cos(rad),2);
                ebs.push_back(eb);
            }
        }
    }
};

void GameStart(int mode) {
    cleardevice(); settextcolor(BLACK); settextstyle(40,0,_T("\u9ed1\u4f53"));
    if (mode==1) outtextxy(WIDTH/2-100,HEIGHT/2,_T("\u5355\u4eba\u6e38\u620f\u5f00\u59cb\uff01"));
    else outtextxy(WIDTH/2-100,HEIGHT/2,_T("\u53cc\u4eba\u6e38\u620f\u5f00\u59cb\uff01"));
    outtextxy(WIDTH/2-160,HEIGHT/2+60,_T("\u6309\u4efb\u610f\u952e\u8fd4\u56de\u4e3b\u83dc\u5355"));
    ExMessage m; while (true) { if (peekmessage(&m,EX_KEY|EX_MOUSE)) return; }
}

int screen2() {
    IMAGE bk; loadimage(&bk,_T("./images/bk2.png"),WIDTH,HEIGHT);
    LPCTSTR s1=_T("\u5355\u4eba\u6e38\u620f"),s2=_T("\u53cc\u4eba\u6e38\u620f"),sb=_T("\u8fd4\u56de");
    RECT r1,r2,rb;
    while (true) {
        BeginBatchDraw(); putimage(0,0,&bk); setbkmode(TRANSPARENT); settextcolor(BLACK);
        settextstyle(50,0,_T("\u9ed1\u4f53")); outtextxy(WIDTH/2-textwidth(_T("\u9009\u62e9\u6a21\u5f0f"))/2,HEIGHT/10,_T("\u9009\u62e9\u6a21\u5f0f"));
        settextstyle(36,0,_T("\u9ed1\u4f53"));
        r1.left=WIDTH/2-textwidth(s1)/2; r1.right=r1.left+textwidth(s1);
        r1.top=HEIGHT/3; r1.bottom=r1.top+textheight(s1);
        outtextxy(r1.left,r1.top,s1);
        r2.left=WIDTH/2-textwidth(s2)/2; r2.right=r2.left+textwidth(s2);
        r2.top=HEIGHT/3+100; r2.bottom=r2.top+textheight(s2);
        outtextxy(r2.left,r2.top,s2);
        settextstyle(28,0,_T("\u9ed1\u4f53"));
        rb.left=WIDTH/2-textwidth(sb)/2; rb.right=rb.left+textwidth(sb);
        rb.top=HEIGHT/3+220; rb.bottom=rb.top+textheight(sb);
        outtextxy(rb.left,rb.top,sb);
        EndBatchDraw();
        ExMessage m; getmessage(&m,EX_MOUSE);
        if (m.lbutton) {
            if (MessInPoint(m.x,m.y,r1)) { GameStart(1); return 1; }
            if (MessInPoint(m.x,m.y,r2)) { GameStart(2); return 2; }
            if (MessInPoint(m.x,m.y,rb)) return 0;
        }
    }
}

void Welcome() {
    IMAGE bk; loadimage(&bk,_T("./images/bk2.png"),WIDTH,HEIGHT);
    LPCTSTR t=_T("\u98de\u673a\u5927\u6218"),p=_T("\u5f00\u59cb\u6e38\u620f"),e=_T("\u9000\u51fa\u6e38\u620f");
    RECT rp,re;
    BeginBatchDraw(); putimage(0,0,&bk); setbkmode(TRANSPARENT); settextcolor(BLACK);
    settextstyle(60,0,_T("\u9ed1\u4f53")); outtextxy(WIDTH/2-textwidth(t)/2,HEIGHT/10,t);
    settextstyle(40,0,_T("\u9ed1\u4f53"));
    rp.left=WIDTH/2-textwidth(p)/2; rp.right=rp.left+textwidth(p);
    rp.top=HEIGHT/4; rp.bottom=rp.top+textheight(p);
    re.left=WIDTH/2-textwidth(e)/2; re.right=re.left+textwidth(e);
    re.top=HEIGHT/4*1.5; re.bottom=re.top+textheight(e);
    outtextxy(rp.left,rp.top,p); outtextxy(re.left,re.top,e);
    EndBatchDraw();
    while (true) {
        ExMessage m; getmessage(&m,EX_MOUSE);
        if (m.lbutton) {
            if (MessInPoint(m.x,m.y,rp)) return;
            if (MessInPoint(m.x,m.y,re)) exit(0);
        }
    }
}

void Over() {
    IMAGE obk; loadimage(&obk,_T("./images/bk2.png"),WIDTH,HEIGHT);
    cleardevice(); putimage(0,0,&obk);
    TCHAR s[128];
    _stprintf_s(s,128,_T("\u603b\u5f97\u5206:%llu"),totalScore);
    setbkmode(TRANSPARENT); settextcolor(gameWon?GREEN:RED); settextstyle(44,0,_T("\u9ed1\u4f53"));
    LPCTSTR result=gameWon?_T("\u4e09\u5173\u901a\u5173\uff01"):_T("\u6e38\u620f\u7ed3\u675f");
    outtextxy(WIDTH/2-textwidth(result)/2,HEIGHT/10,result);
    settextstyle(36,0,_T("\u9ed1\u4f53"));
    outtextxy(WIDTH/2-textwidth(s)/2,HEIGHT/10+60,s);
    LPCTSTR info=_T("\u6309Enter\u952e\u8fd4\u56de\u4e3b\u83dc\u5355");
    settextcolor(BLACK); settextstyle(24,0,_T("\u9ed1\u4f53"));
    outtextxy(WIDTH/2-textwidth(info)/2,HEIGHT/10+115,info);
    while (true) { ExMessage m; getmessage(&m,EX_KEY); if (m.vkcode==0x0D) return; }
}

int PauseMenu() {
    LPCTSTR tc=_T("  \u7ee7\u7eed\u6e38\u620f  "),tm=_T("  \u8fd4\u56de\u83dc\u5355  "),te=_T("  \u9000\u51fa\u6e38\u620f  ");
    RECT cr,mr,er;
    settextstyle(28,0,_T("\u9ed1\u4f53"));
    cr.left=WIDTH/2-textwidth(tc)/2-20; cr.right=cr.left+textwidth(tc)+40;
    cr.top=HEIGHT/2-40; cr.bottom=cr.top+45;
    mr.left=WIDTH/2-textwidth(tm)/2-20; mr.right=mr.left+textwidth(tm)+40;
    mr.top=cr.bottom+15; mr.bottom=mr.top+45;
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
        if ((m.message==WM_KEYDOWN&&m.vkcode==49)||(m.lbutton&&MessInPoint(m.x,m.y,mr))) return 1;
        if ((m.message==WM_KEYDOWN&&m.vkcode==50)||(m.lbutton&&MessInPoint(m.x,m.y,er))) { exit(0); return 2; }
        Sleep(30);
    }
}
void SpawnEnemy(vector<ENEMY*>& es, IMAGE& e1i, IMAGE& e1m, IMAGE* e1b, IMAGE* e1bm,
    IMAGE& e2i, IMAGE& e2m, IMAGE* e2b, IMAGE* e2bm, int level, int mode) {
    int r = rand() % 100;
    if (level==1) {
        if (r<70) es.push_back(new ENEMY_TYPE1(e1i,e1m,e1b,e1bm,level,mode));
        else es.push_back(new ENEMY_TYPE2(e2i,e2m,e2b,e2bm,level,mode));
    } else if (level==2) {
        if (r<40) es.push_back(new ENEMY_TYPE1(e1i,e1m,e1b,e1bm,level,mode));
        else if (r<75) es.push_back(new ENEMY_TYPE2(e2i,e2m,e2b,e2bm,level,mode));
        else es.push_back(new ENEMY_TYPE4(e2i,e2m,e2b,e2bm,level,mode));
    } else {
        if (r<25) es.push_back(new ENEMY_TYPE1(e1i,e1m,e1b,e1bm,level,mode));
        else if (r<60) es.push_back(new ENEMY_TYPE2(e2i,e2m,e2b,e2bm,level,mode));
        else es.push_back(new ENEMY_TYPE4(e2i,e2m,e2b,e2bm,level,mode));
    }
}

void TryDropPowerup(vector<POWERUP*>& items, RECT where,
    IMAGE& fireI,IMAGE& fireM,IMAGE& lifeI,IMAGE& lifeM,
    IMAGE& bombI,IMAGE& bombM,IMAGE& shieldI,IMAGE& shieldM,
    int level,int mode,bool guaranteed=false) {
    // Later levels offer fewer resources; two-player drops are shared.
    int dropChance=(mode==2?16:12)-(level-1)*2;
    if (!guaranteed && rand()%100>=dropChance) return;
    int roll=rand()%100;
    int t=roll<45?POWER_FIRE:roll<60?POWER_LIFE:roll<75?POWER_BOMB:POWER_SHIELD;
    if (t==POWER_FIRE) items.push_back(new POWERUP(POWER_FIRE,fireI,fireM,where));
    else if (t==POWER_LIFE) items.push_back(new POWERUP(POWER_LIFE,lifeI,lifeM,where));
    else if (t==POWER_BOMB) items.push_back(new POWERUP(POWER_BOMB,bombI,bombM,where));
    else items.push_back(new POWERUP(POWER_SHIELD,shieldI,shieldM,where));
}

bool Play() {
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
    totalScore = 0;
    gameWon = false;
    bool is_play = true, p1Dead = false, p2Dead = false;
    int bsing = 0;

    // ===== Load images =====
    IMAGE me_image, me_image_mask, p2_image, p2_image_mask;
    IMAGE enemy_image, enemy_image_mask;
    IMAGE enemy2_image, enemy2_image_mask;
    IMAGE enemy3_image, enemy3_image_mask;
    IMAGE bullet_image, bullet_image_mask;
    IMAGE fire_icon,fire_mask,life_icon,life_mask,bomb_icon,bomb_mask,shield_icon,shield_mask;
    IMAGE bk_image;

    // Player
    loadimage(&me_image, _T("./images/me1_src.png"));
    loadimage(&me_image_mask, _T("./images/me1_mask.png"));
    // P2 (tinted)
    loadimage(&p2_image, _T("./images/me1_src.png"));
    loadimage(&p2_image_mask, _T("./images/me1_mask.png"));
    {
        DWORD* buf = GetImageBuffer(&p2_image);
        DWORD* mb = GetImageBuffer(&p2_image_mask);
        int w = p2_image.getwidth(), h = p2_image.getheight();
        for (int i = 0; i < w*h; i++) {
            if (mb[i] != 0) {
                BYTE r = (buf[i]>>16)&0xFF, g = (buf[i]>>8)&0xFF, b = buf[i]&0xFF;
                r = min(255, r*255/128); g = min(255, g*160/128); b = b*40/128;
                buf[i] = RGB(r,g,b);
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

    // Bullet
    loadimage(&bullet_image, _T("./images/bullet1_src.png"));
    loadimage(&bullet_image_mask, _T("./images/bullet1_mask.png"));

    // Power-up icons
    load_power_icon(fire_icon,fire_mask,_T("./images/bullet_supply.png"));
    load_power_icon(life_icon,life_mask,_T("./images/life.png"));
    load_power_icon(bomb_icon,bomb_mask,_T("./images/bomb_supply.png"));
    load_power_icon(shield_icon,shield_mask,_T("./images/shield_supply.png"));

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
    ME me(me_image, me_image_mask);
    ME me2(p2_image, p2_image_mask);
    me2.GetRect().left = WIDTH/4 - me_image.getwidth()/2;
    me2.GetRect().right = me2.GetRect().left + me_image.getwidth();
    me2.GetRect().bottom = HEIGHT;
    me2.GetRect().top = HEIGHT - me_image.getheight();

    vector<ENEMY*> es;
    vector<EBULLET*> ebs, p1bs, p2bs;
    vector<POWERUP*> powerups;

    ULONGLONG hurtlast = 0, hurtlast2 = 0;
    ULONGLONG gameStart = GetTickCount();
    ULONGLONG levelStart = gameStart;
    ULONGLONG lastSpawn = gameStart - 1200;
    ULONGLONG levelBannerUntil = gameStart + 2000;
    unsigned long long levelScoreStart = 0;
    int currentLevel = 1;
    bool bossActive = false;
    ULONGLONG firePowerUntil1=0,firePowerUntil2=0;
    int bombCount=0;
    bool bombLast=false;

    while (is_play) {
        ULONGLONG frameStart = GetTickCount();
        bsing++;

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

        auto firePlayerBullets = [&](vector<EBULLET*>& shots,RECT plane,bool powered) {
            float dirs[3]={-0.32f,0.0f,0.32f};
            int count=powered?3:1;
            int start=powered?0:1;
            for (int i=start;i<start+count;i++) {
                EBULLET* eb=new EBULLET();
                eb->Init(bullet_image,bullet_image_mask,plane,dirs[i],-1,5);
                int bh=bullet_image.getheight();
                eb->rect.top=plane.top-bh; eb->rect.bottom=plane.top;
                eb->y=(float)eb->rect.top;
                shots.push_back(eb);
            }
        };

        // ---- P1 shoot ----
        if (!p1Dead) {
            bool sNow = (GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
            if (sNow && frameStart-p1ShootClock>220) {
                firePlayerBullets(p1bs,me.GetRect(),frameStart<firePowerUntil1);
                p1ShootClock = frameStart;
            }
            spaceLast = sNow;
        }

        // ---- P2 shoot ----
        if (gameMode==2 && !p2Dead) {
            bool n1Now = (GetAsyncKeyState(0x61)&0x8000)!=0;
            if (n1Now && frameStart-p2ShootClock>220) {
                firePlayerBullets(p2bs,me2.GetRect(),frameStart<firePowerUntil2);
                p2ShootClock = frameStart;
            }
            num1Last = n1Now;
        }

        // ---- Bomb: clears regular enemies but only damages a Boss ----
        bool bombNow=(GetAsyncKeyState('B')&0x8000)!=0;
        if (bombNow && !bombLast && bombCount>0) {
            bombCount--;
            for (auto e:es) if (!e->is_died) {
                if (e->IsBoss()) {
                    e->TakeDamage(BOMB_BOSS_DAMAGE);
                    if (e->is_died) totalScore+=e->scoreVal;
                } else {
                    totalScore+=e->scoreVal; e->IsDied();
                }
            }
            for (auto b:ebs) delete b; ebs.clear();
        }
        bombLast=bombNow;

        // ---- End-of-level Boss trigger ----
        ULONGLONG levelPlayedMs = frameStart-levelStart;
        unsigned long long levelTargetScore = 260ULL + currentLevel*120ULL;
        if (!bossActive && (levelPlayedMs>=45000 || totalScore-levelScoreStart>=levelTargetScore)) {
            for (auto e:es) delete e; es.clear();
            for (auto b:ebs) delete b; ebs.clear();
            es.push_back(new ENEMY_TYPE3(enemy3_image,enemy3_image_mask,e3boom,e3boom_mask,currentLevel,gameMode));
            bossActive=true;
            levelBannerUntil=frameStart+2000;
        }

        // ---- Enemy bullets (each enemy owns its own attack timer) ----
        for (auto enemy:es)
            if (enemy->CanAttack() && !enemy->is_died && enemy->GetRect().top>=0)
                enemy->GetBullets(ebs, bullet_image, bullet_image_mask);

        // ---- Render ----
        BeginBatchDraw(); cleardevice(); bk.Show();

        if (!p1Dead) { me.Control(); me.Show(); }
        if (gameMode==2 && !p2Dead) { me2.Control2(); me2.Show(); }

        // ---- UI ----
        setbkmode(TRANSPARENT);
        if (gameMode==1) {
            TCHAR inf[64];
            _stprintf_s(inf,64,_T("Score:%llu"),totalScore);
            settextcolor(RED); settextstyle(24,0,_T("\u9ed1\u4f53")); outtextxy(10,10,inf);
            _stprintf_s(inf,64,_T("HP:%d"),me.GetHP()); outtextxy(10,38,inf);
        } else {
            TCHAR inf[64];
            _stprintf_s(inf,64,_T("Score:%llu"),totalScore);
            settextcolor(RED); settextstyle(24,0,_T("\u9ed1\u4f53")); outtextxy(10,10,inf);
            _stprintf_s(inf,64,_T("P1 HP:%d"),me.GetHP()); outtextxy(10,38,inf);
            _stprintf_s(inf,64,_T("P2 HP:%d"),me2.GetHP());
            outtextxy(WIDTH-textwidth(inf)-15,15,inf);
        }

        TCHAR levelText[32];
        _stprintf_s(levelText,32,_T("LEVEL %d / 3"),currentLevel);
        settextcolor(RGB(30,80,180)); settextstyle(22,0,_T("\u9ed1\u4f53"));
        outtextxy(WIDTH/2-textwidth(levelText)/2,12,levelText);

        int fireSeconds1=frameStart<firePowerUntil1?(int)((firePowerUntil1-frameStart+999)/1000):0;
        TCHAR itemText[64];
        _stprintf_s(itemText,64,_T("BOMB(B):%d  FIRE:%ds"),bombCount,fireSeconds1);
        settextcolor(RGB(30,110,190)); settextstyle(19,0,_T("\u9ed1\u4f53"));
        outtextxy(10,66,itemText);

        ENEMY* bossForUi=nullptr;
        for (auto e:es) if (e->IsBoss() && !e->is_died) { bossForUi=e; break; }
        if (bossForUi) {
            const int bx=120, by=100, bw=360, bh=20;
            setfillcolor(RGB(55,55,65)); solidrectangle(bx,by,bx+bw,by+bh);
            int hp=bossForUi->GetHP(); if (hp<0) hp=0;
            int fill=bw*hp/bossForUi->GetMaxHP();
            setfillcolor(RGB(220,45,45)); solidrectangle(bx,by,bx+fill,by+bh);
            setlinecolor(BLACK); rectangle(bx,by,bx+bw,by+bh);
            TCHAR bossText[32]; _stprintf_s(bossText,32,_T("BOSS  %d / %d"),hp,bossForUi->GetMaxHP());
            settextcolor(BLACK); settextstyle(18,0,_T("\u9ed1\u4f53"));
            outtextxy(WIDTH/2-textwidth(bossText)/2,by+23,bossText);
        }

        if (frameStart<levelBannerUntil) {
            TCHAR banner[48];
            if (bossActive) _stprintf_s(banner,48,_T("LEVEL %d  BOSS"),currentLevel);
            else _stprintf_s(banner,48,_T("LEVEL %d  START"),currentLevel);
            settextcolor(RGB(200,45,45)); settextstyle(42,0,_T("\u9ed1\u4f53"));
            outtextxy(WIDTH/2-textwidth(banner)/2,HEIGHT/2-50,banner);
        }

        for (auto& b : p1bs) b->Show();
        if (gameMode==2) for (auto& b : p2bs) b->Show();
        for (auto& b : ebs) b->Show();

        auto applyPower = [&](POWER_TYPE type,ME& player,ULONGLONG& fireUntil) {
            if (type==POWER_FIRE) fireUntil=frameStart+FIRE_POWER_DURATION_MS;
            else if (type==POWER_LIFE) player.Heal();
            else if (type==POWER_BOMB) bombCount++;
            else if (type==POWER_SHIELD) player.GrantShield();
        };
        for (auto pit=powerups.begin();pit!=powerups.end();) {
            POWERUP* p=*pit;
            if (!p->Show()) { delete p; pit=powerups.erase(pit); continue; }
            bool picked=false;
            if (!p1Dead && CheckCrash(p->GetRect(),me.GetRect())) {
                applyPower(p->type,me,firePowerUntil1); picked=true;
            } else if (gameMode==2 && !p2Dead && CheckCrash(p->GetRect(),me2.GetRect())) {
                applyPower(p->type,me2,firePowerUntil2); picked=true;
            }
            if (picked) { delete p; pit=powerups.erase(pit); }
            else pit++;
        }

        // ---- Enemy loop ----
        auto it = es.begin();
        while (it != es.end()) {
            // Player bullets vs enemy
            for (auto bit=p1bs.begin(); bit!=p1bs.end(); ) {
                if ((*bit)->active && CheckCrash((*bit)->GetRect(), (*it)->GetRect())) {
                    (*bit)->active=false; delete *bit; bit=p1bs.erase(bit);
                    (*it)->TakeDamage();
                    if ((*it)->is_died) {
                        totalScore+=(*it)->scoreVal;
                        TryDropPowerup(powerups,(*it)->GetRect(),fire_icon,fire_mask,life_icon,life_mask,
                            bomb_icon,bomb_mask,shield_icon,shield_mask,currentLevel,gameMode,(*it)->IsBoss());
                        (*it)->IsDied();
                    }
                    break;
                } else bit++;
            }
            if (gameMode==2) {
                for (auto bit=p2bs.begin(); bit!=p2bs.end(); ) {
                    if ((*bit)->active && CheckCrash((*bit)->GetRect(), (*it)->GetRect())) {
                        (*bit)->active=false; delete *bit; bit=p2bs.erase(bit);
                        if (!(*it)->is_died) {
                            (*it)->TakeDamage();
                            if ((*it)->is_died) {
                                totalScore+=(*it)->scoreVal;
                                TryDropPowerup(powerups,(*it)->GetRect(),fire_icon,fire_mask,life_icon,life_mask,
                                    bomb_icon,bomb_mask,shield_icon,shield_mask,currentLevel,gameMode,(*it)->IsBoss());
                                (*it)->IsDied();
                            }
                        }
                        break;
                    } else bit++;
                }
            }

            // Enemy body vs players
            if (!(*it)->is_died && !p1Dead && CheckCrash((*it)->GetRect(),me.GetRect()))
                if (frameStart-hurtlast>=hurttime) { if (!me.hurt()) p1Dead=true; hurtlast=frameStart; }
            if (gameMode==2 && !(*it)->is_died && !p2Dead && CheckCrash((*it)->GetRect(),me2.GetRect()))
                if (frameStart-hurtlast2>=hurttime) { if (!me2.hurt()) p2Dead=true; hurtlast2=frameStart; }

            if (!(*it)->Show()) { delete *it; it=es.erase(it); }
            else it++;
        }

        // ---- Enemy bullets vs players ----
        for (auto eit=ebs.begin(); eit!=ebs.end(); ) {
            if (!(*eit)->active) { delete *eit; eit=ebs.erase(eit); continue; }
            bool hit=false;
            if (!p1Dead && CheckCrash((*eit)->GetRect(),me.GetRect())) {
                if (frameStart-hurtlast>=hurttime) { if (!me.hurt()) p1Dead=true; hurtlast=frameStart; }
                (*eit)->active=false; delete *eit; eit=ebs.erase(eit); hit=true;
            }
            if (!hit && gameMode==2 && !p2Dead && CheckCrash((*eit)->GetRect(),me2.GetRect())) {
                if (frameStart-hurtlast2>=hurttime) { if (!me2.hurt()) p2Dead=true; hurtlast2=frameStart; }
                (*eit)->active=false; delete *eit; eit=ebs.erase(eit); hit=true;
            }
            if (!hit) eit++;
        }

        // ---- Advance after defeating the current level Boss ----
        bool bossStillPresent=false;
        for (auto e:es) if (e->IsBoss()) { bossStillPresent=true; break; }
        if (bossActive && !bossStillPresent) {
            if (currentLevel>=3) {
                gameWon=true;
                is_play=false;
            } else {
                currentLevel++;
                bossActive=false;
                levelStart=frameStart;
                levelScoreStart=totalScore;
                lastSpawn=frameStart-1200;
                levelBannerUntil=frameStart+2000;
            }
        }

        // ---- Timed spawn and gradual difficulty increase ----
        levelPlayedMs = frameStart-levelStart;
        size_t maxEnemies = 3 + currentLevel + (size_t)(levelPlayedMs/12000);
        if (maxEnemies > 10) maxEnemies = 10;
        ULONGLONG difficultyReduction = levelPlayedMs/90 + (currentLevel-1)*180;
        ULONGLONG spawnInterval = difficultyReduction>=750 ? 300 : 1050-difficultyReduction;
        if (!bossActive && is_play && es.size()<maxEnemies && frameStart-lastSpawn>=spawnInterval) {
            SpawnEnemy(es, enemy_image,enemy_image_mask, e1boom,e1boom_mask,
                enemy2_image,enemy2_image_mask, e2boom,e2boom_mask,currentLevel,gameMode);
            lastSpawn = frameStart;
        }

        EndBatchDraw();

        // ---- Pause ----
        ExMessage esc;
        if (peekmessage(&esc,EX_KEY) && esc.message==WM_KEYDOWN && esc.vkcode==VK_ESCAPE) {
            while(peekmessage(&esc,EX_KEY));
            int cmd = PauseMenu();
            if (cmd==1) is_play=false; else if (cmd==2) exit(0);
        }

        // ---- Death check ----
        if (gameMode==1 && p1Dead) is_play=false;
        if (gameMode==2 && p1Dead && p2Dead) is_play=false;

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
    for (auto p : powerups) delete p; powerups.clear();
    Over();
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
    while (true) {
        Welcome();
        gameMode = screen2();
        if (gameMode == 0) continue;
        Play();
    }
    return 0;
}


