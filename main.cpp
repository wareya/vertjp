#include <stdio.h>
#include <math.h>
#include <locale.h>
#ifndef M_PI
#define M_PI 3.1415926535897932
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/stb_image_write.h"

#include "include/ft2build.h"
#include "include/freetype/freetype.h"
#include "include/hb/hb-ot.h"

#include "include/unishim_split.h"

#include <vector>
#include <map>
#include <string>
#include <algorithm>

#ifndef macro_max
#define macro_max(X,Y) (((X)>(Y))?(X):(Y))
#endif

#ifndef macro_min
#define macro_min(X,Y) (((X)<(Y))?(X):(Y))
#endif

#include "renderer.cpp"

bool fontinitialized = false;
FT_Face fontface;
FT_Library freetype;
hb_blob_t * hbblob = nullptr;
hb_font_t * hbfont = nullptr;
hb_face_t * hbface = nullptr;
uint8_t * fontbuffer = nullptr;

bool origin_hack = false;

auto fontname = "NotoSansCJKjp-Regular.otf";

#define FONTSIZE 48
#define MODE 1 // 0: horizontal ltr; 1: vertical ttb
#define BASELINE_HACK 0.385

#include "orientation.cpp"

void init_font()
{
    auto error = FT_Init_FreeType(&freetype);
    if(error)
    {
        puts("failed to initialize freetype");
        return;
    }
    auto fontfile = fopen(fontname, "rb");
    if(!fontfile)
    {
        puts("failed to open font file");
        return;
    }
    
    fseek(fontfile, 0, SEEK_END);
    uint64_t fontsize = ftell(fontfile);
    fseek(fontfile, 0, SEEK_SET);
    
    fontbuffer = (uint8_t*)malloc(fontsize);
    if(!fontbuffer)
    {
        puts("could not allocate font data");
        fclose(fontfile);
        return;
    }
    if(fread(fontbuffer, 1, fontsize, fontfile) != fontsize)
    {
        puts("failed to read font file");
        fclose(fontfile);
        return;
    }
    fclose(fontfile);
    
    error = FT_New_Memory_Face(freetype, fontbuffer, fontsize, 0, &fontface);
    if(error)
    {
        puts("Something happened initializing the font");
        return;
    }
    
    error = FT_Set_Pixel_Sizes(fontface, 0, FONTSIZE);
    if(error)
    {
        puts("Something happened setting the font size");
        return;
    }
    
    error = FT_Select_Charmap(fontface, FT_ENCODING_UNICODE);
    if(error)
    {
        puts("Something happened setting the font character map (font probably doesn't have a unicode mapping)");
        return;
    }
    
    // we have to do this ourselves instead of using hb-ft because of https://github.com/harfbuzz/harfbuzz/issues/1595
    
    hbblob = hb_blob_create((char*)fontbuffer, fontsize, HB_MEMORY_MODE_READONLY, NULL, NULL);
    hbface = hb_face_create(hbblob, 0);
    hbfont = hb_font_create(hbface);
    
    hb_ot_font_set_funcs(hbfont);
    hb_font_set_scale(hbfont, FONTSIZE*64.0, FONTSIZE*64.0);
    
    hbface = hb_font_get_face(hbfont);
    
    init_orientations();
    
    fontinitialized = true;
}

template<typename T>
void swap(T & a, T & b)
{
    T t = a;
    a = b;
    b = t;
}
template<typename T>
void rotate(T & a, T & b)
{
    swap(a, b);
    b = -b;
}

struct glyph
{
    sprite * image = nullptr;
    int w, h, x, y;
    uint64_t index = 0;
    glyph(const uint32_t & glyphindex, int mode)
    {
        w = 0;
        h = 0;
        x = 0;
        y = 0;
        
        if(!fontinitialized)
            return;
        
        auto error = FT_Load_Glyph(fontface, glyphindex, FT_LOAD_RENDER|((mode == 1) ? FT_LOAD_VERTICAL_LAYOUT : 0));
        if(error)
            return;
        
        // hb_glyph_info_t.codepoint is actually the glyph index once hb_shape has been run
        index = glyphindex;
        
        const auto & bitmap = fontface->glyph->bitmap;
        w = bitmap.width;
        h = bitmap.rows;
        x = fontface->glyph->bitmap_left;
        y = fontface->glyph->bitmap_top;
        
        if(bitmap.buffer && bitmap.pixel_mode == FT_PIXEL_MODE_GRAY)
        {
            if(mode == 2)
            {
                image = rotated_sprite_from_mono(bitmap.buffer, w, h);
                
                rotate(x, y);
                swap(w, h);
                x -= w;
                x -= FONTSIZE*BASELINE_HACK; // stupid hack because I don't want to attempt to guess baselines
            }
            else
                image = sprite_from_mono(bitmap.buffer, w, h);
        }
    }
    ~glyph()
    {
        if(image != nullptr)
            delete image;
    }
};

struct posdata {
    float x, y, x2, y2, x_advance, y_advance;
    posdata(float x_origin, float y_origin, const hb_glyph_info_t & info, const hb_glyph_position_t & pos, const glyph & glyph, int mode) // 0: horizontal, 1: vertical, 2: rotated
    {
        float x_offset = pos.x_offset/64.0;
        float y_offset = pos.y_offset/64.0;
        float x_advance = pos.x_advance/64.0;
        float y_advance = pos.y_advance/64.0;
        
        if(mode == 2)
        {
            rotate(x_offset, y_offset);
            rotate(x_advance, y_advance);
        }
        
        x =  glyph.x +  x_offset;
        y = -glyph.y + -y_offset;
        x2 = x + glyph.w;
        y2 = y + glyph.h;
        this->x_advance =  x_advance;
        this->y_advance = -y_advance;
    }
};

