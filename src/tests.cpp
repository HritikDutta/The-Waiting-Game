#include "core/logger.h"
#include "core/coroutines.h"
#include "containers/darray.h"
#include "containers/string.h"
#include "containers/bytes.h"
#include "containers/string_builder.h"
#include "containers/hash_table.h"

#include "platform/platform.h"

#include "fileio/fileio.h"
#include "serialization/json.h"
#include "serialization/binary/binary_conversion.h"
#include "serialization/binary/binary_lexer.h"

#include "math/math.h"
#include "engine/imgui.h"

void string_test()
{
    print("String Test:\n");

    String str  = make<String>("Hello there!", 13);
    String str1 = copy(get_substring(str));

    print("source string: %\ncopied string: %\n", str, str1);
    print("len: %, eql: %\n", str1.size, (str == str1));

    free(str);
    free(str1);

    print("test passed!\n");
}

void array_test()
{
    print("Dynamic Array Test:\n");

    DynamicArray<int> ints = make<DynamicArray<int>>();

    append(ints, 1);
    append(ints, 2);
    append(ints, 3);
    append(ints, 4);
    append(ints, 5);
    append(ints, 6);

    insert(ints, 1, 2020);
    print("before removing: %\n", ints);

    remove(ints, 0);
    print("after removing:  %\n", ints);

    free(ints);

    {   // Array of arrays
        DynamicArray<DynamicArray<int>> intss = make<DynamicArray<DynamicArray<int>>>();

        for (size_t i = 0; i < 8; i++)
        {
            append(intss, make<DynamicArray<int>>());
            append(intss[i], 0);
            append(intss[i], 1);
            append(intss[i], 2);
            append(intss[i], 3);
            append(intss[i], 6);
            append(intss[i], 7);
            append(intss[i], 8);

            if (i % 2 == 0)
            {
                append(intss[i], 8);
                append(intss[i], 9);
                append(intss[i], 10);
                append(intss[i], 11);
            }
        }

        print("array of arrays: %\n", intss);

        free_all(intss);
    }

    print("test passed!\n");
}

void builder_test()
{
    print("String Builder Test:\n");

    StringBuilder builder = make<StringBuilder>();

    append(builder, make<String>("Hello"));
    append(builder, make<String>(" "));
    append(builder, make<String>("There!"));

    String s = build_string(builder);
    print("result string:  \"%\"\n", s);

    {   // Change last word
        String& str = builder[2];
        free(str);
        str = make<String>("World!");
    }

    free(s);
    s = build_string(builder);
    print("changed string: \"%\"\n", s);

    free(s);
    free_all(builder);

    print("test passed!\n");
}

void hash_table_test()
{
    print("Hash Table Test:\n");

    HashTable<String, int> table = make<HashTable<String, int>>();

    put(table, make<String>("abse"), 5);
    put(table, make<String>("absds"), 7);
    put(table, make<String>("a23e"), 102);
    put(table, make<String>("12123e"), 52);
    put(table, make<String>("abs235e"), 24);
    
    {   // Remove
        String key = ref("abse");
        auto elem = find(table, key);

        if (elem)
        {
            free(elem.key());
            remove(elem);
            print("removed element with key: %\n", key);
        }
    }

    String key = ref("absds");
    auto elem = find(table, key);

    if (elem)
        print("found key: '%' with value: %\n", elem.key(), elem.value());
    else
        print("not found key: %\n", key);

    print("filled: %, capacity: %\n", table.filled, table.capacity);

    free(table);

    print("test passed!\n");
}

void json_test(char* in_path)
{
    String content = file_load_string(ref(in_path));

    Json::Document document = make<Json::Document>();
    bool success = Json::parse_string(content, document);

    if (success)
        print("Parsing succeeded! (tree size: %)\n", document.dependency_tree.size);

err_parsing:
    free(document);
    free(content);

    print("\ntest passed!\n");
}

void binary_test(char* path)
{
    Bytes bytes = file_load_bytes(ref(path));
    Binary::pretty_print(bytes);
    free(bytes);

    print("\ntest passed!\n");
}

void font_conversion_test()
{
    String json = file_load_string(ref("json_tests/font.json"));

    DynamicArray<Json::Token> tokens = {};
    bool success = Json::lex(json, tokens);

    if (!success)
    {
        print("Lexing failed!");
        goto err_lexing;
    }

    Json::Document document = make<Json::Document>();
    success = Json::parse_tokens(tokens, json, document);

    if (!success)
    {
        print("Parsing failed!");
        goto err_parsing;
    }

    Imgui::Font font = Imgui::font_load_from_json(document);
    Bytes bytes = Imgui::font_encode_to_bytes(font);
    Binary::pretty_print(bytes);
    file_write_bytes(ref("binary_tests/font_.bytes"), bytes);
    free(bytes);

err_parsing:
    free(document);

err_lexing:
    free(tokens);
    free(json);

    print("\ntest passed!\n");
}

