#include<iostream>
#include<cstdlib>
#include<graphics.h>
#include<vector>
#include<windows.h>
#include<ctime>
#define NOMINMAX
#include<math.h>

using namespace std;

constexpr auto WIDTH = 600;
constexpr auto HEIGHT = 1000;
constexpr unsigned SUMHP = 20;
constexpr int hurttime = 1000;

int gameMode = 1;
unsigned long long totalScore = 0;
clock_t p1ShootClock = 0, p2ShootClock = 0;
bool spaceLast = false, num1Last = false;

inline void putimage_mask(int x, int y, IMAGE* src, IMAGE* mask) {
    putimage(x, y, mask, SRCAND);
    putimage(x, y, src, SRCPAINT);
}

class BK {
public:
    BK(IMAGE& img) :img(img), y(-HEIGHT) {}
    void Show() {
        if (y == 0) y = -HEIGHT;
        y += 4; putimage(0, y, &img);
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
        if (HP > 0) putimage_mask(rect.left,rect.top,&img,&img_mask);
    }
    void Control() {
        if (GetAsyncKeyState('W')&0x8000) { rect.top-=6; rect.bottom-=6; }
        if (GetAsyncKeyState('S')&0x8000) { rect.top+=6; rect.bottom+=6; }
        if (GetAsyncKeyState('A')&0x8000) { rect.left-=6; rect.right-=6; }
        if (GetAsyncKeyState('D')&0x8000) { rect.left+=6; rect.right+=6; }
        Clamp();
    }
    void Control2() {
        if (GetAsyncKeyState(VK_UP)&0x8000)    { rect.top-=6; rect.bottom-=6; }
        if (GetAsyncKeyState(VK_DOWN)&0x8000)  { rect.top+=6; rect.bottom+=6; }
        if (GetAsyncKeyState(VK_LEFT)&0x8000)  { rect.left-=6; rect.right-=6; }
        if (GetAsyncKeyState(VK_RIGHT)&0x8000) { rect.left+=6; rect.right+=6; }
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
        HP--; hurtTime = clock();
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
private:
    IMAGE &img, &img_mask;
    RECT rect; unsigned int HP; clock_t hurtTime;
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
    RECT rect; float vx, vy; bool active;
    EBULLET() : img(0), img_mask(0), active(false), vx(0), vy(0) {}
    void Init(IMAGE& i, IMAGE& im, RECT pr, float dx, float dy, int spd) {
        img=&i; img_mask=&im; vx=dx*spd; vy=dy*spd;
        rect.left = pr.left+(pr.right-pr.left)/2 - i.getwidth()/2;
        rect.right = rect.left+i.getwidth();
        rect.top = pr.bottom; rect.bottom = rect.top+i.getheight();
        active = true;
    }
    void InitDown(IMAGE& i, IMAGE& im, RECT pr) { Init(i,im,pr,0,1,3); }
    bool Show() {
        if (!active) return false;
        if (rect.top>=HEIGHT||rect.bottom<0||rect.right<0||rect.left>WIDTH)
            { active=false; return false; }
        rect.left+=(int)vx; rect.right+=(int)vx;
        rect.top+=(int)vy; rect.bottom+=(int)vy;
        putimage_mask(rect.left, rect.top, img, img_mask);
        return true;
    }
    RECT& GetRect() { return rect; }
};

// ============ Abstract Enemy ============
class ENEMY {
public:
    RECT rect; bool is_died; int boom_num, hp, scoreVal;
    ENEMY() : is_died(false), boom_num(0), hp(1), scoreVal(1) {}
    virtual ~ENEMY() {}
    virtual bool Show() = 0;
    void IsDied() { is_died=true; }
    RECT& GetRect() { return rect; }
    virtual bool CanAttack() = 0;
    int GetHP() { return hp; }
    void TakeDamage() { hp--; if (hp<=0) is_died=true; }
    virtual void Move() = 0;
    virtual void GetBullets(vector<EBULLET*>& ebs, IMAGE& bi, IMAGE& bm, int& timer) {}
};

// ============ Type1: basic, no attack ============
class ENEMY_TYPE1 : public ENEMY {
    IMAGE *pI, *pM, *pB, *pBM;
public:
    ENEMY_TYPE1(IMAGE& i, IMAGE& im, IMAGE* b, IMAGE* bm) {
        pI=&i; pM=&im; pB=b; pBM=bm; hp=1; scoreVal=1;
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
    void Move() override { rect.top+=2; rect.bottom+=2; }
    bool CanAttack() override { return false; }
};

// ============ Type2: can attack, slow ============
class ENEMY_TYPE2 : public ENEMY {
    IMAGE *pI, *pM, *pB, *pBM; int totalF;
public:
    ENEMY_TYPE2(IMAGE& i, IMAGE& im, IMAGE* b, IMAGE* bm) {
        pI=&i; pM=&im; pB=b; pBM=bm; hp=10; scoreVal=5; totalF=4;
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
    void Move() override { rect.top+=1; rect.bottom+=1; }
    bool CanAttack() override { return true; }
    void GetBullets(vector<EBULLET*>& ebs, IMAGE& bi, IMAGE& bm, int& timer) override {
        timer++;
        if (timer>=120) { timer=0;
            EBULLET* eb=new EBULLET();
            eb->InitDown(bi,bm,rect);
            ebs.push_back(eb);
        }
    }
};

// ============ Type3: boss, scatter+ring ============
class ENEMY_TYPE3 : public ENEMY {
    IMAGE *pI, *pM, *pB, *pBM; int totalF, patT;
public:
    ENEMY_TYPE3(IMAGE& i, IMAGE& im, IMAGE* b, IMAGE* bm) {
        pI=&i; pM=&im; pB=b; pBM=bm; hp=30; scoreVal=20; totalF=6; patT=0;
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
    void Move() override {}
    bool CanAttack() override { return true; }
    void GetBullets(vector<EBULLET*>& ebs, IMAGE& bi, IMAGE& bm, int& t) override {
        patT++;
        if (patT>=60) { patT=0;
            for (int a=-40; a<=40; a+=20) {
                float rad=a*3.14159f/180.0f;
                EBULLET* eb=new EBULLET();
                eb->Init(bi,bm,rect,sin(rad),cos(rad),5);
                ebs.push_back(eb);
            }
        }
        if (patT>0 && patT%30==0) {
            for (int a=0; a<360; a+=45) {
                float rad=a*3.14159f/180.0f;
                EBULLET* eb=new EBULLET();
                eb->Init(bi,bm,rect,sin(rad),cos(rad),3);
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
    setbkmode(TRANSPARENT); settextcolor(RED); settextstyle(40,0,_T("\u9ed1\u4f53"));
    outtextxy(WIDTH/2-textwidth(s)/2,HEIGHT/10,s);
    LPCTSTR info=_T("\u6309Enter\u952e\u8fd4\u56de\u4e3b\u83dc\u5355");
    settextcolor(BLACK); settextstyle(24,0,_T("\u9ed1\u4f53"));
    outtextxy(WIDTH/2-textwidth(info)/2,HEIGHT/10+60,info);
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
    IMAGE& e2i, IMAGE& e2m, IMAGE* e2b, IMAGE* e2bm,
    IMAGE& e3i, IMAGE& e3m, IMAGE* e3b, IMAGE* e3bm, int& type3Count) {
    
    int r = rand() % 100;
    if (r < 60) {
        es.push_back(new ENEMY_TYPE1(e1i, e1m, e1b, e1bm));
    } else if (r < 90) {
        es.push_back(new ENEMY_TYPE2(e2i, e2m, e2b, e2bm));
    } else if (type3Count == 0) {
        type3Count++;
        es.push_back(new ENEMY_TYPE3(e3i, e3m, e3b, e3bm));
    } else {
        es.push_back(new ENEMY_TYPE1(e1i, e1m, e1b, e1bm));
    }
}

bool Play() {
    setbkcolor(WHITE); cleardevice();
    totalScore = 0;
    bool is_play = true, p1Dead = false, p2Dead = false;
    int bsing = 0;

    // ===== Load images =====
    IMAGE me_image, me_image_mask, p2_image, p2_image_mask;
    IMAGE enemy_image, enemy_image_mask;
    IMAGE enemy2_image, enemy2_image_mask;
    IMAGE enemy3_image, enemy3_image_mask;
    IMAGE bullet_image, bullet_image_mask;
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
    vector<int> enemyTimers;

    clock_t hurtlast = clock(), hurtlast2 = clock();
    int type3Count = 0;

    while (is_play) {
        bsing++;

        // ---- P1 shoot ----
        if (!p1Dead) {
            bool sNow = (GetAsyncKeyState(VK_SPACE)&0x8000)!=0;
            if (sNow && !spaceLast && clock()-p1ShootClock>150) {
                EBULLET* eb = new EBULLET();
                eb->Init(bullet_image, bullet_image_mask, me.GetRect(), 0, -1, 8);
                int bh = bullet_image.getheight();
                eb->rect.top = me.GetRect().top - bh;
                eb->rect.bottom = me.GetRect().top;
                p1bs.push_back(eb); p1ShootClock = clock();
            }
            spaceLast = sNow;
        }

        // ---- P2 shoot ----
        if (gameMode==2 && !p2Dead) {
            bool n1Now = (GetAsyncKeyState(0x61)&0x8000)!=0;
            if (n1Now && !num1Last && clock()-p2ShootClock>150) {
                EBULLET* eb = new EBULLET();
                eb->Init(bullet_image, bullet_image_mask, me2.GetRect(), 0, -1, 8);
                int bh = bullet_image.getheight();
                eb->rect.top = me2.GetRect().top - bh;
                eb->rect.bottom = me2.GetRect().top;
                p2bs.push_back(eb); p2ShootClock = clock();
            }
            num1Last = n1Now;
        }

        // ---- Enemy bullets (per type) ----
        while (enemyTimers.size() < es.size()) enemyTimers.push_back(0);
        for (size_t ei=0; ei<es.size(); ei++)
            if (es[ei]->CanAttack() && !es[ei]->is_died)
                es[ei]->GetBullets(ebs, bullet_image, bullet_image_mask, enemyTimers[ei]);

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

        for (auto& b : p1bs) b->Show();
        if (gameMode==2) for (auto& b : p2bs) b->Show();
        for (auto& b : ebs) b->Show();

        // ---- Enemy loop ----
        auto it = es.begin();
        while (it != es.end()) {
            // Player bullets vs enemy
            for (auto bit=p1bs.begin(); bit!=p1bs.end(); ) {
                if ((*bit)->active && CheckCrash((*bit)->GetRect(), (*it)->GetRect())) {
                    (*bit)->active=false; delete *bit; bit=p1bs.erase(bit);
                    (*it)->TakeDamage();
                    if ((*it)->is_died) { totalScore+=(*it)->scoreVal; (*it)->IsDied(); }
                    break;
                } else bit++;
            }
            if (gameMode==2) {
                for (auto bit=p2bs.begin(); bit!=p2bs.end(); ) {
                    if ((*bit)->active && CheckCrash((*bit)->GetRect(), (*it)->GetRect())) {
                        (*bit)->active=false; delete *bit; bit=p2bs.erase(bit);
                        if (!(*it)->is_died) {
                            (*it)->TakeDamage();
                            if ((*it)->is_died) { totalScore+=(*it)->scoreVal; (*it)->IsDied(); }
                        }
                        break;
                    } else bit++;
                }
            }

            // Enemy body vs players
            if (!(*it)->is_died && !p1Dead && CheckCrash((*it)->GetRect(),me.GetRect()))
                if (clock()-hurtlast>=hurttime) { if (!me.hurt()) p1Dead=true; hurtlast=clock(); }
            if (gameMode==2 && !(*it)->is_died && !p2Dead && CheckCrash((*it)->GetRect(),me2.GetRect()))
                if (clock()-hurtlast2>=hurttime) { if (!me2.hurt()) p2Dead=true; hurtlast2=clock(); }

            if (!(*it)->Show()) { delete *it; it=es.erase(it); }
            else it++;
        }

        // ---- Enemy bullets vs players ----
        for (auto eit=ebs.begin(); eit!=ebs.end(); ) {
            if (!(*eit)->active) { delete *eit; eit=ebs.erase(eit); continue; }
            bool hit=false;
            if (!p1Dead && CheckCrash((*eit)->GetRect(),me.GetRect())) {
                if (clock()-hurtlast>=hurttime) { if (!me.hurt()) p1Dead=true; hurtlast=clock(); }
                (*eit)->active=false; delete *eit; eit=ebs.erase(eit); hit=true;
            }
            if (!hit && gameMode==2 && !p2Dead && CheckCrash((*eit)->GetRect(),me2.GetRect())) {
                if (clock()-hurtlast2>=hurttime) { if (!me2.hurt()) p2Dead=true; hurtlast2=clock(); }
                (*eit)->active=false; delete *eit; eit=ebs.erase(eit); hit=true;
            }
            if (!hit) eit++;
        }

        // ---- Spawn ----
        while (es.size() < 5)
            SpawnEnemy(es, enemy_image,enemy_image_mask, e1boom,e1boom_mask,
                enemy2_image,enemy2_image_mask, e2boom,e2boom_mask,
                enemy3_image,enemy3_image_mask, e3boom,e3boom_mask, type3Count);

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

        Sleep(4);
    }

    for (auto e : es) delete e; es.clear();
    for (auto b : ebs) delete b; ebs.clear();
    Over();
    return true;
}

int main() {
    initgraph(WIDTH, HEIGHT, EX_SHOWCONSOLE);
    srand((unsigned)time(NULL));
    while (true) {
        Welcome();
        gameMode = screen2();
        if (gameMode == 0) continue;
        Play();
    }
    return 0;
}