typedef std::map<hb_codepoint_t, glyph*> glyphmap;

glyphmap cache;

struct textrun {
    std::vector<uint32_t> text;
    bool rotated;
};

struct subtitle {
    int initialized = false;
    
    std::vector<hb_codepoint_t> glyphs;
    std::vector<posdata> positions;
    
    int minx, miny, maxx, maxy;
    int mode;
    
    subtitle()
    {
        
    }
    
    subtitle(std::string text, float size, int mode = 0) // 0: LTR; 1: TTB
    {
        if(!fontinitialized) return;
        
        this->mode = mode;
        
        std::vector<textrun> runs {{{}, false}};
        
        if(mode == 1)
        {
            utf8_iterate((uint8_t *)text.data(), 0, [](uint32_t codepoint, UNISHIM_PUN_TYPE * userdata) -> int
            {
                auto & runs = *(std::vector<textrun> *)userdata;
                auto rotate = requires_rotation(codepoint);
                if(rotate != runs.back().rotated)
                {
                    if(runs.back().text.size() == 0)
                        runs.pop_back();
                    runs.push_back({{codepoint}, rotate});
                }
                else
                    runs.back().text.push_back(codepoint);
                return 0;
            }, &runs);
        }
        else
        {
            utf8_iterate((uint8_t *)text.data(), 0, [](uint32_t codepoint, UNISHIM_PUN_TYPE * userdata) -> int
            {
                auto & runs = *(std::vector<textrun> *)userdata;
                runs.back().text.push_back(codepoint);
                return 0;
            }, &runs);
        }
        
        float x = 0;
        float y = 0;
        
        minx = 0;
        miny = 0;
        maxx = -1000000;
        maxy = -1000000;
        
        for(const auto & run : runs)
        {
            auto buffer = hb_buffer_create();
            
            hb_buffer_add_utf32(buffer, run.text.data(), run.text.size(), 0, run.text.size());
            
            auto realmode = (run.rotated)?(2):(mode);
            
            if(realmode == 1)
                hb_buffer_set_direction(buffer, HB_DIRECTION_TTB);
            else
                hb_buffer_set_direction(buffer, HB_DIRECTION_LTR);
            
            hb_buffer_set_script(buffer, hb_script_from_string("Jpan", -1));
            hb_buffer_set_language(buffer, hb_language_from_string("ja", -1));
            
            /*
            hb_feature_t features[] = {
                { HB_TAG('v','e','r','t'), 1, 0, std::numeric_limits<unsigned int>::max() },
                { HB_TAG('v','r','t','2'), 1, 0, std::numeric_limits<unsigned int>::max() },
                { HB_TAG('v','k','r','n'), 1, 0, std::numeric_limits<unsigned int>::max() },
                { HB_TAG('v','p','a','l'), 1, 0, std::numeric_limits<unsigned int>::max() },
            };
            */
            
            hb_shape(hbfont, buffer, NULL, 0);
            unsigned int glyph_count;
            hb_glyph_info_t *     glyph_info = hb_buffer_get_glyph_infos    (buffer, &glyph_count);
            hb_glyph_position_t * glyph_pos  = hb_buffer_get_glyph_positions(buffer, &glyph_count);
            
            float run_x = x;
            float run_y = y;
            
            for(unsigned int i = 0; i < glyph_count; ++i)
            {
                auto & hb_pos = glyph_pos[i];
                auto & hb_info = glyph_info[i];
                auto & glyph_id = hb_info.codepoint;
                
                if(!cache.count(glyph_id))
                    cache[glyph_id] = new glyph(glyph_id, realmode);
                glyphs.push_back(glyph_id);
                positions.push_back(posdata(run_x, run_y, hb_info, hb_pos, *cache[glyph_id], realmode));
                
                auto & pos = positions.back();
                
                minx = macro_min(minx, floor(x + pos.x));
                miny = macro_min(miny, floor(y + pos.y));
                
                maxx = macro_max(maxx,  ceil(x + pos.x2));
                maxy = macro_max(maxy,  ceil(y + pos.y2));
                
                x += pos.x_advance;
                y += pos.y_advance;
                
                maxx = macro_max(maxx, x);
                maxy = macro_max(maxy, y);
            }
            
            hb_buffer_destroy(buffer);
        }
        initialized = true;
    }
};


int main(int argc, char ** argv)
{
    init_font();
    auto mysub = subtitle("【テストｔｅｓｔ１２３test123】ー―～〰", FONTSIZE, MODE);
    
    int width  = mysub.maxx - mysub.minx;
    int height = mysub.maxy - mysub.miny;
    
    unsigned char * buffer = (unsigned char *)malloc(width*height*4);
    sprite image(buffer, width, height);
    
    image.clear();
    
    if(mysub.initialized and fontinitialized)
    {
        int x = -mysub.minx;
        int y = -mysub.miny;
        
        for(unsigned int i = 0; i < mysub.glyphs.size(); i++)
        {
            auto index = mysub.glyphs[i];
            const auto & glyph = cache[index];
            const auto & pos = mysub.positions[i];
            
            int posx = round(x + pos.x);
            int posy = round(y + pos.y);
            if(glyph->image)
                image.draw(posx, posy, glyph->image);
            
            x += pos.x_advance;
            y += pos.y_advance;
        }
        
        auto f = fopen("temp.png", "wb");
        if(f)
        {
            stbi_write_png_to_func([](void * file, void * data, int size){
                fwrite(data, 1, size, (FILE *) file);
            }, f, width, height, 4, image.buffer, width*4);
            fclose(f);
        }
    }
    
    return 0;
}