Imgui::Font font_load_json_test()
{
    Imgui::Font font = {};

    String json = file_load_string(ref("json_tests/font.json"));

    DynamicArray<Json::Token> tokens = {};
    bool success = Json::lex(json, tokens);

    if (!success)
    {
        print("Lexing failed!");
        goto err_lexing;
    }

    Json::Document document = make<Json::Document>();
    success = Json::parse_tokens(tokens, json, document);

    if (!success)
    {
        print("Parsing failed!");
        goto err_parsing;
    }

    font = Imgui::font_load_from_json(document);
    
err_parsing:
    free(document);

err_lexing:
    free(tokens);
    free(json);

    print("\ntest passed!\n");

    return font;
}

Imgui::Font font_load_binary_test()
{
    Imgui::Font font = {};

    Bytes bytes = file_load_bytes(ref("binary_tests/font_.bytes"));
    font = Imgui::font_load_from_bytes(bytes);

    free(bytes);

    print("\ntest passed!\n");

    return font;
}

void compare_fonts(Imgui::Font& font_json, Imgui::Font& font_bin)
{
    gn_assert_with_message(font_json.size == font_bin.size, "Fonts don't have the same size! (json: %, binary: %)", font_json.size, font_bin.size);
    gn_assert_with_message(font_json.line_height == font_bin.line_height, "Fonts don't have the same line height! (json: %, binary: %)", font_json.line_height, font_bin.line_height);
    gn_assert_with_message(font_json.ascender == font_bin.ascender, "Fonts don't have the same ascender! (json: %, binary: %)", font_json.ascender, font_bin.ascender);
    gn_assert_with_message(font_json.descender == font_bin.descender, "Fonts don't have the same line descender! (json: %, binary: %)", font_json.descender, font_bin.descender);

    constexpr u32 num_glyphs = 127 - ' ';
    for (u32 i = 0; i < num_glyphs; i++)
    {
        const Imgui::Font::GlyphData& glyph_json = font_json.glyphs[i];
        const Imgui::Font::GlyphData& glyph_bin  = font_bin.glyphs[i];

        gn_assert_with_message(glyph_json.advance == glyph_bin.advance, "Font glyphs for '%' don't have the same advance! (json: %, binary: %)", (char)(i + ' '), glyph_json.advance, glyph_bin.advance);
        gn_assert_with_message(glyph_json.plane_bounds == glyph_bin.plane_bounds, "Font glyphs for '%' don't have the same plane bounds! (json: %, binary: %)", (char)(i + ' '), glyph_json.plane_bounds, glyph_bin.plane_bounds);
        gn_assert_with_message(glyph_json.atlas_bounds == glyph_bin.atlas_bounds, "Font glyphs for '%' don't have the same atlas bounds! (json: %, binary: %)", (char)(i + ' '), glyph_json.atlas_bounds, glyph_bin.atlas_bounds);
    }

    Imgui::Font::KerningTable& kerning_json = font_json.kerning_table;
    Imgui::Font::KerningTable& kerning_bin  = font_bin.kerning_table;
    gn_assert_with_message(kerning_json.filled == kerning_bin.filled, "Fonts don't have the same number of kerning data! (json: %, binary: %)", kerning_json.filled, kerning_bin.filled);

    u32 count = kerning_bin.filled;
    for (u32 i = 0; count > 0 && i < kerning_bin.capacity; i++)
    {
        if (kerning_bin.states[i] == Imgui::Font::KerningTable::State::ALIVE)
        {
            s32 key = kerning_bin.keys[i];

            const auto& elem_json = find(kerning_json, key);
            gn_assert_with_message(elem_json, "Element for % in binary not found in json!", key);

            gn_assert_with_message(elem_json.value() == kerning_bin.values[i], "Advance for key % is not the same for both fonts! (json: %, binary: %)", key, elem_json.value(), kerning_bin.values[i]);
        }
    }

    print("\ntest passed!\n");
}

enum struct NPCState
{
    IDLE,
    GREET,
    ASK_ABOUT_DAY,
    REACTION_YES,
    REACTION_NO,
    BYE,

    NUM_STATES
};

void state_machine_test()
{
    static NPCState state = NPCState::IDLE;
    static bool first = true;

    if (first)
    {
        print("RPG NPC Interaction Test\n");
        first = false;
    }

    switch (state)
    {
        case NPCState::IDLE:
        {
            state = NPCState::GREET;
        } break;

        case NPCState::GREET:
        {
            print("Bob: Heyy Hritik! Nice to see you here.\n");
            state = NPCState::ASK_ABOUT_DAY;
        } break;

        case NPCState::ASK_ABOUT_DAY:
        {
            print("Bob: So did your day go well? (y/n)\n");
            print("Answer: ");

            char option = getchar();
            getchar();
            state = (option == 'y') ? NPCState::REACTION_YES : NPCState::REACTION_NO;

            print("\n");
        } break;

        case NPCState::REACTION_YES:
        {
            print("Bob: Great news!\n");
            state = NPCState::BYE;
        } break;
        
        case NPCState::REACTION_NO:
        {
            print("Bob: Oh damn. Well tomorrow will be a better day then!\n");
            state = NPCState::BYE;
        } break;

        case NPCState::BYE:
        {
            print("Bob: Okay imma head off! See you soon...\n");
            print("Bob: Byeee\n");
            state = NPCState::IDLE;
        } break;
    }
}

