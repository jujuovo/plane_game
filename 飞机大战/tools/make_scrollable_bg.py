"""
生成一张可无缝纵向循环滚动的背景图：
- 宽度 = 600 (WIDTH)
- 高度 = 2000 (HEIGHT * 2)
- 颜色基调继承 bk2.png：顶部深蓝，底部浅蓝
- 关键：顶部一行像素的 RGB 与底部一行像素的 RGB 一致 → 可循环
- 加稀疏白色星星（程序生成），每颗星星在 (x, y) 和 (x, y+H) 都画 → 无缝

输出：bk_scroll.png
"""
from PIL import Image
import random
import math

W, H = 600, 2000

# 取 bk2.png 的顶部 / 底部颜色，作为渐变基准
src = Image.open(r'd:/wolf/learning/小学期程序设计小组作业/6/plane_game-main/飞机大战/images/bk2.png').convert('RGB')
W2, H2 = src.size  # (600, 2000)

# 在 bk2.png 顶部(1/4)、底部(3/4) 各取一些样本平均，得到渐变端点
def avg_color(im, y0, y1):
    px = im.load()
    rs, gs, bs, n = 0, 0, 0, 0
    for y in range(y0, y1):
        for x in range(0, W, 8):
            r, g, b = px[x, y]
            rs += r; gs += g; bs += b; n += 1
    return rs // n, gs // n, bs // n

