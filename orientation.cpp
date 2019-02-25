#include <vector>
#include <map>
#include <string>

struct rule_range {
    uint32_t first;
    uint32_t final;
    int8_t orientation;
};

std::vector<rule_range> ranges;
std::map<uint32_t, int8_t> singles;

std::vector<std::string> read_lines(FILE * f)
{
    std::vector<std::string> ret{std::string("")};
    
    while(!ferror(f) and !feof(f))
    {
        int c = fgetc(f);
        if(c >= 0 and c < 0x100 and c != '\r' and c != '\n')
            ret.back() += c;
        if(c == '\n')
            ret.push_back(std::string(""));
        if(c < 0 or c >= 0x100)
            break;
    }
    return ret;
}

uint32_t from_hex(std::string hexstring)
{
    return std::stoul(hexstring.data(), nullptr, 16);
}
int8_t parse_mode(std::string text)
{
    if(text == "U") return 0;
    if(text == "R") return 1;
    if(text == "Tu") return 2;
    if(text == "Tr") return 3;
    return -1;
}

void parse_lines(std::vector<std::string> & lines)
{
    for(const auto & line : lines)
    {
        if(line.size() == 0 or line.data()[0] == '#')
            continue;
        auto split_loc = line.find(" ; ");
        if(split_loc == std::string::npos)
            continue;
        auto range_loc = line.find("..");
        if(range_loc == std::string::npos)
        {
            auto codepoint = line.substr(0, split_loc);
            auto mode = line.substr(split_loc+3, -1);
            singles[from_hex(codepoint)] = parse_mode(mode);
        }
        else
        {
            auto first = line.substr(0, range_loc);
            auto final = line.substr(range_loc+2, split_loc - (range_loc+2));
            auto mode = line.substr(split_loc+3, -1);
            ranges.push_back({from_hex(first), from_hex(final), parse_mode(mode)});
        }
    }
}

hb_set_t * vert_set = nullptr;

void init_orientations()
{
    auto data = fopen("VerticalOrientation-17.txt", "rb");
    auto lines = read_lines(data);
    parse_lines(lines);
    fclose(data);
    
    vert_set = hb_set_create();
    hb_set_t * vert_lookups = hb_set_create();
    
    hb_tag_t vert[2] = {
        HB_TAG('v', 'e', 'r', 't'),
        HB_TAG_NONE,
    };
    hb_ot_layout_collect_lookups(hbface, HB_TAG('G', 'S', 'U', 'B'), nullptr, nullptr, vert, vert_lookups);
    hb_codepoint_t index;
    while(hb_set_next(vert_lookups, &index))
        hb_ot_layout_lookup_collect_glyphs(hbface, HB_TAG('G', 'S', 'U', 'B'), index, nullptr, vert_set, nullptr, nullptr);
    
    hb_set_destroy(vert_lookups);
}

int8_t get_orientation(uint32_t codepoint)
{
    if(singles.count(codepoint) == 1)
        return singles[codepoint];
    for(const auto & range : ranges)
    {
        if(codepoint >= range.first and codepoint <= range.final)
            return range.orientation;
    }
    return 1; // default to R
}

bool requires_rotation(uint32_t codepoint)
{
    auto orientation = get_orientation(codepoint);
    if(orientation == -1) return true; // unknown, default to R
    if(orientation == 0) return false; // U
    if(orientation == 1) return true; // R
    if(orientation == 2) return false; // Tu
    // Tr
    hb_codepoint_t glyph;
    hb_font_get_nominal_glyph(hbfont, codepoint, &glyph);
    return !hb_set_has(vert_set, glyph);
}