// Simplified logic for how the coroutines work
void coroutine_idea(Coroutine& co)
{
    // Get variable from coroutine's stack
    char& option = *(char*)(&co.stack_frame[co.stack_ptr]);
    co.stack_ptr += sizeof(option);

    switch (co.line[0]) { default:                           // Coroutine Start

    // Greet
    print("Bob: Heyy Hritik! Nice to see you here.\n");
    co.line[0] = __LINE__; goto _co_end; case __LINE__:      // Coroutine Yield

    // Ask About Day
    print("Bob: So did your day go well? (y/n)\n");
    co.line[0] = __LINE__; goto _co_end; case __LINE__:      // Coroutine Yield

    // Reaction
    print("Answer: ");
    option = getchar();
    getchar();

    if (option == 'y')
        print("Bob: Great news!\n");
    else
        print("Bob: Oh damn. Well tomorrow will be a better day then!\n");
    
    co.line[0] = __LINE__; goto _co_end; case __LINE__:      // Coroutine Yield

    // Bye
    print("Bob: Okay imma head off! See you soon...\n");
    print("Bob: Byeee\n");

    case __LINE__: co.line[0] = __LINE__; } _co_end:;        // Coroutine End
}

void speech_coroutine(Coroutine& co, const String name, const String dialogue, f64 delay)
{
    u64& i = coroutine_stack_variable<u64>(co);

    coroutine_start(co);

    print("%: ", name);
    coroutine_wait_seconds(co, 0.3f);

    for (i = 0; i < dialogue.size; i++)
    {
        coroutine_wait_seconds(co, 0.1f);
        print("%", dialogue[i]);
    }

    coroutine_wait_seconds(co, delay);
    print("\n");

    coroutine_end(co);
}

void rpg_npc_conversation_coroutine(Coroutine& co, f64 delay, const bool allow_conversation)
{
    static const String name = ref("Bob");
    char& option = coroutine_stack_variable<char>(co);

    coroutine_start(co);
    
    // Wait for permission
    coroutine_wait_until(co, allow_conversation);

    // Greet
    coroutine_call(co, speech_coroutine(co, name, ref("Heyy Hritik! Nice to see you here."), delay));

    // Ask About Day
    coroutine_call(co, speech_coroutine(co, name, ref("So did your day go well? (y/n)"), delay));
    
    // Reaction
    print("Answer: ");
    option = getchar();
    getchar();
    coroutine_wait_seconds(co, delay);

    if (option == 'y')
        coroutine_call(co, speech_coroutine(co, name, ref("That's great news!"), delay));
    else
    {
        coroutine_call(co, speech_coroutine(co, name, ref("Oh damn..."), delay));
        coroutine_call(co, speech_coroutine(co, name, ref("Hopefully tomorrow will be a better day then!"), delay));
    }
    
    // Bye
    coroutine_call(co, speech_coroutine(co, name, ref("Okay imma head off now! See you soon..."), delay));
    coroutine_call(co, speech_coroutine(co, name, ref("Byeee"), delay));

    print("Coroutine ended!\n");

    coroutine_end(co);
}

bool conversation_start_coroutine(Coroutine& co, int seconds)
{
    int& timer_count = coroutine_stack_variable<int>(co);
    bool& allow_conversation = coroutine_stack_variable<bool>(co);

    coroutine_start(co);

    allow_conversation = false;

    for (timer_count = 0; timer_count < seconds; timer_count++)
    {
        coroutine_wait_seconds(co, 1.0f);
        print("\rTime elapsed: % seconds", timer_count + 1);
    }

    allow_conversation = true;
    print("\n");

    coroutine_end(co);

    return allow_conversation;
}

void coroutine_test()
{
    Coroutine c1 = {};
    Coroutine c2 = {};

    do
    {
        const bool allow_conversation = conversation_start_coroutine(c2, 4);
        rpg_npc_conversation_coroutine(c1, 0.5f, allow_conversation);
    } while (c1.running || c2.running);
}

int main(int argc, const char** argv)
{
    platform_init_clock();

    // string_test();
    // print("\n");
    
    // array_test();
    // print("\n");

    // builder_test();
    // print("\n");

    // hash_table_test();
    // print("\n");

    // char* in_path  = ((argc == 1) ? "json_tests/example_1.json" : argv[1]);

    // json_test(in_path);
    // print("\n");
    
    // binary_test(out_path);
    // print("\n");

    // font_conversion_test();

    // Imgui::Font font_json = font_load_json_test();
    // Imgui::Font font_bin  = font_load_binary_test();

    // compare_fonts(font_json, font_bin);

    coroutine_test();

    // free(font_json);
    // free(font_bin);

    return 0;
}