# 上端色：顶部 10%
top_col = avg_color(src, 0, max(1, H2 // 10))
# 下端色：底部 10%
bot_col = avg_color(src, int(H2 * 0.9), H2)
print(f"top={top_col} bot={bot_col}")

# 现在生成新图：
# 让它在 y=0 和 y=H 处的颜色相同（无缝条件）。
# 用一个 *周期* 函数：在 y 方向用 sin / 周期性插值。
# 因为 H = 2*HEIGHT = 2000, 我们希望 0~2000 的循环周期是 HEIGHT = 1000：
# 即 y 位置的颜色取决于 y mod HEIGHT，这样 [0,HEIGHT] 与 [HEIGHT, 2HEIGHT] 完全一致。
# 但 H=2000, 周期=1000。所以我们要存 H/2=1000 高度的"tile"，然后上下重复。

# 选 (top+bot)/2 为中间色，让周期 tile 内从 top_col 到 bot_col 来回一次（实际用 cos 即可）
# 让 tile (高度=H/2=1000) 内部 y=0..1000：
#   phase = y / 1000.0  (0..1)
#   用 cos 让 y=0 和 y=1000 都是 top_col（首尾闭合）
#   col(y) = lerp(top_col, bot_col, 0.5 - 0.5*cos(2*pi*phase))
# 在 y=0: cos(0)=1, t=0, col=top
# 在 y=1000: cos(2pi)=1, t=0, col=top
# 在 y=500: cos(pi)=-1, t=1, col=bot
# 这样 tile 内从 top 渐到 bot 再渐回 top，拼接时 y=1000 与下一 tile 的 y=0 都是 top，无缝。

# 但 bk2 本身不是渐变回 top，是从 top 单调到 bot。
# 想单调：让 tile 内是 y=0 -> top, y=1000 -> bot，且两 tile 衔接处颜色一致。
# 这要求 tile 是从 top 到 bot 单调，然后 bot 紧接着 top。需要"渐变回头"：top → bot → top

# 实际上简单做法：tile 高度=1000, tile 内 y=0→1000：col 从 top 渐到 bot。
# 然后我们让最终图像高度为 2*HEIGHT=2000，且 1000 像素高为 tile 的两次拼接：
#   0..1000: top -> bot
#   1000..2000: bot -> top   (即颜色从 bot 平滑回到 top)
# 拼接时 1000 行 (bot) 紧接着 1001 行 (top)，不连续。
# 解决：让 tile 高度=HEIGHT=1000, tile 内部做 top → bot → top 的往返。
# 这样拼接时 y=1000 (top) 紧接 y=1001 (top) → 无缝。

TILE = H // 2  # 1000
img = Image.new('RGB', (W, H), (0, 0, 0))
px = img.load()
for y in range(H):
    ymod = y % TILE            # 0..999
    phase = ymod / TILE        # 0..1
    # 用 0.5 - 0.5*cos(2pi*phase) 让 y=0,y=TILE 都 = 0 (top), 中间 y=TILE/2 = 1 (bot)
    t = 0.5 - 0.5 * math.cos(2 * math.pi * phase)
    r = int(top_col[0] + (bot_col[0] - top_col[0]) * t)
    g = int(top_col[1] + (bot_col[1] - top_col[1]) * t)
    b = int(top_col[2] + (bot_col[2] - top_col[2]) * t)
    for x in range(W):
        px[x, y] = (r, g, b)

# 加稀疏白色星星。重要：每颗星在 (x, y) 和 (x, y+TILE) 都画 → 保证可循环。
random.seed(42)
star_count = 90
for _ in range(star_count):
    x = random.randint(2, W - 3)
    y = random.randint(2, TILE - 3)
    # 随机亮度、大小
    bright = random.randint(140, 255)
    radius = random.choice([0, 0, 0, 1, 1, 2])  # 大部分 1 像素
    if radius == 0:
        px[x, y] = (bright, bright, bright)
        if y + TILE < H:
            px[x, y + TILE] = (bright, bright, bright)
    elif radius == 1:
        # 3x3 中心十字
        for dx, dy in [(0, 0), (-1, 0), (1, 0), (0, -1), (0, 1)]:
            nx, ny = x + dx, y + dy
            if 0 <= nx < W and 0 <= ny < H:
                v = bright if (dx, dy) == (0, 0) else max(80, bright // 2)
                px[nx, ny] = (v, v, v)
                if ny + TILE < H:
                    px[nx, ny + TILE] = (v, v, v)
    else:
        # 5x5 模糊星
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                d2 = dx * dx + dy * dy
                if d2 > 5: continue
                v = bright if d2 == 0 else max(60, bright // (1 + d2))
                nx, ny = x + dx, y + dy
                if 0 <= nx < W and 0 <= ny < H:
                    px[nx, ny] = (v, v, v)
                    if ny + TILE < H:
                        px[nx, ny + TILE] = (v, v, v)

# 加少量"星云/光斑"（柔和大色块）—— 同样对称画
for _ in range(8):
    cx = random.randint(60, W - 60)
    cy = random.randint(60, TILE - 60)
    r = random.randint(40, 90)
    cr = random.randint(30, 80)
    cg = random.randint(40, 100)
    cb = random.randint(100, 180)
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            d = math.sqrt(dx * dx + dy * dy)
            if d > r: continue
            falloff = (1.0 - d / r) ** 2
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < W and 0 <= ny < H:
                pr, pg, pb = px[nx, ny]
                px[nx, ny] = (
                    min(255, int(pr + cr * falloff * 0.3)),
                    min(255, int(pg + cg * falloff * 0.3)),
                    min(255, int(pb + cb * falloff * 0.3)),
                )
                if ny + TILE < H:
                    pr, pg, pb = px[nx, ny + TILE]
                    px[nx, ny + TILE] = (
                        min(255, int(pr + cr * falloff * 0.3)),
                        min(255, int(pg + cg * falloff * 0.3)),
                        min(255, int(pb + cb * falloff * 0.3)),
                    )

# 保存
out = r'd:/wolf/learning/小学期程序设计小组作业/6/plane_game-main/飞机大战/images/bk_scroll.png'
img.save(out)
print(f"saved: {out}, size={img.size}")

# 自检：y=0 行像素 == y=TILE 行像素
ok = True
for x in range(W):
    if px[x, 0] != px[x, TILE]:
        ok = False
        print(f"mismatch at x={x}: {px[x, 0]} vs {px[x, TILE]}")
        break
print(f"seamless check y=0 vs y=TILE: {'OK' if ok else 'FAIL'}")