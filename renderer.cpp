struct pixel {
    unsigned char r, g, b, a;
    pixel()
    {
        r = 0;
        g = 0;
        b = 0;
        a = 0;
    }
    pixel(const unsigned char r, const unsigned char g, const unsigned char b, const unsigned char a)
    {
        this->r = r;
        this->g = g;
        this->b = b;
        this->a = a;
    }
    void blend_over_self(const pixel other)
    {
        float alpha = other.a/255.0;
        float ret_r = (1-alpha)*r*a/255.0 + alpha*other.r*other.a/255.0;
        float ret_g = (1-alpha)*g*a/255.0 + alpha*other.g*other.a/255.0;
        float ret_b = (1-alpha)*b*a/255.0 + alpha*other.b*other.a/255.0;
        float ret_a = (1-alpha)*a   + other.a;
        if(ret_a != 0)
        {
            ret_r /= ret_a/255.0;
            ret_g /= ret_a/255.0;
            ret_b /= ret_a/255.0;
        }
        r = round(ret_r);
        g = round(ret_g);
        b = round(ret_b);
        a = round(ret_a);
    }
};
void ensure_ordered(float & a, float & b)
{
    if(a > b)
    {
        float t = b;
        b = a;
        a = t;
    }
}
struct sprite {
    pixel * buffer = nullptr;
    int w, h;
    sprite(unsigned char * buffer, const int w, const int h)
    {
        this->w = w;
        this->h = h;
        this->buffer = (pixel *)buffer;
    }
    pixel read(const int x, const int y) const
    {
        if(x < 0 or y < 0 or x >= w or y >= h) return pixel({0,0,0,0});
        return buffer[y*w + x];
    }
    void mix(const int x, const int y, const pixel c)
    {
        if(x < 0 or y < 0 or x >= w or y >= h) return;
        buffer[y*w + x].blend_over_self(c);
    }
    void set(const int x, const int y, const pixel c)
    {
        if(x < 0 or y < 0 or x >= w or y >= h) return;
        buffer[y*w + x] = c;
    }
    void clear(const pixel c)
    {
        for(int i = 0; i < w*h; i++)
            buffer[i] = c;
    }
    void clear()
    {
        clear({0,0,0,255});
    }
    void draw(const int base_x, const int base_y, const sprite * other, const bool domix = true)
    {
        int less_x = base_x;
        int more_x = base_x + other->w;
        int less_y = base_y;
        int more_y = base_y + other->h;
        
        int start_x = macro_max(less_x, 0);
        int final_x = macro_min(more_x, w-1);
        int start_y = macro_max(less_y, 0);
        int final_y = macro_min(more_y, h-1);
        
        if(final_x < start_x) final_x = start_x;
        if(final_y < start_y) final_y = start_y;
        
        for(int y = start_y; y <= final_y; y++)
        {
            for(int x = start_x; x <= final_x; x++)
            {
                auto color = other->read(x - base_x, y - base_y);
                mix(x, y, color);
            }
        }
    }
    void draw_rect(float x1, float y1, float x2, float y2, bool aliased = false)
    {
        ensure_ordered(x1, x2);
        ensure_ordered(y1, y2);
        x2 -= 1;
        y2 -= 1;
        ensure_ordered(x1, x2);
        ensure_ordered(y1, y2);
        if(x1 == x2 or y1 == y2) return;
        
        float start_x = floor(x1);
        float final_x = ceil (x2);
        
        float start_y = floor(y1);
        float final_y = ceil (y2);
        
        for(int y = start_y; y <= final_y; y++)
        {
            set(start_x, y, pixel({255,0,0,255}));
            set(final_x, y, pixel({255,0,0,255}));
        }
        for(int x = start_x; x <= final_x; x++)
        {
            set(x, start_y, pixel({255,0,0,255}));
            set(x, final_y, pixel({255,0,0,255}));
        }
    }
};

#define TEXT_COLOR_RED 255
#define TEXT_COLOR_GREEN 255
#define TEXT_COLOR_BLUE 255
sprite * sprite_from_mono(unsigned char * buffer, const int w, const int h)
{
    unsigned char * newbuff = (unsigned char *)malloc(w*h*4);
    auto image = new sprite(newbuff, w, h);
    
    for(int y = 0; y <= h; y++)
    {
        for(int x = 0; x <= w; x++)
        {
            auto color = pixel(TEXT_COLOR_RED, TEXT_COLOR_GREEN, TEXT_COLOR_BLUE, buffer[y*w + x]);
            image->set(x, y, color);
        }
    }
    
    return image;
}
sprite * rotated_sprite_from_mono(unsigned char * buffer, const int w, const int h)
{
    unsigned char * newbuff = (unsigned char *)malloc(w*h*4);
    auto image = new sprite(newbuff, h, w);
    
    for(int y = 0; y <= h; y++)
    {
        for(int x = 0; x <= w; x++)
        {
            auto color = pixel(TEXT_COLOR_RED, TEXT_COLOR_GREEN, TEXT_COLOR_BLUE, buffer[y*w + x]);
            image->set(h-1-y, x, color);
        }
    }
    
    return image;
}
