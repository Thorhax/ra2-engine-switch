#include "switch_text.hpp"

#include <SDL_ttf.h>

#include <sys/stat.h>

#include <cstdio>
#include <map>
#include <string>
#include <tuple>

#include "switch_platform.hpp"

namespace {

struct Fonts {
    bool tried = false, ok = false;
    std::map<std::pair<int, bool>, TTF_Font*> sized;   // (px, bold)
};
Fonts g_fonts;

struct Entry {
    SDL_Texture* tex = nullptr;
    int w = 0, h = 0;
    unsigned long long used = 0;
};
using Key = std::tuple<std::string, int, bool, Uint32>;   // text, px, bold, rgb
std::map<Key, Entry> g_cache;
unsigned long long g_clock = 0;
constexpr std::size_t kCacheMax = 600;   // strings; the oldest half go when full

bool init() {
    if (g_fonts.tried) return g_fonts.ok;
    g_fonts.tried = true;
    struct stat st;
    if (stat(RA2_SWITCH_BASE_DIR "classic_font", &st) == 0) {
        std::fprintf(stderr, "[text] classic_font present: the game's bitmap fonts\n");
        return false;
    }
    if (TTF_WasInit() == 0 && TTF_Init() != 0) {
        std::fprintf(stderr, "[text] TTF_Init failed: %s\n", TTF_GetError());
        return false;
    }
    TTF_Font* probe = TTF_OpenFont("romfs:/fonts/DejaVuSans.ttf", 16);
    if (!probe) {
        std::fprintf(stderr, "[text] romfs:/fonts/DejaVuSans.ttf: %s\n", TTF_GetError());
        return false;
    }
    g_fonts.sized[{16, false}] = probe;
    g_fonts.ok = true;
    std::fprintf(stderr, "[text] DejaVu Sans for menu / HUD text\n");
    return true;
}

TTF_Font* font(int px, bool bold) {
    if (!init()) return nullptr;
    if (px < 6) px = 6;
    auto it = g_fonts.sized.find({px, bold});
    if (it != g_fonts.sized.end()) return it->second;
    TTF_Font* f = TTF_OpenFont(bold ? "romfs:/fonts/DejaVuSans-Bold.ttf" : "romfs:/fonts/DejaVuSans.ttf", px);
    if (f) TTF_SetFontHinting(f, TTF_HINTING_LIGHT);
    g_fonts.sized[{px, bold}] = f;
    return f;
}

const Entry* render(SDL_Renderer* ren, const std::string& s, int px, bool bold, SDL_Color col) {
    const Uint32 rgb = (Uint32(col.r) << 16) | (Uint32(col.g) << 8) | col.b;
    const Key key{s, px, bold, rgb};
    auto it = g_cache.find(key);
    if (it != g_cache.end()) {
        it->second.used = ++g_clock;
        return &it->second;
    }
    TTF_Font* f = font(px, bold);
    if (!f) return nullptr;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(f, s.c_str(), SDL_Color{col.r, col.g, col.b, 255});
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, surf);
    Entry e;
    e.tex = tex;
    e.w = surf->w;
    e.h = surf->h;
    e.used = ++g_clock;
    SDL_FreeSurface(surf);
    if (!tex) return nullptr;
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    if (g_cache.size() >= kCacheMax) {   // drop the least recently used half
        unsigned long long cut = g_clock - kCacheMax / 2;
        for (auto i = g_cache.begin(); i != g_cache.end();) {
            if (i->second.used < cut) {
                SDL_DestroyTexture(i->second.tex);
                i = g_cache.erase(i);
            } else {
                ++i;
            }
        }
    }
    return &g_cache.emplace(key, e).first->second;
}

}  // namespace

bool sw_text_enabled() { return init(); }

int sw_text_width(const std::string& s, int px, bool bold) {
    TTF_Font* f = font(px, bold);
    if (!f || s.empty()) return 0;
    int w = 0, h = 0;
    if (TTF_SizeUTF8(f, s.c_str(), &w, &h) != 0) return 0;
    return w;
}

int sw_text_line_h(int px, bool bold) {
    TTF_Font* f = font(px, bold);
    return f ? TTF_FontHeight(f) : px;
}

int sw_text_fit(const std::string& s, int px, int max_w, int min_px, bool bold) {
    while (px > min_px && sw_text_width(s, px, bold) > max_w) --px;
    return px;
}

void sw_text_draw(SDL_Renderer* ren, const std::string& s, int x, int y, int px, SDL_Color col, bool shadow,
                  bool bold) {
    if (s.empty()) return;
    const Entry* e = render(ren, s, px, bold, col);
    if (!e || !e->tex) return;
    if (shadow) {
        SDL_SetTextureColorMod(e->tex, 0, 0, 0);
        SDL_SetTextureAlphaMod(e->tex, 220);
        const SDL_Rect sd{x + 1, y + 1, e->w, e->h};
        SDL_RenderCopy(ren, e->tex, nullptr, &sd);
        SDL_SetTextureColorMod(e->tex, 255, 255, 255);
    }
    SDL_SetTextureAlphaMod(e->tex, col.a);
    const SDL_Rect d{x, y, e->w, e->h};
    SDL_RenderCopy(ren, e->tex, nullptr, &d);
    SDL_SetTextureAlphaMod(e->tex, 255);
}

void sw_text_box(SDL_Renderer* ren, const std::string& s, const SDL_Rect& box, int align, int px, SDL_Color col,
                 bool shadow, bool bold) {
    if (s.empty()) return;
    px = sw_text_fit(s, px, box.w - 2, 10, bold);
    const int w = sw_text_width(s, px, bold), h = sw_text_line_h(px, bold);
    const int x = align == 1 ? box.x + (box.w - w) / 2 : align == 2 ? box.x + box.w - w : box.x;
    sw_text_draw(ren, s, x, box.y + (box.h - h) / 2, px, col, shadow, bold);
}
